#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::lowerLeafExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::ThisValue:
                return constructionThisOverride_ != nullptr ? constructionThisOverride_ : thisValue_;
            case frontend::ProjectOopMirNodeKind::ParameterValue:
                return lowerParameterValue(node, error);
            case frontend::ProjectOopMirNodeKind::LocalValue:
            {
                if (!node->localOrdinal.has_value()) return failValue(error, "local value missing ordinal");
                LLVMValueRef address = localStorage_[*node->localOrdinal];
                LLVMTypeRef type = llvmType(node->typeKey, error);
                if (type == nullptr || address == nullptr) return nullptr;
                LLVMValueRef load = LLVMBuildLoad2(builder_, type, address, "local.load");
                if (node->volatileAccess) LLVMSetVolatile(load, 1);
                return load;
            }
            case frontend::ProjectOopMirNodeKind::IntegerConstant:
            {
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr
                    ? LLVMConstInt(type, node->integerValue, isSignedInteger(node->typeKey) ? 1 : 0)
                    : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::FloatingConstant:
            {
                LLVMTypeRef type = llvmType(node->typeKey, error);
                if (type == nullptr) return nullptr;
                if (node->typeKey == "builtin:decimal")
                {
                    // 128-bit decimal: low 96 bits = coefficient, bits 96..101 = scale,
                    // bit 127 = sign. The C API wrapper exposes no constant shift/or, so the
                    // value is assembled with builder instructions.
                    std::uint64_t coefficient = 0;
                    int scale = 0;
                    if (!decimalCoefficientAndScale(node->floatingValue, coefficient, scale))
                        return failValue(error, "decimal literal coefficient exceeds 64 bits in Stage 34");
                    LLVMValueRef lo = LLVMConstInt(type, coefficient, 0);
                    LLVMValueRef scaleValue = LLVMConstInt(
                        type, static_cast<std::uint64_t>(scale), 0);
                    LLVMValueRef scaleShifted = LLVMBuildShl(
                        builder_, scaleValue,
                        LLVMConstInt(LLVMInt32TypeInContext(context_), 96, 0), "decimal.scale");
                    LLVMValueRef combined = LLVMBuildOr(builder_, lo, scaleShifted, "decimal.value");
                    if (node->floatingValue < 0.0)
                    {
                        LLVMValueRef sign = LLVMBuildShl(
                            builder_, LLVMConstInt(type, 1, 0),
                            LLVMConstInt(LLVMInt32TypeInContext(context_), 127, 0), "decimal.sign");
                        combined = LLVMBuildOr(builder_, combined, sign, "decimal.signed");
                    }
                    return combined;
                }
                return LLVMConstReal(type, node->floatingValue);
            }
            case frontend::ProjectOopMirNodeKind::BooleanConstant:
                return LLVMConstInt(LLVMInt1TypeInContext(context_), node->booleanValue ? 1U : 0U, 0);
            case frontend::ProjectOopMirNodeKind::NullConstant:
                return LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
            case frontend::ProjectOopMirNodeKind::DefaultValue:
            {
                // Stage 52: default(T) -- LLVMConstNull is uniformly correct for every closed T
                // here: an integer/float/bool/char primitive type gives numeric zero, a pointer
                // type (class, string, delegate) gives a null pointer, and a struct (value) type
                // gives an all-zero-fields aggregate -- exactly "the value every field of T would
                // have if never assigned", with no per-primitive special-casing needed.
                LLVMTypeRef type = llvmType(node->typeKey, error);
                return type != nullptr ? LLVMConstNull(type) : nullptr;
            }
            case frontend::ProjectOopMirNodeKind::StringLiteral:
                return lowerStringLiteral(node, error);
            case frontend::ProjectOopMirNodeKind::Utf8StringLiteral:
                return lowerUtf8StringLiteral(node, error);
            case frontend::ProjectOopMirNodeKind::EnumName:
                return lowerEnumName(node, error);
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerParameterValue(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (!node->parameterOrdinal.has_value()) return failValue(error, "parameter value missing ordinal");
        // NS304-T06-F04: a $moveNext/$poll captures the original callable's parameters into
        // the frame (stored by the constructor). Read the captured parameter from the frame
        // slot instead of the (now-shifted) parameter storage.
        if (currentMirFunction_ != nullptr &&
            !currentMirFunction_->asyncFrameParameters.empty())
        {
            const auto* indexedParameter = mirModel_.asyncFrameParameter(
                *currentMirFunction_, *node->parameterOrdinal, node->typeKey);
            if (indexedParameter != nullptr)
            {
                const auto& [paramOrdinal, offset, typeKey] = *indexedParameter;
                // NS304-T07-F06: match both the ordinal AND the physical type so the poll's
                // own `self` (ordinal 0, future:<T>*) never aliases a captured ref parameter
                // (ordinal 0, T*).
                if (paramOrdinal == *node->parameterOrdinal && node->typeKey == typeKey)
                {
                    LLVMValueRef self = LLVMBuildLoad2(
                        builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                        "param.self");
                    // NS304-T17-F09: the async-generator poll's `self` is the demand future.
                    // Its frame (offset 8) is the ITERATOR, and the iterator's frame (offset 8)
                    // is the generator frame that holds the captured parameters — one extra
                    // dereference relative to $moveNext/$drop, whose `self` IS the iterator.
                    LLVMValueRef frame = nullptr;
                    if (currentMirFunction_->mangledName.ends_with("$moveNext$poll"))
                    {
                        LLVMValueRef iterator = LLVMBuildLoad2(
                            builder_, LLVMPointerTypeInContext(context_, 0),
                            byteOffset(self, 8, "param.iter.addr"), "param.iter");
                        frame = LLVMBuildLoad2(
                            builder_, LLVMPointerTypeInContext(context_, 0),
                            byteOffset(iterator, 8, "param.frame.addr"), "param.frame");
                    }
                    else
                    {
                        frame = LLVMBuildLoad2(
                            builder_, LLVMPointerTypeInContext(context_, 0),
                            byteOffset(self, 8, "param.frame.addr"), "param.frame");
                    }
                    LLVMTypeRef capturedType = llvmType(typeKey, error);
                    if (capturedType == nullptr) return nullptr;
                    LLVMValueRef frameValue = LLVMBuildLoad2(
                        builder_, capturedType,
                        byteOffset(frame, offset, "param.frame.slot"), "param.frame.load");
                    // NS304-T07-F06: a captured `ref` parameter stores the ADDRESS in the
                    // frame slot (typeKey is the pointer), but the USE evaluates to the
                    // pointee. Dereference once so the read is the pointed-to value.
                    if (typeKey.ends_with("*") && !node->typeKey.ends_with("*"))
                    {
                        LLVMTypeRef pointeeType = llvmType(node->typeKey, error);
                        if (pointeeType == nullptr) return nullptr;
                        return LLVMBuildLoad2(
                            builder_, pointeeType, frameValue, "param.frame.deref");
                    }
                    return frameValue;
                }
            }
        }
        LLVMValueRef address = parameterStorage_[*node->parameterOrdinal];
        LLVMTypeRef type = llvmType(node->typeKey, error);
        return type != nullptr && address != nullptr
            ? LLVMBuildLoad2(builder_, type, address, "param.load")
            : nullptr;
    }

LLVMValueRef OopLlvmImplementation::lowerAddressExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::ParameterAddress:
            {
                if (!node->parameterOrdinal.has_value()) return nullptr;
                // NS304-T07-F06: in $moveNext/$poll a captured parameter lives in the frame. A
                // captured `ref` parameter stores the ADDRESS in its slot; returning that loaded
                // pointer lets a store write through to the original. A value parameter returns
                // the slot address itself.
                if (currentMirFunction_ != nullptr &&
                    !currentMirFunction_->asyncFrameParameters.empty())
                {
                    const auto* indexedParameter = mirModel_.asyncFrameParameter(
                        *currentMirFunction_, *node->parameterOrdinal, node->typeKey);
                    if (indexedParameter != nullptr)
                    {
                        const auto& [paramOrdinal, offset, typeKey] = *indexedParameter;
                        // NS304-T07-F06: ordinal + physical type must both match (see ParameterValue).
                        if (paramOrdinal == *node->parameterOrdinal && node->typeKey == typeKey)
                        {
                            LLVMValueRef self = LLVMBuildLoad2(
                                builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                                "paramaddr.self");
                            LLVMValueRef frame = LLVMBuildLoad2(
                                builder_, LLVMPointerTypeInContext(context_, 0),
                                byteOffset(self, 8, "paramaddr.frame.addr"), "paramaddr.frame");
                            LLVMValueRef slot = byteOffset(frame, offset, "paramaddr.frame.slot");
                            if (typeKey.ends_with("*"))
                            {
                                LLVMTypeRef slotType = llvmType(typeKey, error);
                                if (slotType == nullptr) return nullptr;
                                return LLVMBuildLoad2(
                                    builder_, slotType, slot, "paramaddr.frame.ptr");
                            }
                            return slot;
                        }
                    }
                }
                return parameterStorage_[*node->parameterOrdinal];
            }
            case frontend::ProjectOopMirNodeKind::LocalAddress:
                return node->localOrdinal.has_value() ? localStorage_[*node->localOrdinal] : nullptr;
            case frontend::ProjectOopMirNodeKind::FieldAddress:
            case frontend::ProjectOopMirNodeKind::BitFieldAddress:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::StaticFieldAddress:
            case frontend::ProjectOopMirNodeKind::NamespaceStateAddress:
                return staticFieldAddress(node, error);
            case frontend::ProjectOopMirNodeKind::FutureFieldAddress:
                return lowerExpressionIterative(node, error);
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerLoadExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::FieldLoad:
            case frontend::ProjectOopMirNodeKind::BitFieldLoad:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::StaticFieldLoad:
            case frontend::ProjectOopMirNodeKind::NamespaceStateLoad:
            {
                LLVMValueRef address = staticFieldAddress(node, error);
                LLVMTypeRef type = llvmType(node->typeKey, error);
                if (address == nullptr || type == nullptr) return nullptr;
                LLVMValueRef load = LLVMBuildLoad2(builder_, type, address, "static.field.load");
                if (node->volatileAccess) LLVMSetVolatile(load, 1);
                return load;
            }
            case frontend::ProjectOopMirNodeKind::ArrayElementLoad:
            case frontend::ProjectOopMirNodeKind::StringElementLoad:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::RangeArrayElementLoad:
            {
                LLVMValueRef address = lowerRangeArrayElementAddress(node, error);
                LLVMTypeRef type = llvmType(node->elementTypeKey, error);
                return address != nullptr && type != nullptr
                    ? LLVMBuildLoad2(builder_, type, address, "range.element.load")
                    : nullptr;
            }
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerExceptionExpression(
    const frontend::ProjectOopMirNode* node,
    std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::Throw:
            {
                // NS304-T13-F08: a `Throw` used as an EXPRESSION (a `Conditional` failure branch).
                // lowerExceptionalTransfer already terminates the block with a trap + unreachable;
                // the dummy value below is never observed by any phi (the branch never falls
                // through).
                if (node->children.size() != 1 || node->children.front() == nullptr)
                    return failValue(error, "Stage 45.10 throw expression MIR requires one exception value");
                LLVMValueRef exception = lowerExpression(node->children.front().get(), error);
                if (exception == nullptr) return nullptr;
                if (terminated()) return nullptr;
                if (!lowerExceptionalTransfer(exception, node, error)) return nullptr;
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::Unreachable:
            {
                if (!node->children.empty() || node->typeKey != "builtin:never" ||
                    trapFunction_ == nullptr || trapFunctionType_ == nullptr)
                {
                    return failValue(error, "T07-F06 unreachable MIR lost its no-operand never contract");
                }
                LLVMValueRef reason = LLVMConstInt(
                    LLVMInt32TypeInContext(context_),
                    static_cast<std::uint64_t>(LlvmTrapReason::Unreachable),
                    0);
                LLVMValueRef arguments[] = {reason};
                buildNoUnwindCall(trapFunctionType_, trapFunction_, arguments, 1, "");
                LLVMBuildUnreachable(builder_);
                return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            }
            case frontend::ProjectOopMirNodeKind::Panic:
                return lowerPanicExpression(node, error);
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerPanicExpression(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() > 1U || node->typeKey != "builtin:never")
        {
            return failValue(error, "T07-F06 panic MIR lost its payload/never contract");
        }
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef message = LLVMConstNull(ptr);
        if (!node->children.empty())
        {
            message = lowerExpression(node->children.front().get(), error);
            if (message == nullptr || !error.empty()) return nullptr;
        }

        if (!mirModel_.panicHandlerMangledName().empty())
        {
            const auto handler = functions_.find(
                std::string(mirModel_.panicHandlerMangledName()));
            if (handler == functions_.end() || handler->second.function == nullptr ||
                handler->second.functionType == nullptr || sourceManager_ == nullptr ||
                node->syntax == nullptr || !node->syntax->range().begin().isValid())
            {
                return failValue(error,
                    "T07-F06 panic handler or source location is unavailable");
            }
            const SourceLocation location = node->syntax->range().begin();
            const source::SourcePosition position = sourceManager_->getPosition(location);
            LLVMValueRef file = internStringLiteral(
                sourceManager_->getPath(location), error);
            if (file == nullptr || !error.empty()) return nullptr;
            LLVMValueRef handlerArguments[] = {
                message,
                file,
                LLVMConstInt(i32, position.line, 0),
                LLVMConstInt(i32, position.column, 0)};
            if (buildNoUnwindCall(
                    handler->second.functionType, handler->second.function,
                    handlerArguments, 4, "") == nullptr)
            {
                return failValue(error, "T07-F06 failed to call the configured panic handler");
            }
        }
        if (mirModel_.panicProfile() == language::PanicProfile::Abort)
        {
            if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
                return failValue(error, "T07-F06 abort panic trap is unavailable");
            LLVMValueRef reason = LLVMConstInt(
                LLVMInt32TypeInContext(context_),
                static_cast<std::uint64_t>(LlvmTrapReason::Panic),
                0);
            LLVMValueRef arguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, arguments, 1, "");
        }
        else
        {
            if (raiseExceptionFunction_ == nullptr || raiseExceptionFunctionType_ == nullptr)
                return failValue(error, "T07-F06 native panic unwind hook is unavailable");
            LLVMValueRef arguments[] = {
                LLVMConstInt(i32, kNativeSharpPanicExceptionCode, 0),
                LLVMConstInt(i32, 1, 0),
                LLVMConstInt(i32, 0, 0),
                LLVMConstNull(ptr)};
            if (!lowerNativePanicTransfer(node, arguments, error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }
        LLVMBuildUnreachable(builder_);
        // The caller only uses this sentinel to distinguish successful lowering from an
        // internal error. No instruction may consume it after the terminator.
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerCleanupExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::FullExpressionCleanup:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::Conditional:
                return lowerConditional(node, error);
            case frontend::ProjectOopMirNodeKind::LoopExpression:
                return lowerLoopExpression(node, error);
            case frontend::ProjectOopMirNodeKind::MatchExpression:
                return lowerMatchExpression(node, error);
            case frontend::ProjectOopMirNodeKind::MatchValue:
                if (matchValueStack_.empty())
                    return failValue(error, "T03-F03 match value used outside an active match");
                return node->referenceResult
                    ? matchValueStack_.back().second
                    : matchValueStack_.back().first;
            case frontend::ProjectOopMirNodeKind::OverflowPolicy:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::NullCoalescing:
                return lowerNullCoalescing(node, error);
            case frontend::ProjectOopMirNodeKind::OptionalCoalescing:
                return lowerOptionalCoalescing(node, error);
            case frontend::ProjectOopMirNodeKind::NullCoalescingStore:
                return lowerNullCoalescingStore(node, error);
            case frontend::ProjectOopMirNodeKind::BorrowValue:
                return lowerExpressionIterative(node, error);
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerIteratorExpression(const frontend::ProjectOopMirNode* node, std::string& error)
{
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::IteratorCurrent:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::IteratorMoveNext:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::IteratorGetEnumerator:
            case frontend::ProjectOopMirNodeKind::IteratorDrop:
            case frontend::ProjectOopMirNodeKind::IteratorCancel:
                return lowerExpressionIterative(node, error);
            case frontend::ProjectOopMirNodeKind::IteratorMoveNextAsync:
                return lowerExpressionIterative(node, error);
            default:
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::lowerAwaitExpression(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride )
{
        switch (node->kind)
        {
            // integerValue: 1 suspends inside an async body; 0 is a synchronous await (one poll);
            // 2 is the async entry point, which drives the future to completion.
            case frontend::ProjectOopMirNodeKind::AwaitFuture:
                return node->integerValue == 1
                    ? lowerSuspendingAwait(node, error, childOverride)
                    : lowerBlockingAwait(node, error, childOverride);

            default:
                return nullptr;
        }
}

std::uint64_t OopLlvmImplementation::futureFieldOffset(
        std::string_view futureTypeKey, const std::size_t memberOrdinal) const
{
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, futureTypeKey);
        if (layout == nullptr) return 0;
        for (const frontend::ObjectFieldLayout& field : layout->fields)
            if (field.memberOrdinal == memberOrdinal) return field.offset;
        return 0;
}

bool OopLlvmImplementation::resolveAwaitPollTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        std::string_view addressName,
        std::string_view loadName,
        LLVMValueRef& pollFunction,
        LLVMTypeRef& pollFunctionType,
        std::string& error)
{
        if (node->calleeName.empty())
        {
            // NS304-T05-F09: variable await — resolve the poll from the future's pollFn.
            // NS304-T05-F09: await over a future VARIABLE — resolve the poll from the
            // future's pollFn field instead of a direct function reference.
            const std::uint64_t pollFnOffset = futureFieldOffset(node->elementTypeKey, 3);
            const std::string addressNameText(addressName);
            const std::string loadNameText(loadName);
            pollFunction = LLVMBuildLoad2(
                builder_, LLVMPointerTypeInContext(context_, 0),
                byteOffset(futureSlot, pollFnOffset, addressNameText.c_str()), loadNameText.c_str());
            LLVMTypeRef pollParamTypes[] = { LLVMPointerTypeInContext(context_, 0) };
            pollFunctionType = LLVMFunctionType(
                LLVMVoidTypeInContext(context_), pollParamTypes, 1, 0);
            return true;
        }
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end() || found->second.function == nullptr)
            return fail(error, "async poll target was not declared: " + node->calleeName);
        pollFunction = found->second.function;
        pollFunctionType = found->second.functionType;
        return true;
}

LLVMValueRef OopLlvmImplementation::allocateAwaitFrame(
        const std::uint64_t frameSize,
        const bool frameOnHeap,
        std::string_view stackName,
        std::string_view heapName,
        std::string_view heapFailure,
        std::string_view frameFailure,
        std::string& error)
{
        LLVMValueRef frameSlot = nullptr;
        if (frameOnHeap)
        {
            LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), frameSize, 0);
            LLVMValueRef allocArgs[] = {size};
            const std::string heapNameText(heapName);
            frameSlot = buildNoUnwindCall(
                allocationFunctionType_, allocationFunction_, allocArgs, 1, heapNameText.c_str());
            if (frameSlot == nullptr) return failValue(error, std::string(heapFailure));
        }
        else
        {
            LLVMTypeRef frameType = LLVMArrayType2(LLVMInt8TypeInContext(context_), frameSize);
            const std::string stackNameText(stackName);
            frameSlot = entryBlockAlloca(frameType, 8, stackNameText.c_str());
        }
        if (frameSlot == nullptr) return failValue(error, std::string(frameFailure));
        return frameSlot;
}

