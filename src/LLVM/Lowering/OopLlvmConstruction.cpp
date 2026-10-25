#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::lowerValueConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C6 struct construction has no value layout");
        }
        if (node->argumentCount > node->children.size())
        {
            return failValue(error, "C6 struct construction has an invalid source-argument count");
        }
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);

        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "struct.new");
        if (slot == nullptr) return failValue(error, "C6 struct construction could not reserve storage");

        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                slot,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end())
                return failValue(error, "C6 struct constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(slot);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function, ctorArgs.data(),
                    static_cast<unsigned>(ctorArgs.size()), "", error) == nullptr && !error.empty())
            {
                return nullptr;
            }
        }

        // Field initializers synthesized for an implicit default constructor run against the slot,
        // exactly as they run against the freshly allocated object on the class path.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = slot;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return nullptr;
            }
        }
        constructionThisOverride_ = previousOverride;

        // BUG FOUND BY C7, FIXED HERE. This used to short-circuit -- `if
        // (isIndirectStructTypeKey(...)) return slot;` -- handing back the ADDRESS of the freshly
        // constructed struct for any aggregate that travels indirectly (anything not 1/2/4/8
        // bytes). But an expression of aggregate type hands over the LOADED carrier at this
        // level, never a pointer: a call returning one writes into an sret slot and then loads it
        // (`abi.sret.load`), and the TemporaryAddress case above already carries the same lesson
        // in writing. The mismatch was silent rather than fatal wherever the consumer stored the
        // result: `Big g = new Big(1.0f);` on a twenty-byte struct emitted
        // `store ptr %struct.new, ptr %local0.addr` -- the local ended up holding a POINTER in
        // its first eight bytes, and every field read afterwards read pointer bits as floats.
        // Wrong values, no diagnostic, no verifier complaint. One shape, one path.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "struct.new.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

