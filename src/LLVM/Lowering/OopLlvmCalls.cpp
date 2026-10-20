#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::callArgumentOrder(
        const frontend::ProjectOopMirNode* node,
        std::size_t sourceCount,
        std::vector<std::size_t>& parameterToSource,
        std::string& error)
{
        parameterToSource.resize(sourceCount);
        if (node->argumentParameterOrdinals.empty())
        {
            for (std::size_t index = 0; index < sourceCount; ++index) parameterToSource[index] = index;
            return true;
        }
        if (node->argumentParameterOrdinals.size() != sourceCount)
        {
            return fail(error, "named call argument map does not match the evaluated argument count");
        }
        std::vector<bool> assigned(sourceCount, false);
        for (std::size_t source = 0; source < sourceCount; ++source)
        {
            const std::size_t parameter = node->argumentParameterOrdinals[source];
            if (parameter >= sourceCount || assigned[parameter])
            {
                return fail(error, "named call argument map is not a parameter permutation");
            }
            assigned[parameter] = true;
            parameterToSource[parameter] = source;
        }
        return true;
    }

bool OopLlvmImplementation::applyMustTail(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef call,
        std::string& error)
{
        if (!node->mustTailCall) return true;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
            return fail(error, "T08-F06 must_tail cannot use the current indirect aggregate return ABI");
        if (call == nullptr || LLVMIsACallInst(call) == nullptr)
            return fail(error, "T08-F06 must_tail lowered to an invoke or non-call instruction");
        LLVMSetTailCallKind(call, LLVMTailCallKindMustTail);
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerNullConditionalCall(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.empty())
            return failValue(error, "null-conditional call is missing its receiver");
        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr) return nullptr;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcall.hasreceiver");
        LLVMBasicBlockRef nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef callBlock = appendBlock("nullcall.invoke");
        LLVMBasicBlockRef endBlock = appendBlock("nullcall.end");
        LLVMBuildCondBr(builder_, hasReceiver, callBlock, endBlock);

        LLVMPositionBuilderAtEnd(builder_, callBlock);
        LLVMValueRef value = nullptr;
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::DirectInstanceCall:
                value = lowerCall(node, error, receiver);
                break;
            case frontend::ProjectOopMirNodeKind::VirtualInstanceCall:
                value = lowerVirtualCall(node, error, receiver);
                break;
            case frontend::ProjectOopMirNodeKind::InterfaceInstanceCall:
                value = lowerInterfaceCall(node, error, receiver);
                break;
            default:
                return failValue(error, "null-conditional dispatch requires an instance call");
        }
        if (value == nullptr) return nullptr;
        LLVMBasicBlockRef callExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        // Semantic analysis admits only physical nullable carriers here; every such carrier uses
        // the opaque-pointer ABI.  Do not depend on LLVM's type-introspection C API merely to
        // repeat that already authoritative decision.
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcall");
        LLVMValueRef values[] = {LLVMConstNull(resultType), value};
        LLVMBasicBlockRef blocks[] = {nullBlock, callExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerRangeSlice(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 3U || node->calleeName.empty() ||
            node->secondaryCalleeName.empty())
        {
            return failValue(error, "T01-F04 range slice requires receiver, two endpoints and Length/Slice targets");
        }
        const auto lengthTarget = functions_.find(node->secondaryCalleeName);
        if (lengthTarget == functions_.end())
        {
            return failValue(error, "T01-F04 Length target was not declared: " + node->secondaryCalleeName);
        }

        LLVMValueRef receiver = lowerExpression(node->children[0].get(), error);
        if (receiver == nullptr) return nullptr;
        const bool structReceiver = isStructValue(node->children[0]->typeKey);
        LLVMValueRef checked = receiver;
        if (!structReceiver)
        {
            checked = checkedReceiver(receiver, error);
            if (checked == nullptr) return nullptr;
        }

        LLVMValueRef lengthArguments[] = {checked};
        LLVMValueRef length = buildPotentiallyThrowingCall(
            lengthTarget->second.functionType,
            lengthTarget->second.function,
            lengthArguments,
            1U,
            "range.length",
            error);
        if (length == nullptr) return nullptr;

        LLVMValueRef writtenStart = lowerExpression(node->children[1].get(), error);
        LLVMValueRef writtenEnd = lowerExpression(node->children[2].get(), error);
        if (writtenStart == nullptr || writtenEnd == nullptr) return nullptr;

        constexpr std::uint64_t hasStartBit = 1U;
        constexpr std::uint64_t startFromEndBit = 2U;
        constexpr std::uint64_t hasEndBit = 4U;
        constexpr std::uint64_t endFromEndBit = 8U;
        constexpr std::uint64_t boundsProvenBit = 16U;
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef zero = LLVMConstInt(i32, 0U, 0);
        LLVMValueRef start = (node->integerValue & hasStartBit) != 0U
            ? writtenStart : zero;
        if ((node->integerValue & startFromEndBit) != 0U)
            start = LLVMBuildSub(builder_, length, start, "range.start.fromend");
        LLVMValueRef end = (node->integerValue & hasEndBit) != 0U
            ? writtenEnd : length;
        if ((node->integerValue & endFromEndBit) != 0U)
            end = LLVMBuildSub(builder_, length, end, "range.end.fromend");

        if ((node->integerValue & boundsProvenBit) == 0U)
        {
            LLVMValueRef lengthValid = LLVMBuildICmp(
                builder_, LLVMIntSGE, length, zero, "range.length.nonnegative");
            LLVMValueRef startValid = LLVMBuildICmp(
                builder_, LLVMIntSGE, start, zero, "range.start.nonnegative");
            LLVMValueRef ordered = LLVMBuildICmp(
                builder_, LLVMIntSLE, start, end, "range.ordered");
            LLVMValueRef endValid = LLVMBuildICmp(
                builder_, LLVMIntSLE, end, length, "range.end.inbounds");
            LLVMValueRef valid = LLVMBuildAnd(
                builder_, lengthValid, startValid, "range.valid.length-start");
            valid = LLVMBuildAnd(builder_, valid, ordered, "range.valid.order");
            valid = LLVMBuildAnd(builder_, valid, endValid, "range.valid.end");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "range.slice", error))
                return nullptr;
        }

        LLVMValueRef count = LLVMBuildSub(builder_, end, start, "range.count");
        const std::vector<LLVMValueRef> sliceArguments = {start, count};
        return lowerCall(node, error, checked, &sliceArguments);
    }