bool OopLlvmImplementation::attachBlockingAwaitFrame(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef& heapFrame,
        std::string& error)
{
        heapFrame = nullptr;
        if (node->calleeName.empty()) return true;
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end() || found->second.asyncFrameSize == 0) return true;
        // NS304-T05-F04: attach the awaited producer's frame (holds its own awaited
        // futures across suspension) so its suspend path has valid storage.
        const bool frameOnHeap =
            found->second.asyncFrameStorage == language::FrameStorage::ExplicitHeap ||
            found->second.asyncFrameStorage == language::FrameStorage::CallerArena;
        // EN: Freeze 3.0 C-14: a frame the constructor already allocated (heap) belongs to this
        //     future and is released when the await ends; a stack frame attached here is not.
        // ES: Congelamiento 3.0 C-14: un frame que el constructor ya reservo (heap) es de este
        //     future y se libera al terminar el await; un frame de pila adjuntado aqui no.
        if (!frameOnHeap)
            heapFrame = LLVMBuildLoad2(
                builder_, LLVMPointerTypeInContext(context_, 0),
                byteOffset(futureSlot, 8, "await.frame.ctor.addr"), "await.frame.ctor");
        if (!attachAwaitFrameIfMissing(futureSlot, found->second.asyncFrameSize, frameOnHeap, error))
            return false;
        // EN: Freeze 3.0 C-02: the frame to release is whichever one the future holds now.
        // ES: Congelamiento 3.0 C-02: el frame a liberar es el que el future tenga ahora.
        if (frameOnHeap)
            heapFrame = LLVMBuildLoad2(
                builder_, LLVMPointerTypeInContext(context_, 0),
                byteOffset(futureSlot, 8, "await.frame.held.addr"), "await.frame.held");
        return true;
}