LLVMValueRef OopLlvmImplementation::lowerParameterPack(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node == nullptr || node->immediate == 0 || node->alignment == 0 ||
            node->secondaryImmediate == 0)
        {
            return failValue(error, "T01-F02 parameter pack has incomplete layout data");
        }
        LLVMTypeRef spanType = llvmType(node->typeKey, error);
        LLVMTypeRef elementType = llvmType(node->elementTypeKey, error);
        if (spanType == nullptr || elementType == nullptr) return nullptr;

        const unsigned spanAlignment = structAlignmentFor(node->typeKey);
        LLVMValueRef spanSlot = entryBlockAlloca(spanType, spanAlignment, "params.span");
        if (spanSlot == nullptr)
            return failValue(error, "T01-F02 could not reserve parameter-pack span storage");

        LLVMValueRef memsetArgs[] = {
            spanSlot,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->secondaryImmediate, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");

        LLVMTypeRef ptrType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef elementStorage = LLVMConstNull(ptrType);
        if (!node->children.empty())
        {
            LLVMTypeRef arrayType = LLVMArrayType2(elementType, node->children.size());
            elementStorage = entryBlockAlloca(
                arrayType, static_cast<unsigned>(node->alignment), "params.elements");
            if (elementStorage == nullptr)
                return failValue(error, "T01-F02 could not reserve parameter-pack elements");

            for (std::size_t index = 0; index < node->children.size(); ++index)
            {
                LLVMValueRef value = argumentOverrides != nullptr
                    ? (*argumentOverrides)[index]
                    : lowerExpression(node->children[index].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef indices[] = {
                    LLVMConstInt(LLVMInt32TypeInContext(context_), 0, 0),
                    LLVMConstInt(LLVMInt64TypeInContext(context_), index, 0)};
                LLVMValueRef address = LLVMBuildGEP2(
                    builder_, arrayType, elementStorage, indices, 2, "params.element.addr");
                LLVMValueRef store = LLVMBuildStore(builder_, value, address);
                if (node->alignment > 1U)
                    LLVMSetAlignment(store, static_cast<unsigned>(node->alignment));
            }
        }

        LLVMValueRef pointerAddress = byteOffset(
            spanSlot, node->integerValue, "params.span.pointer.addr");
        LLVMValueRef pointerStore = LLVMBuildStore(builder_, elementStorage, pointerAddress);
        LLVMSetAlignment(pointerStore, static_cast<unsigned>(sizeof(void*)));
        LLVMValueRef countAddress = byteOffset(
            spanSlot, node->bitStorageSize, "params.span.count.addr");
        LLVMValueRef countStore = LLVMBuildStore(
            builder_,
            LLVMConstInt(LLVMInt32TypeInContext(context_), node->children.size(), 0),
            countAddress);
        LLVMSetAlignment(countStore, 4U);

        LLVMValueRef result = LLVMBuildLoad2(builder_, spanType, spanSlot, "params.span.value");
        if (spanAlignment > 1U) LLVMSetAlignment(result, spanAlignment);
        return result;
    }

bool OopLlvmImplementation::initializeAggregateSpread(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error)
{
                if (child->children.empty())
                    return fail(error, "T01-F05 aggregate spread has no source value");
                LLVMValueRef sourceSlot = nullptr;
                if (child->booleanValue)
                {
                    if (fieldValues != nullptr)
                    {
                        if (fieldIndex < fieldValues->size()) sourceSlot = (*fieldValues)[fieldIndex++];
                    }
                    else sourceSlot = lowerExpression(child->children.front().get(), error);
                    if (sourceSlot == nullptr) return false;
                }
                else
                {
                    LLVMValueRef sourceValue = nullptr;
                    if (fieldValues != nullptr)
                    {
                        if (fieldIndex < fieldValues->size()) sourceValue = (*fieldValues)[fieldIndex++];
                    }
                    else sourceValue = lowerExpression(child->children.front().get(), error);
                    if (sourceValue == nullptr) return false;
                    LLVMTypeRef sourceType = llvmType(child->typeKey, error);
                    if (sourceType == nullptr) return false;
                    sourceSlot = entryBlockAlloca(
                        sourceType, structAlignmentFor(child->typeKey), "aggregate.spread.source");
                    LLVMBuildStore(builder_, sourceValue, sourceSlot);
                }
                for (std::size_t index = 1; index < child->children.size(); ++index)
                {
                    const frontend::ProjectOopMirNode* descriptor = child->children[index].get();
                    if (descriptor == nullptr ||
                        descriptor->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore ||
                        !descriptor->children.empty() || descriptor->bitStorageSize == 0)
                    {
                        return fail(error, "T01-F05 aggregate spread has a malformed field descriptor");
                    }
                    LLVMValueRef destination = byteOffset(
                        slot, descriptor->immediate, "aggregate.spread.destination");
                    LLVMValueRef source = byteOffset(
                        sourceSlot, descriptor->secondaryImmediate, "aggregate.spread.source.field");
                    LLVMValueRef copyArgs[] = {
                        destination,
                        source,
                        LLVMConstInt(
                            LLVMInt64TypeInContext(context_), descriptor->bitStorageSize, 0),
                        LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
                    buildNoUnwindCall(memcpyFunctionType_, memcpyFunction_, copyArgs, 4, "");
                    if (child->booleanValue)
                    {
                        // T02-F06 move-spread transfers this field's bits into the destination.
                        // Empty the original slot immediately so the source aggregate's ordinary
                        // drop glue skips the moved field while continuing to destroy untouched
                        // siblings. Zero is the canonical empty state for every drop-bearing
                        // carrier in the 3.0 value model.
                        LLVMValueRef clearArgs[] = {
                            source,
                            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                            LLVMConstInt(
                                LLVMInt64TypeInContext(context_), descriptor->bitStorageSize, 0),
                            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
                        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, clearArgs, 4, "");
                    }
                }
        return true;
    }

bool OopLlvmImplementation::initializeAggregateField(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error)
{
            if (child == nullptr ||
                child->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore ||
                child->children.size() != 1)
            {
                return fail(error, "C7 aggregate initializer has a malformed element");
            }
            LLVMValueRef value = nullptr;
            if (fieldValues != nullptr)
            {
                if (fieldIndex < fieldValues->size()) value = (*fieldValues)[fieldIndex++];
            }
            else value = lowerExpression(child->children.front().get(), error);
            if (value == nullptr) return false;
            LLVMValueRef address = byteOffset(slot, child->immediate, "aggregate.element.addr");
            if (child->bitWidth.has_value())
            {
                LLVMTypeRef storageType = bitFieldStorageType(child, error);
                if (storageType == nullptr) return false;
                LLVMValueRef previous = LLVMBuildLoad2(
                    builder_, storageType, address, "aggregate.bitfield.storage.load");
                LLVMSetAlignment(previous, static_cast<unsigned>(child->alignment));
                if (child->volatileAccess) LLVMSetVolatile(previous, 1);
                LLVMValueRef merged = insertBitField(child, previous, value, error);
                if (merged == nullptr) return false;
                LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
                LLVMSetAlignment(store, static_cast<unsigned>(child->alignment));
                if (child->volatileAccess) LLVMSetVolatile(store, 1);
                return true;
            }
            LLVMValueRef store = LLVMBuildStore(builder_, value, address);
            // An element is written through its own typed address, so it needs its own natural
            // alignment -- the carrier's `[N x i8]` shape would otherwise imply align 1.
            const unsigned elementAlignment = structAlignmentFor(child->typeKey);
            if (elementAlignment > 1U) LLVMSetAlignment(store, elementAlignment);
        return true;
    }

bool OopLlvmImplementation::initializeAggregateIntoSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef slot,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues )
{
        if (node == nullptr ||
            node->kind != frontend::ProjectOopMirNodeKind::AggregateInitialize ||
            slot == nullptr || node->immediate == 0 || node->alignment == 0)
        {
            return fail(error, "C7 aggregate initializer has no destination or value layout");
        }
        std::size_t fieldIndex = 0;
        LLVMValueRef memsetArgs[] = {
            slot,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");
        for (const auto& child : node->children)
        {
            if (child != nullptr &&
                child->kind == frontend::ProjectOopMirNodeKind::AggregateSpreadStore)
            {
                if (!initializeAggregateSpread(child.get(), slot, fieldValues, fieldIndex, error))
                    return false;
                continue;
            }
            if (!initializeAggregateField(child.get(), slot, fieldValues, fieldIndex, error))
                return false;
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerAggregateInitializer(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues )
{
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C7 aggregate initializer has no value layout");
        }
        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "aggregate.init");
        if (slot == nullptr)
            return failValue(error, "C7 aggregate initializer could not reserve storage");
        if (!initializeAggregateIntoSlot(node, slot, error, fieldValues)) return nullptr;

        // Always the loaded carrier, never the slot pointer -- see the note in
        // lowerValueConstruction for the bug that short-circuit causes.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "aggregate.init.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

bool OopLlvmImplementation::collectConstructionArguments(
        const frontend::ProjectOopMirNode* node,
        std::size_t childOffset,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        // Source arguments are evaluated left-to-right before object allocation. Their SSA values
        // therefore dominate both the allocation-success branch and the constructor call. This
        // prevents argument side effects from disappearing merely because allocation returns null.
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(node->children[index + childOffset].get(), error);
            if (value == nullptr) return false;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index + childOffset]->typeKey, error);
            if (value == nullptr) return false;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return false;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);
        return true;
    }