LLVMValueRef OopLlvmImplementation::lowerCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Macro A call target was not declared: " + node->calleeName);
        }
        if (node->children.empty())
        {
            return failValue(error, "Macro A instance call is missing its explicit receiver operand");
        }

        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride
            : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        if (terminated()) return receiver;
        const bool structReceiver = node->children.front() != nullptr &&
            isStructValue(node->children.front()->typeKey);
        if (!structReceiver)
        {
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr)
            {
                return nullptr;
            }
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        // Win64 aggregate ABI: an indirect struct return travels through an sret slot placed
        // right after the receiver; indirect struct arguments travel as pointers to per-call
        // copies. Mirrors the signature built in declareFunctions -- and clang's own output.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size() - 1);
        LLVMValueRef assignedValue = nullptr;
        LLVMValueRef terminatedValue = nullptr;
        if (!collectCallArguments(
                node, argumentOverrides, sourceArguments, assignedValue, terminatedValue, error))
        {
            return nullptr;
        }
        if (terminatedValue != nullptr) return terminatedValue;
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "call", error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

bool OopLlvmImplementation::collectCallArguments(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& sourceArguments,
        LLVMValueRef& assignedValue,
        LLVMValueRef& terminatedValue,
        std::string& error)
{
        if (argumentOverrides != nullptr &&
            argumentOverrides->size() != node->children.size() - 1U)
        {
            failValue(error, "direct call argument override count does not match MIR operands");
            return false;
        }
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1U]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return false;
            if (terminated())
            {
                terminatedValue = value;
                return true;
            }
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return false;
            sourceArguments.push_back(value);
        }
        return true;
    }