bool OopLlvmImplementation::attachAwaitFrameIfMissing(
        LLVMValueRef futureSlot,
        std::uint64_t frameSize,
        bool frameOnHeap,
        std::string& error)
{
        // EN: Freeze 3.0 C-02 (D3): the producer's constructor allocates the frame and stores the
        //     parameters in it; replacing that frame lost the parameters. Attach only when absent.
        // ES: Congelamiento 3.0 C-02 (D3): el constructor del productor reserva el frame y guarda
        //     ahi los parametros; reemplazarlo los perdia. Solo se adjunta si falta.
        if (frameSize == 0) return true;
        LLVMValueRef field = byteOffset(futureSlot, 8, "await.frame.field");
        LLVMValueRef current = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), field, "await.frame.current");
        LLVMBasicBlockRef missing = appendBlock("await.frame.missing");
        LLVMBasicBlockRef present = appendBlock("await.frame.present");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(
                builder_, LLVMIntEQ, current, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
                "await.frame.absent"),
            missing, present);
        enterBlock(missing);
        LLVMValueRef frameSlot = allocateAwaitFrame(
            frameSize, frameOnHeap,
            "await.frame", "await.frame.heap",
            "await future could not reserve heap frame storage",
            "await future could not reserve frame storage", error);
        if (frameSlot == nullptr) return false;
        LLVMBuildStore(builder_, frameSlot, field);
        LLVMBuildBr(builder_, present);
        enterBlock(present);
        return true;
}