LLVMValueRef OopLlvmImplementation::allocateConstructionStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMBasicBlockRef& endBlock,
        std::string& error)
{
        LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef object = nullptr;
        endBlock = nullptr;
        if (node->stackPromotedObject)
        {
            if (!node->localOrdinal.has_value())
                return failValue(error, "item 323 stack construction has no local identity");
            const auto storage = stackObjectStorage_.find(*node->localOrdinal);
            if (storage == stackObjectStorage_.end() || storage->second == nullptr)
                return failValue(error, "item 323 stack construction cannot find its frame storage");
            object = storage->second;
        }
        else
        {
            LLVMValueRef allocArgs[] = {size};
            object = buildNoUnwindCall(
                allocationFunctionType_, allocationFunction_, allocArgs, 1, "object");
            LLVMValueRef nullValue = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
            LLVMValueRef isNull = LLVMBuildICmp(
                builder_, LLVMIntEQ, object, nullValue, "alloc.failed");
            LLVMBasicBlockRef initBlock = appendBlock("object.init");
            endBlock = appendBlock("object.end");
            LLVMBuildCondBr(builder_, isNull, endBlock, initBlock);
            enterBlock(initBlock);
        }
        if (node->zeroInitialize)
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0);
            LLVMValueRef isVolatile = LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            LLVMValueRef args[] = {object, zero, size, isVolatile};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }
        if (!node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Macro A allocation is missing its target class ordinal");
        }
        LLVMValueRef descriptorValue = nullptr;
        const auto descriptorAccessor = descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
        if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
            runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
        {
            descriptorValue = buildNoUnwindCall(
                descriptorAccessor->second.functionType,
                descriptorAccessor->second.function,
                nullptr,
                0,
                "classdesc");
        }
        else
        {
            const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
            if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
        }
        if (descriptorValue == nullptr)
        {
            return failValue(error, "Macro A allocation cannot resolve its class descriptor");
        }
        LLVMBuildStore(builder_, descriptorValue, object);
        return object;
    }