bool OopLlvmImplementation::ensureTypeInitialized(std::size_t typeOrdinal, std::string& error)
{
        const auto found = staticInitFunctions_.find(typeOrdinal);
        if (found == staticInitFunctions_.end())
        {
            return true;
        }
        if (currentFunction_ == nullptr)
        {
            return fail(error, "Stage 37 cannot emit a type-initialization guard without an active function");
        }
        if (buildPotentiallyThrowingCall(
                found->second.functionType, found->second.function,
                nullptr, 0, "", error) == nullptr && !error.empty())
        {
            return false;
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::staticFieldAddress(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || !node->memberOrdinal.has_value())
        {
            return failValue(error, "Stage 37 static field MIR is missing its member ordinal");
        }
        const auto globalFound = staticFieldGlobals_.find(*node->memberOrdinal);
        const auto infoFound = staticFieldInfo_.find(*node->memberOrdinal);
        if (infoFound == staticFieldInfo_.end() || infoFound->second == nullptr)
        {
            return failValue(error, "Stage 37 static field has no declared MIR information");
        }
        const frontend::ProjectOopMirStaticField& field = *infoFound->second;
        if (field.isImported && !field.isConst)
        {
            const auto accessorFound = staticFieldAccessorFunctions_.find(*node->memberOrdinal);
            if (accessorFound == staticFieldAccessorFunctions_.end())
                return failValue(error, "Stage 37 imported static field has no declared accessor");
            LLVMValueRef address = buildPotentiallyThrowingCall(
                accessorFound->second.functionType, accessorFound->second.function,
                nullptr, 0, "static.address", error);
            if (address == nullptr && !error.empty()) return nullptr;
            return address;
        }
        if (globalFound == staticFieldGlobals_.end())
            return failValue(error, "Stage 37 static field has no declared LLVM global");
        if (!field.isConst && !ensureTypeInitialized(field.declaringTypeOrdinal, error))
        {
            return nullptr;
        }
        return globalFound->second;
    }

LLVMValueRef OopLlvmImplementation::lowerDirectClosureCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.empty() || node->children.size() != node->argumentCount + 1)
            return failValue(error, "generic closure call has invalid MIR children");
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
            return failValue(error, "generic closure target was not declared: " + node->calleeName);

        LLVMValueRef delegateCell = lowerExpression(node->children.front().get(), error);
        if (delegateCell == nullptr) return nullptr;
        delegateCell = checkedReceiver(delegateCell, error);
        if (delegateCell == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef receiver = LLVMBuildLoad2(
            builder_, ptr, byteOffset(delegateCell, 8, "generic.closure.receiver.addr"),
            "generic.closure.receiver");
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr) return nullptr;

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount + 2);
        arguments.push_back(receiver);
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(
                carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            if (terminated()) return value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            arguments.push_back(value);
        }
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr
                ? "" : "generic.closure.call",
            error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::lowerStaticCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Stage 37 static call target was not declared: " + node->calleeName);
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        // Win64 aggregate ABI -- see lowerCall; for a static callee the sret slot leads.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size());
        LLVMValueRef assignedValue = nullptr;
        if (argumentOverrides != nullptr &&
            argumentOverrides->size() != node->children.size())
        {
            return failValue(error, "static call argument override count does not match MIR operands");
        }
        for (std::size_t index = 0; index < node->children.size(); ++index)
        {
            const auto& child = node->children[index];
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(child.get(), error);
            if (value == nullptr)
            {
                return nullptr;
            }
            if (terminated()) return value;
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            value = wrapIndirectArgument(value, child->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "static.call", error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                0U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "static setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::loadVirtualTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error)
{
        if (!node->virtualSlot.has_value())
            return failValue(error, "T08-F05 virtual target is missing its vtable slot");
        if (descriptorType_ == nullptr)
            return failValue(error, "T08-F05 virtual target has no class-descriptor LLVM type");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "class.descriptor");
        LLVMValueRef vtableAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 1, "vtable.addr");
        LLVMValueRef vtable = LLVMBuildLoad2(builder_, ptr, vtableAddress, "vtable");
        LLVMValueRef slotIndex = LLVMConstInt(
            LLVMInt64TypeInContext(context_), *node->virtualSlot, 0);
        LLVMValueRef indices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, vtable, indices, 1, "vslot.addr");
        return LLVMBuildLoad2(builder_, ptr, slotAddress, "vtarget");
    }