bool OopLlvmImplementation::emitBlockingAwaitPollLoop(
        LLVMValueRef futureSlot,
        LLVMValueRef pollFunction,
        LLVMTypeRef pollFunctionType,
        LLVMTypeRef intType,
        bool driveToCompletion,
        LLVMBasicBlockRef& loopDone,
        LLVMBasicBlockRef& loopError,
        std::string& error)
{
        // NS304-T05-F03: cooperative blocking poll — loop until the future reaches
        // ready (state == -1). For a synchronously-completing producer this runs once.
        // EN: Freeze 3.0 C-02 and DEC-04: only the async entry point drives the loop. Any other
        //     synchronous await polls once: semantic analysis rejects a producer known to suspend
        //     and accepts an unknown origin (D-04), so a future still pending afterwards panics
        //     instead of spinning.
        // ES: Congelamiento 3.0 C-02 y DEC-04: solo el punto de entrada async itera. Cualquier otro
        //     await sincrono hace un poll: el analisis rechaza un productor que se sabe que se
        //     suspende y acepta un origen desconocido (D-04), asi que un future aun pendiente
        //     despues es panico, no un ciclo.
        LLVMBasicBlockRef pollLoop = appendBlock("await.poll.loop");
        LLVMBasicBlockRef notReady = appendBlock("await.poll.notready");
        LLVMBasicBlockRef checkError = appendBlock("await.poll.checkerror");
        loopDone = appendBlock("await.poll.done");
        loopError = appendBlock("await.poll.error");
        LLVMBuildBr(builder_, pollLoop);
        enterBlock(pollLoop);
        // NS304-T07-F02: check the state BEFORE polling so an already-ready future
        // (state == -1, e.g. MoveNextAsync's eager demand) skips the poll entirely.
        LLVMValueRef ready = LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1);
        LLVMValueRef preState = LLVMBuildLoad2(builder_, intType, futureSlot, "await.pre.state");
        LLVMBuildCondBr(
            builder_, LLVMBuildICmp(builder_, LLVMIntEQ, preState, ready, "await.pre.ready"),
            loopDone, notReady);
        enterBlock(notReady);
        LLVMValueRef arguments[] = { futureSlot };
        if (LLVMBuildCall2(
                builder_, pollFunctionType, pollFunction, arguments, 1U, "") == nullptr)
            return fail(error, "async poll call failed");
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, futureSlot, "await.state");
        LLVMBuildCondBr(
            builder_, LLVMBuildICmp(builder_, LLVMIntEQ, state, ready, "await.ready"),
            loopDone, checkError);
        enterBlock(checkError);
        LLVMValueRef errored = LLVMConstInt(intType, static_cast<unsigned long long>(-2), 1);
        LLVMBasicBlockRef pending = driveToCompletion ? pollLoop : appendBlock("await.poll.pending");
        LLVMBuildCondBr(
            builder_, LLVMBuildICmp(builder_, LLVMIntEQ, state, errored, "await.errored"),
            loopError, pending);
        if (driveToCompletion) return true;
        enterBlock(pending);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_), static_cast<std::uint64_t>(LlvmTrapReason::Panic), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
        return true;
}