bool OopLlvmImplementation::invokeConstructionInitializer(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end())
                return fail(error, "Stage 34 constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(object);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function,
                    ctorArgs.data(), static_cast<unsigned>(ctorArgs.size()), "", error) == nullptr &&
                !error.empty())
            {
                return false;
            }
        }

        // A construction node may carry synthesized instance-initializer work after its source
        // arguments.  These expressions conceptually execute against the just-allocated object;
        // they are not source arguments and therefore must not be evaluated before allocation.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = object;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            LLVMValueRef ignored = lowerExpression(node->children[index].get(), error);
            if (ignored == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return false;
            }
        }
        constructionThisOverride_ = previousOverride;
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->stackPromotedObject && !node->allocationFailureReturnsNull)
        {
            return failValue(
                error,
                "Stage 34 LLVM lowering requires the frozen Stage 29 allocation-failure -> null contract");
        }
        if (node->argumentCount > node->children.size())
            return failValue(error, "Stage 37 construction MIR has an invalid source-argument count");
        std::vector<LLVMValueRef> orderedArguments;
        if (!collectConstructionArguments(
                node, 0, argumentOverrides, orderedArguments, error)) return nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        LLVMValueRef object = allocateConstructionStorage(node, endBlock, error);
        if (object == nullptr) return nullptr;
        if (!invokeConstructionInitializer(node, object, orderedArguments, error)) return nullptr;
        if (!node->stackPromotedObject)
        {
            if (!terminated()) LLVMBuildBr(builder_, endBlock);
            enterBlock(endBlock);
        }
        return object;
    }

bool OopLlvmImplementation::initializePlacementStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        std::string& error)
{
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                storage,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (targetType.kind == frontend::NamedTypeDeclarationKind::Class)
        {
            LLVMValueRef descriptorValue = nullptr;
            const auto descriptorAccessor =
                descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
            if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
                runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
            {
                descriptorValue = buildNoUnwindCall(
                    descriptorAccessor->second.functionType,
                    descriptorAccessor->second.function,
                    nullptr,
                    0,
                    "placement.classdesc");
            }
            else
            {
                const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
                if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
            }
            if (descriptorValue == nullptr)
                return fail(error, "item 318 placement construction cannot resolve its class descriptor");
            const bool hasCustomAllocator =
                node->allocatorContextLocalOrdinal.has_value() &&
                node->allocatorDeallocateMemberOrdinal.has_value() &&
                node->allocatorTypeOrdinal.has_value();
            if (hasCustomAllocator)
            {
                const auto found = localStorage_.find(*node->allocatorContextLocalOrdinal);
                if (found == localStorage_.end() || found->second == nullptr)
                    return fail(error,
                        "NS304-T13-F07 allocator provenance has no context storage");
                LLVMValueRef context = LLVMBuildLoad2(
                    builder_, pointerType, found->second, "allocator.context.load");
                // EN: the object was allocated with Allocate(sizeof(T), alignof(T)), which is the
                //     layout size and alignment this node carries (C-08).
                // ES: el objeto se pidio con Allocate(sizeof(T), alignof(T)), que son el tamano y
                //     la alineacion del layout que lleva este nodo (C-08).
                LLVMValueRef thunk = allocatorDeallocateThunk(
                    *node->allocatorTypeOrdinal,
                    node->allocatorDeallocateCalleeName,
                    node->immediate,
                    node->alignment,
                    error);
                if (thunk == nullptr) return false;
                if (!installAllocatorDescriptorSidecar(
                        storage, descriptorValue, context, thunk, error))
                    return false;
            }
            else
            {
                // Raw placement borrows storage. Its ordinary descriptor keeps field 7 available
                // for object-only destruction; cleanup chooses that entry from storageOwnership.
                LLVMValueRef descriptorStore = LLVMBuildStore(builder_, descriptorValue, storage);
                if (node->alignment > 1U)
                    LLVMSetAlignment(descriptorStore, static_cast<unsigned>(node->alignment));
            }
        }
        return true;
    }