bool OopLlvmImplementation::prepareDynamicCallArguments(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& arguments,
        std::vector<LLVMTypeRef>& parameterTypes,
        LLVMValueRef& sretSlot,
        LLVMValueRef& assignedValue,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        arguments.reserve(node->children.size() + 1);
        parameterTypes.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        parameterTypes.push_back(ptr);
        sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return false;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
            parameterTypes.push_back(ptr);
        }
        std::vector<LLVMValueRef> sourceArguments;
        std::vector<LLVMTypeRef> sourceParameterTypes;
        sourceArguments.reserve(node->children.size() - 1);
        sourceParameterTypes.reserve(node->children.size() - 1);
        assignedValue = nullptr;
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1]
                : lowerExpression(node->children[index].get(), error);
            LLVMTypeRef parameterType = abiParameterType(node->children[index]->typeKey, error);
            if (value == nullptr || parameterType == nullptr)
            {
                return false;
            }
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return false;
            sourceArguments.push_back(value);
            sourceParameterTypes.push_back(parameterType);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return false;
        for (const std::size_t source : parameterToSource)
        {
            arguments.push_back(sourceArguments[source]);
            parameterTypes.push_back(sourceParameterTypes[source]);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerVirtualCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->virtualSlot.has_value())
            return failValue(error, "Macro A virtual call is missing its vtable slot");
        if (node->children.empty())
            return failValue(error, "Macro A virtual call is missing its receiver operand");
        if (descriptorType_ == nullptr)
            return failValue(error, "Macro A virtual call has no class-descriptor LLVM type");
        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr) return nullptr;
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef target = loadVirtualTarget(node, receiver, error);
        if (target == nullptr) return nullptr;
        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        // Win64 aggregate ABI -- the reconstructed callee type must match what
        // declareFunctions gave every override: sret slot after the receiver, pointer-typed
        // indirect struct parameters.
        LLVMValueRef sretSlot = nullptr;
        LLVMValueRef assignedValue = nullptr;
        if (!prepareDynamicCallArguments(
                node, receiver, argumentOverrides, arguments, parameterTypes,
                sretSlot, assignedValue, error)) return nullptr;
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef virtualFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            virtualFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "vcall", error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "virtual setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::loadInterfaceTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error)
{
        if (!node->interfaceSlot.has_value() || !node->targetTypeOrdinal.has_value())
            return failValue(error, "T08-F05 interface target is missing identity or method slot");
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
            return failValue(error, "T08-F05 interface target has no interface-map LLVM ABI types");

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "interface.descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "interface.map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "interface.map");

        LLVMBasicBlockRef mapBlock = appendBlock("interface.map.ok");
        LLVMBasicBlockRef searchBlock = appendBlock("interface.search");
        LLVMBasicBlockRef probeBlock = appendBlock("interface.probe");
        LLVMBasicBlockRef nextBlock = appendBlock("interface.next");
        LLVMBasicBlockRef foundBlock = appendBlock("interface.found");
        LLVMBasicBlockRef missingBlock = appendBlock("interface.missing");
        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "interface.map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "interface.map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "interface.map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "interface.map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "interface.map.entries");
        LLVMBuildBr(builder_, searchBlock);

        LLVMPositionBuilderAtEnd(builder_, searchBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "interface.map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {mapBlock};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, count, "interface.map.inrange");
        LLVMBuildCondBr(builder_, inRange, probeBlock, missingBlock);

        LLVMPositionBuilderAtEnd(builder_, probeBlock);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "interface.entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "interface.entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "interface.entry.id");
        LLVMValueRef wantedId = LLVMConstInt(i64, node->immediate, 0);
        LLVMValueRef matches = LLVMBuildICmp(builder_, LLVMIntEQ, id, wantedId, "interface.entry.matches");
        LLVMBuildCondBr(builder_, matches, foundBlock, nextBlock);

        LLVMPositionBuilderAtEnd(builder_, nextBlock);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "interface.map.next");
        LLVMBuildBr(builder_, searchBlock);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {nextBlock};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, missingBlock);
        emitInterfaceDispatchTrap();

        LLVMPositionBuilderAtEnd(builder_, foundBlock);
        return loadInterfaceMethodSlot(node, entry, ptr, i64);
    }

void OopLlvmImplementation::emitInterfaceDispatchTrap()
{
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InterfaceDispatchFailure), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
    }

LLVMValueRef OopLlvmImplementation::loadInterfaceMethodSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef entry,
        LLVMTypeRef ptr,
        LLVMTypeRef i64)
{
        LLVMValueRef tableAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 1, "interface.entry.table.addr");
        LLVMValueRef methodTable = LLVMBuildLoad2(builder_, ptr, tableAddress, "interface.method.table");
        LLVMValueRef slotIndex = LLVMConstInt(i64, *node->interfaceSlot, 0);
        LLVMValueRef slotIndices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, methodTable, slotIndices, 1, "interface.slot.addr");
        return LLVMBuildLoad2(builder_, ptr, slotAddress, "interface.target");
    }

LLVMValueRef OopLlvmImplementation::lowerInterfaceCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->interfaceSlot.has_value() || !node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Stage 36 interface call is missing interface identity or method slot");
        }
        if (node->children.empty())
        {
            return failValue(error, "Stage 36 interface call is missing its receiver operand");
        }
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
        {
            return failValue(error, "Stage 36 interface call has no interface-map LLVM ABI types");
        }

        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride
            : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr)
        {
            return nullptr;
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef target = loadInterfaceTarget(node, receiver, error);
        if (target == nullptr) return nullptr;

        // Win64 aggregate ABI -- same reconstruction rule as lowerVirtualCall: the callee type
        // must match what declareFunctions gave every implementation.
        LLVMValueRef sretSlot = nullptr;
        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        LLVMValueRef assignedValue = nullptr;
        if (!prepareDynamicCallArguments(
                node, receiver, argumentOverrides, arguments, parameterTypes,
                sretSlot, assignedValue, error)) return nullptr;
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef interfaceFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        LLVMValueRef interfaceCall = buildPotentiallyThrowingCall(
            interfaceFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "icall", error);
        if (interfaceCall == nullptr) return nullptr;
        if (!applyMustTail(node, interfaceCall, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(interfaceCall, attributeIndex, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "interface setter assignment call lost its converted value");
            return assignedValue;
        }
        return interfaceCall;
    }

}