void OopLlvmImplementation::releaseAwaitHeapFrame(
        LLVMValueRef heapFrame, LLVMValueRef owned, LLVMValueRef forgetSlot)
{
        if (heapFrame == nullptr) return;
        // EN: Freeze 3.0 C-14: the frame may be absent (a constructor that stored nothing), and only
        //     a future that owns its frame (it carries a Drop pointer) releases it. An async
        //     iterator's demand future points at the iterator, which it does not own.
        // ES: Congelamiento 3.0 C-14: el frame puede faltar (un constructor que no guardo nada), y
        //     solo un future dueno de su frame (lleva puntero Drop) lo libera. El future de demanda
        //     de un iterador async apunta al iterador, que no es suyo.
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef present = LLVMBuildICmp(
            builder_, LLVMIntNE, heapFrame, LLVMConstNull(ptr), "await.frame.present.release");
        if (owned != nullptr) present = LLVMBuildAnd(builder_, present, owned, "await.frame.owned");
        LLVMBasicBlockRef release = appendBlock("await.frame.release");
        LLVMBasicBlockRef kept = appendBlock("await.frame.kept");
        LLVMBuildCondBr(builder_, present, release, kept);
        enterBlock(release);
        LLVMValueRef freeArgs[] = {heapFrame};
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, freeArgs, 1, "");
        if (forgetSlot != nullptr) LLVMBuildStore(builder_, LLVMConstNull(ptr), forgetSlot);
        LLVMBuildBr(builder_, kept);
        enterBlock(kept);
}

LLVMValueRef OopLlvmImplementation::futureOwnsFrame(LLVMValueRef futureSlot, std::string_view futureKey)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        const std::uint64_t dropOffset = futureFieldOffset(futureKey, 5);
        if (dropOffset == 0) return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        LLVMValueRef dropFn = LLVMBuildLoad2(
            builder_, ptr, byteOffset(futureSlot, dropOffset, "await.drop.fn.addr"), "await.drop.fn");
        return LLVMBuildICmp(builder_, LLVMIntNE, dropFn, LLVMConstNull(ptr), "await.owns.frame");
}

// EN: Freeze 3.0 C-14: `await f` consumes the future local or parameter: the await works on its own
//     copy, so the source is zeroed and its scope drop finds nothing to release a second time.
// ES: Congelamiento 3.0 C-14: `await f` consume el future local o parametro: el await trabaja sobre
//     su propia copia, asi que la fuente queda en cero y su drop de scope no libera nada otra vez.
void OopLlvmImplementation::consumeAwaitedFuture(
        const frontend::ProjectOopMirNode* operand, std::string& error)
{
        if (operand == nullptr || terminated()) return;
        LLVMValueRef storage = nullptr;
        if (operand->kind == frontend::ProjectOopMirNodeKind::LocalValue && operand->localOrdinal.has_value())
        {
            const auto found = localStorage_.find(*operand->localOrdinal);
            if (found != localStorage_.end()) storage = found->second;
        }
        else if (operand->kind == frontend::ProjectOopMirNodeKind::ParameterValue &&
                 operand->parameterOrdinal.has_value())
        {
            storage = capturedParameterSlot(*operand->parameterOrdinal, operand->typeKey);
            if (storage == nullptr)
            {
                const auto found = parameterStorage_.find(*operand->parameterOrdinal);
                if (found != parameterStorage_.end()) storage = found->second;
            }
        }
        if (storage == nullptr) return;
        LLVMTypeRef type = llvmType(operand->typeKey, error);
        if (type == nullptr) return;
        LLVMBuildStore(builder_, LLVMConstNull(type), storage);
}

bool OopLlvmImplementation::emitBlockingAwaitErrorPath(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef heapFrame,
        LLVMBasicBlockRef loopMerge,
        bool& transfersException,
        LLVMBasicBlockRef& errorExit,
        std::string& error)
{
        // NS304-T05-F07 rethrow: the future errored (state == -2). Load the carried
        // error slot and rethrow it into the enclosing catch (native unwind) instead of
        // the deterministic -1 signal.
        releaseAwaitHeapFrame(heapFrame, futureOwnsFrame(futureSlot, node->elementTypeKey));
        const std::optional<std::string_view> errorInner =
            language::futureErrorTypeKey(node->elementTypeKey);
        const std::string errorKey = errorInner.has_value()
            ? std::string(*errorInner) : std::string{};
        LLVMTypeRef errorType = errorKey.empty() ? nullptr : llvmType(errorKey, error);
        const std::uint64_t errorOffset = futureFieldOffset(node->elementTypeKey, 4);
        // EN: Freeze 3.0 C-17: a synchronous await (integerValue 0) outside any try propagates an
        //     exception error to its caller, running the active cleanup, as a call does; before, it
        //     returned the default result and the error was lost. Only the async entry point
        //     (integerValue 2) keeps the deterministic signal; C-13 already requires the enclosing
        //     callable to declare what is not caught.
        // ES: Congelamiento 3.0 C-17: un await sincrono (integerValue 0) fuera de todo try propaga un
        //     error de excepcion a su llamador, corriendo el cleanup activo, como una llamada; antes
        //     devolvia el resultado por defecto y el error se perdia. Solo el punto de entrada async
        //     (integerValue 2) conserva la senal determinista; C-13 ya exige que el callable
        //     envolvente declare lo que no se captura.
        const bool propagatesToCaller = node->integerValue == 0 && !errorKey.empty() &&
            isClassReferenceType(nameLookup_, errorKey);
        transfersException = (node->exceptionTargetRegionOrdinal.has_value() || propagatesToCaller) &&
            errorType != nullptr && errorOffset != 0;
        if (transfersException)
        {
            LLVMValueRef exception = LLVMBuildLoad2(
                builder_, errorType,
                byteOffset(futureSlot, errorOffset, "await.error.addr"), "await.error");
            if (exception == nullptr) return false;
            return lowerExceptionalTransfer(exception, node, error);
        }
        // No enclosing catch (or a non-throwing future): keep the deterministic
        // signal, which the caller observes as the defined error exit.
        errorExit = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, loopMerge);
        return true;
}