bool OopLlvmImplementation::invokePlacementConstructor(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        if (!node->calleeName.empty())
        {
            const auto constructor = functions_.find(node->calleeName);
            if (constructor == functions_.end())
                return fail(error, "item 318 placement constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> constructorArguments;
            constructorArguments.reserve(1 + orderedArguments.size());
            constructorArguments.push_back(storage);
            constructorArguments.insert(
                constructorArguments.end(), orderedArguments.begin(), orderedArguments.end());
            LLVMBasicBlockRef allocatorFailureCleanup = nullptr;
            if (targetType.kind == frontend::NamedTypeDeclarationKind::Class &&
                node->allocatorContextLocalOrdinal.has_value() &&
                node->allocatorDeallocateMemberOrdinal.has_value() &&
                node->allocatorTypeOrdinal.has_value())
            {
                LLVMBasicBlockRef outerUnwind = buildCurrentUnwindDestination(error);
                if (!error.empty()) return false;
                allocatorFailureCleanup = emitAllocatorConstructionFailureCleanup(
                    storage, outerUnwind, error);
                if (allocatorFailureCleanup == nullptr) return false;
                LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
            }
            if (allocatorFailureCleanup != nullptr)
            {
                LLVMBasicBlockRef normalBlock = appendBlock("placement.ctor.cont");
                LLVMValueRef invoke = buildInvokeInCurrentFunclet(
                    constructor->second.functionType,
                    constructor->second.function,
                    constructorArguments.data(),
                    static_cast<unsigned>(constructorArguments.size()),
                    normalBlock,
                    allocatorFailureCleanup,
                    "");
                if (invoke == nullptr)
                    return fail(error,
                        "NS304-T13-F07 failed to emit allocator constructor invoke");
                enterBlock(normalBlock);
            }
            else if (buildPotentiallyThrowingCall(
                         constructor->second.functionType,
                         constructor->second.function,
                         constructorArguments.data(),
                         static_cast<unsigned>(constructorArguments.size()),
                         "",
                         error) == nullptr && !error.empty())
            {
                return false;
            }
        }
        return true;
    }

bool OopLlvmImplementation::runPlacementInitializers(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
{
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = storage;
        for (std::size_t index = node->argumentCount + 1; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return false;
            }
        }
        constructionThisOverride_ = previousOverride;
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerPlacementConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef storageOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->children.empty() || node->children.front() == nullptr)
            return failValue(error, "item 318 placement construction requires destination storage");
        if (node->immediate == 0 || node->alignment == 0)
            return failValue(error, "item 318 placement construction has no target layout");
        if (node->argumentCount + 1 > node->children.size())
            return failValue(error, "item 318 placement construction has an invalid argument count");
        // The destination is evaluated exactly once and before every constructor argument. Its
        // semantic type is exact T*, so no conversion or hidden temporary can substitute another
        // location after this point.
        LLVMValueRef storage = storageOverride != nullptr
            ? storageOverride : lowerExpression(node->children.front().get(), error);
        if (storage == nullptr) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        if (!collectConstructionArguments(
                node, 1, argumentOverrides, orderedArguments, error)) return nullptr;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef notNull = LLVMBuildICmp(
            builder_, LLVMIntNE, storage, LLVMConstNull(pointerType), "placement.notnull");
        if (!emitGuard(notNull, LlvmTrapReason::NullReference, "placement.storage", error))
            return nullptr;
        if (!node->targetTypeOrdinal.has_value() ||
            *node->targetTypeOrdinal >= nameLookup_.namedTypes().size())
            return failValue(error, "item 318 placement construction has no target type ordinal");
        const frontend::NamedTypeDeclaration& targetType =
            nameLookup_.namedTypes()[*node->targetTypeOrdinal];
        if (!initializePlacementStorage(node, storage, targetType, error)) return nullptr;
        if (!invokePlacementConstructor(
                node, storage, targetType, orderedArguments, error)) return nullptr;
        if (!runPlacementInitializers(node, storage, error)) return nullptr;
        // NS304-T13-F06: a trailing construction exposes the header struct BY VALUE; load it from
        // the allocated pointer so the caller's value slot receives the initialized header.
        if (targetType.kind == frontend::NamedTypeDeclarationKind::Struct &&
            !node->typeKey.ends_with("*"))
        {
            LLVMTypeRef valueType = llvmType(node->typeKey, error);
            if (valueType == nullptr)
                return failValue(error, "trailing construction has no value layout");
            return LLVMBuildLoad2(builder_, valueType, storage, "trailing.header");
        }
        return storage;
    }