LLVMValueRef OopLlvmImplementation::finishBlockingAwait(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef heapFrame,
        LLVMBasicBlockRef loopDone,
        LLVMBasicBlockRef loopError,
        std::string& error)
{
        // Ready: load the result and merge with the error path.
        enterBlock(loopDone);
        // EN: Freeze 3.0 C-14: a variable await attaches nothing, so the frame the future holds
        //     now (its constructor's, or the one its poll allocated) is released here.
        // ES: Congelamiento 3.0 C-14: un await de variable no adjunta nada, asi que el frame que el
        //     future tiene ahora (el de su constructor, o el que reservo su poll) se libera aqui.
        if (heapFrame == nullptr && node->calleeName.empty())
            heapFrame = LLVMBuildLoad2(
                builder_, LLVMPointerTypeInContext(context_, 0),
                byteOffset(futureSlot, 8, "await.frame.held.addr"), "await.frame.held");
        // NS304-T07-F06: `await future;` on a Future<void> has no result slot to read —
        // the poll runs the future to completion and the statement discards the value.
        const bool voidResult = node->typeKey == "builtin:void";
        LLVMTypeRef resultType = nullptr;
        LLVMValueRef readyValue = nullptr;
        if (!voidResult)
        {
            resultType = llvmType(node->typeKey, error);
            if (resultType == nullptr) return nullptr;
            readyValue = LLVMBuildLoad2(
                builder_, resultType,
                byteOffset(futureSlot, node->immediate, "await.result.addr"), "await.result");
        }
        LLVMBasicBlockRef loopMerge = appendBlock("await.poll.merge");
        releaseAwaitHeapFrame(heapFrame, futureOwnsFrame(futureSlot, node->elementTypeKey));
        // EN: Freeze 3.0 C-14: releasing the frame adds blocks; the phi takes the real exits.
        // ES: Congelamiento 3.0 C-14: liberar el frame agrega bloques; el phi toma las salidas reales.
        LLVMBasicBlockRef readyExit = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, loopMerge);
        enterBlock(loopError);
        if (node->calleeName.empty())
            heapFrame = LLVMBuildLoad2(
                builder_, LLVMPointerTypeInContext(context_, 0),
                byteOffset(futureSlot, 8, "await.frame.held.addr"), "await.frame.held");
        bool transfersException = false;
        LLVMBasicBlockRef errorExit = loopError;
        if (!emitBlockingAwaitErrorPath(
                node, futureSlot, heapFrame, loopMerge, transfersException, errorExit, error))
            return nullptr;
        enterBlock(loopMerge);
        if (voidResult)
        {
            // A blocking await of a Future<void> runs it to completion and yields no
            // value; the expression statement discards the placeholder below.
            return LLVMConstNull(LLVMInt8TypeInContext(context_));
        }
        LLVMValueRef awaitPhi = LLVMBuildPhi(builder_, resultType, "await.value");
        if (transfersException)
        {
            LLVMAddIncoming(awaitPhi, &readyValue, &readyExit, 1);
        }
        else
        {
            LLVMValueRef incomingValues[] = {
                readyValue, LLVMConstInt(resultType, static_cast<unsigned long long>(-1), 1) };
            LLVMBasicBlockRef incomingBlocks[] = { readyExit, errorExit };
            LLVMAddIncoming(awaitPhi, incomingValues, incomingBlocks, 2);
        }
        return awaitPhi;
}

LLVMValueRef OopLlvmImplementation::lowerBlockingAwait(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride)
{
        // NS304-T05-F03 sync context: blocking await. Poll once and read the result; no
        // suspension (the producer cannot suspend, enforced by the Stage 34 guard).
        if (node->children.size() != 1U)
            return failValue(error, "await future requires one operand");
        LLVMValueRef value = childOverride != nullptr
            ? childOverride
            : lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef carrier = llvmType(node->elementTypeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef futureSlot = entryBlockAlloca(
            carrier, structAlignmentFor(node->elementTypeKey), "await.future");
        if (futureSlot == nullptr)
            return failValue(error, "await future could not reserve temporary storage");
        LLVMBuildStore(builder_, value, futureSlot);
        consumeAwaitedFuture(node->children.front().get(), error);
        if (!error.empty()) return nullptr;
        LLVMValueRef pollFunction = nullptr;
        LLVMTypeRef pollFunctionType = nullptr;
        if (!resolveAwaitPollTarget(
                node, futureSlot, "await.pollfn.addr", "await.pollfn",
                pollFunction, pollFunctionType, error)) return nullptr;
        LLVMValueRef heapFrame = nullptr;
        if (!attachBlockingAwaitFrame(node, futureSlot, heapFrame, error)) return nullptr;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return nullptr;
        LLVMBasicBlockRef loopDone = nullptr;
        LLVMBasicBlockRef loopError = nullptr;
        if (!emitBlockingAwaitPollLoop(
                futureSlot, pollFunction, pollFunctionType, intType, node->integerValue == 2,
                loopDone, loopError, error)) return nullptr;
        return finishBlockingAwait(
            node, futureSlot, heapFrame, loopDone, loopError, error);
}

void OopLlvmImplementation::awaitChildFrameInfo(
        const frontend::ProjectOopMirNode* node,
        bool& frameOnHeap,
        std::uint64_t& frameSize) const
{
        const auto found = functions_.find(node->calleeName);
        frameOnHeap = found != functions_.end() &&
            (found->second.asyncFrameStorage == language::FrameStorage::ExplicitHeap ||
             found->second.asyncFrameStorage == language::FrameStorage::CallerArena);
        frameSize = found != functions_.end() ? found->second.asyncFrameSize : 0;
}

bool OopLlvmImplementation::emitAwaitChildSelection(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef childOverride,
        LLVMValueRef self,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        LLVMValueRef childSlot,
        const bool childFrameOnHeap,
        const std::uint64_t childFrameSize,
        LLVMBasicBlockRef& pollBlock,
        std::string& error)
{
        // 1. Select: entry (self->state == 0) → fresh child; resume → self->frame (slot 0).
        LLVMValueRef selfState = LLVMBuildLoad2(builder_, intType, self, "await.self.state");
        LLVMBasicBlockRef entryBlock = appendBlock("await.entry");
        LLVMBasicBlockRef resumeBlock = appendBlock("await.resume");
        pollBlock = appendBlock("await.poll");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntEQ, selfState, LLVMConstInt(intType, 0, 0), "await.is.entry"),
            entryBlock, resumeBlock);
        // Entry: lower the child call → store in childSlot.
        enterBlock(entryBlock);
        LLVMValueRef fresh = childOverride != nullptr
            ? childOverride
            : lowerExpression(node->children.front().get(), error);
        if (fresh == nullptr) return false;
        LLVMBuildStore(builder_, fresh, childSlot);
        consumeAwaitedFuture(node->children.front().get(), error);
        if (!error.empty()) return false;
        // NS304-T05-F04: attach the awaited producer's frame (holds its own awaited futures).
        if (!attachAwaitFrameIfMissing(childSlot, childFrameSize, childFrameOnHeap, error))
            return false;
        LLVMBuildBr(builder_, pollBlock);
        // Resume: load self->frame (offset 8) → childSlot.
        enterBlock(resumeBlock);
        LLVMValueRef framePtr = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0),
            byteOffset(self, 8, "await.self.frame.addr"), "await.self.frame");
        LLVMBuildStore(
            builder_, LLVMBuildLoad2(builder_, carrier, framePtr, "await.saved.child"), childSlot);
        LLVMBuildBr(builder_, pollBlock);
        enterBlock(pollBlock);
        return true;
}

void OopLlvmImplementation::emitAwaitSuspendPath(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        LLVMBasicBlockRef suspendBlock)
{
        // Suspend: save child in self->frame + state=N + ret void.
        enterBlock(suspendBlock);
        // EN: Freeze 3.0 C-02: in a frame-resident coroutine the child already lives in its own
        //     frame slot; only the state (this await's id) is left behind.
        // ES: Congelamiento 3.0 C-02: en una corrutina con frame residente el hijo ya vive en su
        //     slot del frame; solo queda el estado (el id de este await).
        if (coroutine_.active)
        {
            const auto id = coroutine_.suspensionIds.find(node);
            const std::int64_t state = id == coroutine_.suspensionIds.end() ? 1 : id->second;
            LLVMBuildStore(
                builder_, LLVMConstInt(intType, static_cast<unsigned long long>(state), 1), self);
            LLVMBuildRetVoid(builder_);
            return;
        }
        LLVMValueRef suspendFrame = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0),
            byteOffset(self, 8, "await.self.frame.addr2"), "await.self.frame");
        LLVMBuildStore(
            builder_, LLVMBuildLoad2(builder_, carrier, childSlot, "await.child.to.save"),
            suspendFrame);
        LLVMBuildStore(builder_, LLVMConstInt(intType, node->integerValue, 0), self);
        LLVMBuildRetVoid(builder_);
}

void OopLlvmImplementation::releaseAwaitChildFrame(LLVMValueRef childSlot, std::string_view futureKey)
{
        LLVMValueRef childFramePtr = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0),
            byteOffset(childSlot, 8, "await.child.frame.heapfree"),
            "await.child.frame.heapfree");
        // EN: Freeze 3.0 C-02: a released frame is forgotten, so re-reading a completed child
        //     never frees it twice.
        // ES: Congelamiento 3.0 C-02: un frame liberado se olvida, asi releer un hijo completado
        //     nunca lo libera dos veces.
        releaseAwaitHeapFrame(
            childFramePtr, futureOwnsFrame(childSlot, futureKey),
            byteOffset(childSlot, 8, "await.child.frame.forget"));
}

bool OopLlvmImplementation::emitSuspendingAwaitErrorPath(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef errorType,
        const std::uint64_t errorOffset,
        const bool childFrameOnHeap,
        LLVMBasicBlockRef errorBlock,
        std::string& error)
{
        // ReadyError: child errored. Propagate the carried error into the parent
        // future's error slot and mark the parent errored (state == -2) — no unwind.
        enterBlock(errorBlock);
        LLVMValueRef childError = LLVMBuildLoad2(
            builder_, errorType,
            byteOffset(childSlot, errorOffset, "await.child.error.addr"), "await.child.error");
        if (childFrameOnHeap) releaseAwaitChildFrame(childSlot, node->elementTypeKey);
        // EN: Freeze 3.0 C-13: an await inside a try of this async body hands the error to that
        //     catch, as a synchronous await does. Before, it always left the poll, so the catch
        //     never ran and a non-throwing parent got a write past its future.
        // ES: Congelamiento 3.0 C-13: un await dentro de un try de este cuerpo async entrega el
        //     error a ese catch, igual que un await sincrono. Antes siempre salia del poll, asi
        //     que el catch nunca corria y un padre que no lanza recibia una escritura fuera de su
        //     future.
        if (node->exceptionTargetRegionOrdinal.has_value())
            return lowerExceptionalTransfer(childError, node, error);
        // EN: Otherwise the error goes to this future's own error slot, at this future's offset.
        // ES: Si no, el error va al slot de error de este future, en el offset de este future.
        std::string parentKey = currentMirFunction_ != nullptr &&
                !currentMirFunction_->parameterTypeKeys.empty()
            ? currentMirFunction_->parameterTypeKeys.front() : std::string{};
        if (parentKey.ends_with('*')) parentKey.pop_back();
        const std::uint64_t parentErrorOffset = language::futureErrorTypeKey(parentKey).has_value()
            ? futureFieldOffset(parentKey, 4) : 0;
        // EN: Freeze 3.0 C-17: an async-generator poll publishes in its demand future, not in the
        //     async-iterator that holds its state, and the iterator is parked as finished.
        // ES: Congelamiento 3.0 C-17: el poll de un generador async publica en su future de
        //     demanda, no en el iterador async que guarda su estado, y el iterador queda terminado.
        LLVMValueRef published = coroutine_.active && coroutine_.future != nullptr ? coroutine_.future : self;
        if (coroutine_.asyncGenerator && !markCoroutineFinished(error)) return false;
        if (parentErrorOffset != 0)
            LLVMBuildStore(
                builder_, childError, byteOffset(published, parentErrorOffset, "await.self.error.addr"));
        LLVMValueRef errorConst = LLVMConstInt(intType, static_cast<unsigned long long>(-2), 1);
        LLVMBuildStore(builder_, errorConst, published);
        // EN: Freeze 3.0 C-17: the error leaves this body, so the active cleanup runs first, as for
        //     a `throw` that leaves it (its synthesized return carries the same regions). Before,
        //     the live locals of this async body were never destroyed on this path.
        // ES: Congelamiento 3.0 C-17: el error sale de este cuerpo, asi que primero corre el cleanup
        //     activo, como en un `throw` que sale de el (su return sintetizado lleva las mismas
        //     regiones). Antes, los locales vivos de este cuerpo async nunca se destruian aqui.
        //     The error branch is separate from the ready path lowered next, so the cleanup state
        //     the walk changes is restored, as the cancel path of a yield does.
        //     La rama de error es aparte de la ruta lista que se baja despues, asi que el estado
        //     de cleanup que el recorrido cambia se restaura, como en la cancelacion de un yield.
        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        std::size_t pathIndex = 0;
        const bool cleaned = !funcletStack_.empty() || emitRemainingStructuredCleanups(node, pathIndex, error);
        activeCleanupRegions_ = savedActive;
        funcletStack_ = savedFunclets;
        if (!cleaned) return false;
        if (!terminated()) LLVMBuildRetVoid(builder_);
        return true;
}