LLVMValueRef OopLlvmImplementation::lowerObjectDestruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1)
            return failValue(error, "item 319 object destruction requires one operand");

        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;

        // Structs currently have no destructor or owned fields. Evaluating the pointer ends the
        // language lifetime contract and requires no machine operation; importantly, it still
        // never becomes a call to the allocator's free hook.
        if (!node->dynamicDestruction)
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
            return failValue(error, "item 319 object destruction has no descriptor ABI");

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "destroy.object.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("destroy.object");
        LLVMBasicBlockRef doneBlock = appendBlock("destroy.object.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        enterBlock(destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(
            builder_, ptr, object, "destroy.object.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 7, "destroy.object.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "destroy.object.entry");
        LLVMValueRef arguments[] = {object};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerDeallocation(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef loweredOperandOverride )
{
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 35 deallocation MIR requires one operand");
        }
        LLVMValueRef loweredOperand = loweredOperandOverride != nullptr
            ? loweredOperandOverride
            : lowerExpression(node->children.front().get(), error);
        if (loweredOperand == nullptr)
        {
            return nullptr;
        }

        // Stage 52: `delete <value-typed expression>;` (e.g. an int-valued generic collection
        // element) was already evaluated above for its side effects (array-index bounds checks
        // in particular), but its result is not a heap pointer at all -- there is nothing to
        // free. Discard it rather than passing a non-pointer bit pattern to the runtime
        // allocator, which would misbehave or fail LLVM verification.
        if (node->valueTypeNoOpDeletion)
        {
            // EN: Freeze 3.0 C-12: an addressed struct element is dropped in place, then zeroed.
            // ES: Congelamiento 3.0 C-12: un elemento struct con direccion se destruye y queda en cero.
            if (node->booleanValue && !emitValueElementDelete(loweredOperand, node->elementTypeKey, error))
                return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        LLVMValueRef pointer = loweredOperand;
        if (node->booleanValue)
        {
            LLVMTypeRef storedType = llvmType(node->elementTypeKey, error);
            if (storedType == nullptr) return nullptr;
            pointer = LLVMBuildLoad2(
                builder_, storedType, loweredOperand, "delete.element.load");
            // Clear before calling user destruction.  If a destructor transfers control through
            // EH, every cleanup path already observes an empty owner slot.
            LLVMBuildStore(builder_, LLVMConstNull(storedType), loweredOperand);
        }

        if (node->children.front() != nullptr && isDelegateReferenceType(nameLookup_, node->children.front()->typeKey))
        {
            if (!emitDelegateDestroy(pointer, "delete.delegate", error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        const std::string_view deletionType = node->booleanValue && !node->elementTypeKey.empty()
            ? std::string_view(node->elementTypeKey)
            : std::string_view(node->children.front()->typeKey);
        if (frontend::ProjectTypeConversion::isNativeArray(deletionType))
        {
            if (!emitNativeArrayDestroy(
                    pointer,
                    deletionType,
                    "delete.array",
                    node->storageOwnership == sema::OwnershipKind::Owned,
                    error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (!node->dynamicDestruction)
        {
            LLVMValueRef arguments[] = {pointer};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return failValue(error, "Stage 35 dynamic delete has no class-descriptor destruction ABI");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, pointer, LLVMConstNull(ptr), "delete.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("delete.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("delete.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        enterBlock(destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, pointer, "delete.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5, "delete.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "delete.entry");
        LLVMValueRef arguments[] = {pointer};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated())
        {
            LLVMBuildBr(builder_, doneBlock);
        }

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

}