LLVMValueRef OopLlvmImplementation::finishSuspendingAwait(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        const bool childFrameOnHeap,
        std::string& error)
{
        // 2. Check pending: child.state == -1 (ready) → extract; child.state == -2
        // (ReadyError) → propagate the error to the parent; else suspend. The error path
        // only exists for a throwing future (one with an error slot).
        LLVMValueRef childState = LLVMBuildLoad2(builder_, intType, childSlot, "await.child.state");
        LLVMValueRef readyConst = LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1);
        const std::optional<std::string_view> errorInner =
            language::futureErrorTypeKey(node->elementTypeKey);
        const std::string errorKey = errorInner.has_value()
            ? std::string(*errorInner) : std::string{};
        LLVMTypeRef errorType = errorKey.empty() ? nullptr : llvmType(errorKey, error);
        const std::uint64_t errorOffset = futureFieldOffset(node->elementTypeKey, 4);
        const bool hasErrorSlot = errorType != nullptr && errorOffset != 0;
        LLVMBasicBlockRef readyBlock = appendBlock("await.ready");
        LLVMBasicBlockRef suspendBlock = appendBlock("await.suspend");
        if (!hasErrorSlot)
        {
            // Non-throwing child: only ready vs suspend.
            LLVMBuildCondBr(
                builder_, LLVMBuildICmp(builder_, LLVMIntEQ, childState, readyConst, "await.is.ready"),
                readyBlock, suspendBlock);
        }
        else
        {
            LLVMValueRef errorConst = LLVMConstInt(intType, static_cast<unsigned long long>(-2), 1);
            LLVMBasicBlockRef errorCheckBlock = appendBlock("await.errorcheck");
            LLVMBasicBlockRef errorBlock = appendBlock("await.error");
            LLVMBuildCondBr(
                builder_, LLVMBuildICmp(builder_, LLVMIntEQ, childState, readyConst, "await.is.ready"),
                readyBlock, errorCheckBlock);
            enterBlock(errorCheckBlock);
            LLVMBuildCondBr(
                builder_, LLVMBuildICmp(builder_, LLVMIntEQ, childState, errorConst, "await.is.error"),
                errorBlock, suspendBlock);
            if (!emitSuspendingAwaitErrorPath(
                    node, self, childSlot, intType, errorType, errorOffset, childFrameOnHeap,
                    errorBlock, error)) return nullptr;
        }
        emitAwaitSuspendPath(node, self, childSlot, intType, carrier, suspendBlock);
        // Ready: load result (immediate = result offset).
        enterBlock(readyBlock);
        // EN: Freeze 3.0 C-02: a Future<void> has no result slot; the statement discards this
        //     placeholder, as the blocking await already does. Loading `void` crashed the compiler.
        // ES: Congelamiento 3.0 C-02: un Future<void> no tiene slot de resultado; la sentencia
        //     descarta este marcador, igual que el await bloqueante. Cargar `void` tumbaba nsc.
        if (node->typeKey == "builtin:void")
        {
            if (childFrameOnHeap) releaseAwaitChildFrame(childSlot, node->elementTypeKey);
            return LLVMConstNull(LLVMInt8TypeInContext(context_));
        }
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        if (childFrameOnHeap) releaseAwaitChildFrame(childSlot, node->elementTypeKey);
        return LLVMBuildLoad2(
            builder_, resultType, byteOffset(childSlot, node->immediate, "await.result.addr"),
            "await.result");
}

LLVMValueRef OopLlvmImplementation::lowerSuspendingAwait(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride)
{
        // NS304-T05-F03 suspend/resume. `self` is the poll's parameter 0 (future:<T>*);
        // the future layout is { i32 state@0, void* frame@8, T result@16 }.
        if (coroutine_.active) return lowerCoroutineAwait(node, error, childOverride);
        if (node->children.size() != 1U)
            return failValue(error, "await future requires one operand");
        if (parameterStorage_.empty())
            return failValue(error, "await future has no poll self parameter");
        // parameterStorage_[0] is the address of the self parameter slot; load the future
        // pointer out of it before dereferencing the future's fields.
        LLVMValueRef self = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0], "await.self");
        LLVMTypeRef intType = llvmType("builtin:int", error);
        LLVMTypeRef carrier = llvmType(node->elementTypeKey, error);
        if (intType == nullptr || carrier == nullptr) return nullptr;
        LLVMValueRef childSlot = entryBlockAlloca(
            carrier, structAlignmentFor(node->elementTypeKey), "await.child");
        if (childSlot == nullptr)
            return failValue(error, "await future could not reserve child storage");
        // NS304-T05-F08: the awaited producer's frame storage (heap vs stack) is a compile-time
        // property, hoisted so both the entry allocation and the terminal free agree.
        bool childFrameOnHeap = false;
        std::uint64_t childFrameSize = 0;
        awaitChildFrameInfo(node, childFrameOnHeap, childFrameSize);
        LLVMBasicBlockRef pollBlock = nullptr;
        if (!emitAwaitChildSelection(
                node, childOverride, self, intType, carrier, childSlot,
                childFrameOnHeap, childFrameSize, pollBlock, error)) return nullptr;
        LLVMValueRef pollFunction = nullptr;
        LLVMTypeRef pollFunctionType = nullptr;
        // Poll: call G$poll(&childSlot) — or the future's pollFn for a variable await.
        if (!resolveAwaitPollTarget(
                node, childSlot, "await.child.pollfn", "await.child.pollfn",
                pollFunction, pollFunctionType, error)) return nullptr;
        LLVMValueRef pollArgs[] = { childSlot };
        if (LLVMBuildCall2(
                builder_, pollFunctionType, pollFunction, pollArgs, 1U, "") == nullptr)
            return failValue(error, "async poll call failed");
        return finishSuspendingAwait(
            node, self, childSlot, intType, carrier, childFrameOnHeap, error);
}

}
