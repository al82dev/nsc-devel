#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::lowerExpressionIterative(
    const frontend::ProjectOopMirNode* node,
    std::string& error)
{
        std::vector<LlvmExpressionFrame> frames;
        std::vector<LLVMValueRef> values;
        frames.push_back({node, 0, nullptr});
        while (!frames.empty())
        {
            const LlvmExpressionFrame frame = frames.back();
            frames.pop_back();
            const frontend::ProjectOopMirNode* current = frame.node;
            if (current == nullptr)
            {
                values.push_back(nullptr);
                continue;
            }
            if (lowerExpressionFrameGroup01(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup02(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup03(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup04(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup05(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup06(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup07(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup08(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup09(frame, frames, values, error)) continue;
            if (lowerExpressionFrameGroup10(frame, frames, values, error)) continue;
            // The remaining families still use the recursive lowerExpression.
            values.push_back(lowerExpression(current, error));
        }
        return values.back();
    }

bool OopLlvmImplementation::lowerExpressionFrameGroup01(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame001Unary(frame, frames, values, error)) return true;
    if (lowerExpressionFrame002Binary(frame, frames, values, error)) return true;
    if (lowerExpressionFrame003FieldAddress(frame, frames, values, error)) return true;
    if (lowerExpressionFrame004FieldLoad(frame, frames, values, error)) return true;
    if (lowerExpressionFrame005BitFieldLoad(frame, frames, values, error)) return true;
    if (lowerExpressionFrame006ArrayElementAddress(frame, frames, values, error)) return true;
    if (lowerExpressionFrame007FutureFieldAddress(frame, frames, values, error)) return true;
    if (lowerExpressionFrame008OwnedTemporaryBorrow(frame, frames, values, error)) return true;
    if (lowerExpressionFrame009FullExpressionCleanup(frame, frames, values, error)) return true;
    if (lowerExpressionFrame010OverflowPolicy(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup02(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame011IteratorGetEnumerator(frame, frames, values, error)) return true;
    if (lowerExpressionFrame012IteratorCurrent(frame, frames, values, error)) return true;
    if (lowerExpressionFrame013IteratorDrop(frame, frames, values, error)) return true;
    if (lowerExpressionFrame014IteratorCancel(frame, frames, values, error)) return true;
    if (lowerExpressionFrame015IteratorMoveNextAsync(frame, frames, values, error)) return true;
    if (lowerExpressionFrame016IteratorMoveNext(frame, frames, values, error)) return true;
    if (lowerExpressionFrame017BorrowValue(frame, frames, values, error)) return true;
    if (lowerExpressionFrame018DirectInstanceCall(frame, frames, values, error)) return true;
    if (lowerExpressionFrame019DelegateInvoke(frame, frames, values, error)) return true;
    if (lowerExpressionFrame020DirectStaticCall(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup03(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame021ObjectDestroy(frame, frames, values, error)) return true;
    if (lowerExpressionFrame022Deallocate(frame, frames, values, error)) return true;
    if (lowerExpressionFrame023ValueConstruct(frame, frames, values, error)) return true;
    if (lowerExpressionFrame024AllocateAndConstruct(frame, frames, values, error)) return true;
    if (lowerExpressionFrame025PlacementConstruct(frame, frames, values, error)) return true;
    if (lowerExpressionFrame026AwaitFuture(frame, frames, values, error)) return true;
    if (lowerExpressionFrame027AggregateInitialize(frame, frames, values, error)) return true;
    if (lowerExpressionFrame028AddressValue(frame, frames, values, error)) return true;
    if (lowerExpressionFrame029MoveArrayElement(frame, frames, values, error)) return true;
    if (lowerExpressionFrame030PointerLoad(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup04(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame031MoveOwnedField(frame, frames, values, error)) return true;
    if (lowerExpressionFrame032MoveIteratorCurrent(frame, frames, values, error)) return true;
    if (lowerExpressionFrame033RawElementAddress(frame, frames, values, error)) return true;
    if (lowerExpressionFrame034PointerArithmetic(frame, frames, values, error)) return true;
    if (lowerExpressionFrame035CompoundStore(frame, frames, values, error)) return true;
    if (lowerExpressionFrame036StackAllocation(frame, frames, values, error)) return true;
    if (lowerExpressionFrame037NullConditionalFieldLoad(frame, frames, values, error)) return true;
    if (lowerExpressionFrame038NullConditionalArrayLoad(frame, frames, values, error)) return true;
    if (lowerExpressionFrame039NullConditionalFieldStore(frame, frames, values, error)) return true;
    if (lowerExpressionFrame040NullConditionalArrayStore(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup05(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame041Convert(frame, frames, values, error)) return true;
    if (lowerExpressionFrame042Assert(frame, frames, values, error)) return true;
    if (lowerExpressionFrame043ResultPropagation(frame, frames, values, error)) return true;
    if (lowerExpressionFrame044Store(frame, frames, values, error)) return true;
    if (lowerExpressionFrame045TupleDeconstruct(frame, frames, values, error)) return true;
    if (lowerExpressionFrame046ParameterPack(frame, frames, values, error)) return true;
    if (lowerExpressionFrame047CoreConsoleWriteLine(frame, frames, values, error)) return true;
    if (lowerExpressionFrame048CoreConsoleReadLine(frame, frames, values, error)) return true;
    if (lowerExpressionFrame049CoreEnvironmentExit(frame, frames, values, error)) return true;
    if (lowerExpressionFrame050CoreEnvironmentGetVariable(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup06(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame051CoreEnvironmentSetVariable(frame, frames, values, error)) return true;
    if (lowerExpressionFrame052CoreFileExists(frame, frames, values, error)) return true;
    if (lowerExpressionFrame053CoreFileReadAllText(frame, frames, values, error)) return true;
    if (lowerExpressionFrame054CoreFileReadBytes(frame, frames, values, error)) return true;
    if (lowerExpressionFrame055CoreFileWriteAllBytes(frame, frames, values, error)) return true;
    if (lowerExpressionFrame056CoreFileWriteAllText(frame, frames, values, error)) return true;
    if (lowerExpressionFrame057CoreDirectoryDelete(frame, frames, values, error)) return true;
    if (lowerExpressionFrame058CoreEnvironmentGetCommandLineArgs(frame, frames, values, error)) return true;
    if (lowerExpressionFrame059CoreDirectoryGetFiles(frame, frames, values, error)) return true;
    if (lowerExpressionFrame060CoreThreadCreate(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup07(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame061CoreThreadJoin(frame, frames, values, error)) return true;
    if (lowerExpressionFrame062CoreMutexCreateNamed(frame, frames, values, error)) return true;
    if (lowerExpressionFrame063CoreMonitorEnter(frame, frames, values, error)) return true;
    if (lowerExpressionFrame064CoreSemaphoreRelease(frame, frames, values, error)) return true;
    if (lowerExpressionFrame065CoreSemaphoreWait(frame, frames, values, error)) return true;
    if (lowerExpressionFrame066CoreSemaphoreCreate(frame, frames, values, error)) return true;
    if (lowerExpressionFrame067CoreSemaphoreCreateNamed(frame, frames, values, error)) return true;
    if (lowerExpressionFrame068CoreProcessRun(frame, frames, values, error)) return true;
    if (lowerExpressionFrame069CoreProcessSetPriorityClass(frame, frames, values, error)) return true;
    if (lowerExpressionFrame070CoreFileCopy(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup08(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame071CoreFileMove(frame, frames, values, error)) return true;
    if (lowerExpressionFrame072CoreInterlockedIncrementInt32(frame, frames, values, error)) return true;
    if (lowerExpressionFrame073CoreInterlockedIncrementInt64(frame, frames, values, error)) return true;
    if (lowerExpressionFrame074CoreInterlockedAddInt32(frame, frames, values, error)) return true;
    if (lowerExpressionFrame075CoreInterlockedAddInt64(frame, frames, values, error)) return true;
    if (lowerExpressionFrame076CoreAtomicLoadInt32(frame, frames, values, error)) return true;
    if (lowerExpressionFrame077CoreAtomicStoreInt32(frame, frames, values, error)) return true;
    if (lowerExpressionFrame078CoreAtomicLoadPointer(frame, frames, values, error)) return true;
    if (lowerExpressionFrame079CoreAtomicStorePointer(frame, frames, values, error)) return true;
    if (lowerExpressionFrame080CoreInterlockedCompareExchangeInt32(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup09(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame081CoreInterlockedCompareExchangePointer(frame, frames, values, error)) return true;
    if (lowerExpressionFrame082CoreSimdLoadAligned128(frame, frames, values, error)) return true;
    if (lowerExpressionFrame083CoreSimdStoreAligned128(frame, frames, values, error)) return true;
    if (lowerExpressionFrame084CoreCpuPrefetchRead(frame, frames, values, error)) return true;
    if (lowerExpressionFrame085CoreHalfFromFloat(frame, frames, values, error)) return true;
    if (lowerExpressionFrame086CoreHalfToFloat(frame, frames, values, error)) return true;
    if (lowerExpressionFrame087CoreMonitorWait(frame, frames, values, error)) return true;
    if (lowerExpressionFrame088CoreSimdFloat32x4Multiply(frame, frames, values, error)) return true;
    if (lowerExpressionFrame089CoreSimdInt128Multiply(frame, frames, values, error)) return true;
    if (lowerExpressionFrame090TemporaryAddress(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrameGroup10(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    if (lowerExpressionFrame091CoreBitPopulationCount(frame, frames, values, error)) return true;
    if (lowerExpressionFrame092CoreBitRotateLeft(frame, frames, values, error)) return true;
    if (lowerExpressionFrame093CoreSimdFloat256Add(frame, frames, values, error)) return true;
    if (lowerExpressionFrame094CoreSimdInt256Add(frame, frames, values, error)) return true;
    if (lowerExpressionFrame095CoreSimdSqrt(frame, frames, values, error)) return true;
    if (lowerExpressionFrame096CoreSimdSwizzleWzyx(frame, frames, values, error)) return true;
    if (lowerExpressionFrame097StringViewFromString(frame, frames, values, error)) return true;
    if (lowerExpressionFrame098SpanFromArray(frame, frames, values, error)) return true;
    if (lowerExpressionFrame099SpanFromFixedArray(frame, frames, values, error)) return true;
    return false;
}

bool OopLlvmImplementation::lowerExpressionFrame001Unary(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Unary)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "unary MIR requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef operand = values.back();
    values.pop_back();
    if (operand == nullptr) { values.push_back(nullptr); return true; }
    if (terminated()) { values.push_back(operand); return true; }
    if (current->operation == lexer::TokenKind::Plus)
    {
        values.push_back(operand);
        return true;
    }
    if (current->operation == lexer::TokenKind::Bang)
    {
        values.push_back(LLVMBuildNot(builder_, operand, "not"));
        return true;
    }
    if (current->operation == lexer::TokenKind::Tilde)
    {
        values.push_back(LLVMBuildNot(builder_, operand, "bitnot"));
        return true;
    }
    if (current->operation == lexer::TokenKind::Minus)
    {
        if (current->typeKey == "builtin:decimal")
        {
            std::uint64_t signWords[2] = { 0, 0x8000000000000000ull };
            LLVMValueRef signBit = LLVMConstIntOfArbitraryPrecision(
                LLVMIntTypeInContext(context_, 128), 2, signWords);
            values.push_back(LLVMBuildXor(builder_, operand, signBit, "decimal.neg"));
            return true;
        }
        values.push_back(isFloating(current->typeKey)
            ? applyFloatingPointMode(LLVMBuildFNeg(builder_, operand, "fneg"))
            : LLVMBuildNeg(builder_, operand, "neg"));
        return true;
    }
    values.push_back(failValue(error, "unsupported unary operation in Stage 34 LLVM lowering"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame002Binary(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Binary)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "binary MIR requires two operands"));
        return true;
    }
    if (lowerShortCircuitBinaryFrame(frame, frames, values)) return true;
    lowerOrdinaryBinaryFrame(frame, frames, values, error);
    return true;
}

bool OopLlvmImplementation::lowerShortCircuitBinaryFrame(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    const bool shortCircuit = current->operation == lexer::TokenKind::AmpAmp ||
        current->operation == lexer::TokenKind::PipePipe;
    if (!shortCircuit) return false;
    if (frame.phase == 0)
    {
        frames.push_back({current, 3, nullptr});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 3)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr || terminated())
        {
            values.push_back(left);
            return true;
        }
        LLVMBasicBlockRef leftBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef rhsBlock = appendBlock("logic.rhs");
        LLVMBasicBlockRef endBlock = appendBlock("logic.end");
        const bool isAnd = current->operation == lexer::TokenKind::AmpAmp;
        LLVMBuildCondBr(
            builder_, left, isAnd ? rhsBlock : endBlock,
            isAnd ? endBlock : rhsBlock);
        LLVMPositionBuilderAtEnd(builder_, rhsBlock);
        frames.push_back(
            {current, 4, nullptr, 0, {}, nullptr, nullptr, leftBlock, endBlock});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMBasicBlockRef rightBlock = LLVMGetInsertBlock(builder_);
    const bool rightReachesEnd = !terminated();
    if (rightReachesEnd) LLVMBuildBr(builder_, frame.endBlock);
    LLVMPositionBuilderAtEnd(builder_, frame.endBlock);
    const bool isAnd = current->operation == lexer::TokenKind::AmpAmp;
    LLVMValueRef shortValue = LLVMConstInt(
        LLVMInt1TypeInContext(context_), isAnd ? 0U : 1U, 0);
    if (!rightReachesEnd)
    {
        values.push_back(shortValue);
        return true;
    }
    LLVMValueRef phi = LLVMBuildPhi(
        builder_, LLVMInt1TypeInContext(context_), "logic");
    LLVMValueRef incomingValues[] = {shortValue, right};
    LLVMBasicBlockRef incomingBlocks[] = {frame.nullBlock, rightBlock};
    LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
    values.push_back(phi);
    return true;
}

void OopLlvmImplementation::lowerOrdinaryBinaryFrame(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr || terminated())
        {
            values.push_back(left);
            return;
        }
        frames.push_back({current, 2, left});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (frame.lhs == nullptr || right == nullptr)
    {
        values.push_back(nullptr);
        return;
    }
    if (terminated())
    {
        values.push_back(right);
        return;
    }
    if (current->operation == lexer::TokenKind::LessLess ||
        current->operation == lexer::TokenKind::GreaterGreater)
    {
        right = normalizeShiftCount(
            right, current->children[1]->typeKey, current->children[0]->typeKey, error);
        if (right == nullptr)
        {
            values.push_back(nullptr);
            return;
        }
    }
    values.push_back(applyBinaryOperation(
        current->operation, frame.lhs, right, current->children[0]->typeKey, error,
        current->booleanValue));
}

bool OopLlvmImplementation::lowerExpressionFrame003FieldAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::FieldAddress ||
        current->kind == frontend::ProjectOopMirNodeKind::BitFieldAddress)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "Macro A field access requires exactly one explicit receiver operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef receiver = values.back();
    values.pop_back();
    if (receiver == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(fieldAddressFromReceiver(current, receiver, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame004FieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::FieldLoad)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "Macro A field access requires exactly one explicit receiver operand"));
            return true;
        }
        const std::string record =
            runtimeDebugFieldLoadRecord(*current, "FIELD-LOAD-BEGIN");
        if (!record.empty() && !emitRuntimeDebugTrace(4U, record, error))
        {
            values.push_back(nullptr);
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef receiver = values.back();
    values.pop_back();
    if (receiver == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef address = fieldAddressFromReceiver(current, receiver, error);
    LLVMTypeRef type = llvmType(current->typeKey, error);
    if (address == nullptr || type == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef load = LLVMBuildLoad2(builder_, type, address, "field.load");
    if (current->volatileAccess) LLVMSetVolatile(load, 1);
    const std::string record =
        runtimeDebugFieldLoadRecord(*current, "FIELD-LOAD-END");
    if (!record.empty() && !emitRuntimeDebugTrace(4U, record, error))
    {
        values.push_back(nullptr);
        return true;
    }
    values.push_back(load);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame005BitFieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::BitFieldLoad)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "Macro A field access requires exactly one explicit receiver operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef receiver = values.back();
    values.pop_back();
    if (receiver == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef address = fieldAddressFromReceiver(current, receiver, error);
    LLVMTypeRef storageType = bitFieldStorageType(current, error);
    if (address == nullptr || storageType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef storage = LLVMBuildLoad2(
        builder_, storageType, address, "bitfield.storage.load");
    LLVMSetAlignment(storage, static_cast<unsigned>(current->alignment));
    if (current->volatileAccess) LLVMSetVolatile(storage, 1);
    values.push_back(extractBitField(current, storage, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame006ArrayElementAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::ArrayElementAddress ||
        current->kind == frontend::ProjectOopMirNodeKind::ArrayElementLoad ||
        current->kind == frontend::ProjectOopMirNodeKind::StringElementLoad ||
        current->kind == frontend::ProjectOopMirNodeKind::StringCharacterPointer)) return false;
    const bool stringValue =
        current->kind == frontend::ProjectOopMirNodeKind::StringElementLoad ||
        current->kind == frontend::ProjectOopMirNodeKind::StringCharacterPointer;
    if (frame.phase == 0)
    {
        if (current->children.size() != 2)
        {
            values.push_back(failValue(error, "Stage 41 array/string index requires receiver and index"));
            return true;
        }
        const std::string record =
            runtimeDebugArrayRecord(*current, "ARRAY-ELEMENT-BEGIN");
        if (!record.empty() && !emitRuntimeDebugTrace(4U, record, error))
        {
            values.push_back(nullptr);
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef object = values.back();
        values.pop_back();
        if (object == nullptr) { values.push_back(nullptr); return true; }
        object = checkedReceiver(object, error);
        if (object == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, object});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef index = values.back();
    values.pop_back();
    if (index == nullptr) { values.push_back(nullptr); return true; }
    index = convert(index, current->children[1]->typeKey, "builtin:nuint", error);
    if (index == nullptr) { values.push_back(nullptr); return true; }
    finishArrayElementFrame(current, frame.lhs, index, stringValue, values, error);
    return true;
}

void OopLlvmImplementation::finishArrayElementFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, bool stringValue,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    LLVMValueRef address = arrayElementAddressFromParts(current, object, index, stringValue, error);
    if (current->kind == frontend::ProjectOopMirNodeKind::ArrayElementAddress ||
        current->kind == frontend::ProjectOopMirNodeKind::StringCharacterPointer)
    {
        const std::string record =
            runtimeDebugArrayRecord(*current, "ARRAY-ELEMENT-END");
        if (address != nullptr && !record.empty() &&
            !emitRuntimeDebugTrace(4U, record, error))
        {
            address = nullptr;
        }
        values.push_back(address);
        return;
    }
    if (address == nullptr) { values.push_back(nullptr); return; }
    if (current->kind == frontend::ProjectOopMirNodeKind::StringElementLoad)
    {
        LLVMValueRef loaded = LLVMBuildLoad2(
            builder_, LLVMInt8TypeInContext(context_), address, "string.element.load");
        const std::string record =
            runtimeDebugArrayRecord(*current, "ARRAY-ELEMENT-END");
        if (!record.empty() && !emitRuntimeDebugTrace(4U, record, error))
            loaded = nullptr;
        values.push_back(loaded);
        return;
    }
    LLVMTypeRef type = llvmType(current->elementTypeKey, error);
    LLVMValueRef loaded = type != nullptr
        ? LLVMBuildLoad2(builder_, type, address, "array.element.load")
        : nullptr;
    const std::string record =
        runtimeDebugArrayRecord(*current, "ARRAY-ELEMENT-END");
    if (loaded != nullptr && !record.empty() &&
        !emitRuntimeDebugTrace(4U, record, error))
    {
        loaded = nullptr;
    }
    values.push_back(loaded);
}

bool OopLlvmImplementation::lowerExpressionFrame007FutureFieldAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::FutureFieldAddress)) return false;
    if (frame.phase == 0)
    {
        if (current->children.empty())
        {
            values.push_back(failValue(error, "future field address has no future receiver"));
            return true;
        }
        frames.push_back({current, 1, nullptr});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef receiver = values.back();
    values.pop_back();
    if (receiver == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(byteOffset(receiver, current->immediate, "future.field.addr"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame008OwnedTemporaryBorrow(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr ||
            !current->localOrdinal.has_value() || !current->cleanupRegionOrdinal.has_value())
        {
            values.push_back(failValue(
                error, "NSC-LIMIT-021 owned temporary borrow is missing value/storage/cleanup identity"));
            return true;
        }
        const auto storage = localStorage_.find(*current->localOrdinal);
        if (storage == localStorage_.end() || storage->second == nullptr)
        {
            values.push_back(failValue(error, "NSC-LIMIT-021 owned temporary borrow has no hidden owner slot"));
            return true;
        }
        frames.push_back({current, 1, storage->second, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMBuildStore(builder_, value, frame.lhs);
    activeCleanupRegions_.push_back(*current->cleanupRegionOrdinal);
    values.push_back(value);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame009FullExpressionCleanup(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::FullExpressionCleanup)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "NSC-LIMIT-021 full expression requires exactly one value"));
            return true;
        }
        frames.push_back({current, 1, nullptr, activeCleanupRegions_.size()});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    std::vector<std::size_t> temporaries(
        activeCleanupRegions_.begin() + static_cast<std::ptrdiff_t>(frame.retainedDepth),
        activeCleanupRegions_.end());
    activeCleanupRegions_.resize(frame.retainedDepth);
    for (auto it = temporaries.rbegin(); it != temporaries.rend(); ++it)
    {
        if (!emitCleanupAction(*it, error)) { values.push_back(nullptr); continue; }
    }
    values.push_back(value);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame010OverflowPolicy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::OverflowPolicy)) return false;
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0, currentOverflowPolicy_});
        currentOverflowPolicy_ = current->stringValue;
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef result = values.back();
    values.pop_back();
    currentOverflowPolicy_ = frame.overflowPolicy;
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame011IteratorGetEnumerator(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorGetEnumerator)) return false;
    if (current->children.size() != 1U)
    {
        values.push_back(failValue(error, "iterator GetEnumerator requires one operand"));
        return true;
    }
    // Transparent wrapper: the child's value IS the enumerator.
    frames.push_back({current->children.front().get(), 0, nullptr});
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame012IteratorCurrent(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorCurrent)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "iterator Current requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->elementTypeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef slot = entryBlockAlloca(
        carrier, structAlignmentFor(current->elementTypeKey), "iterator.current");
    if (slot == nullptr)
    {
        values.push_back(failValue(error, "iterator Current could not reserve storage"));
        return true;
    }
    LLVMBuildStore(builder_, value, slot);
    LLVMTypeRef itemType = llvmType(current->typeKey, error);
    if (itemType == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(LLVMBuildLoad2(
        builder_, itemType, byteOffset(slot, 16, "iterator.item.addr"), "iterator.current.item"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame013IteratorDrop(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorDrop)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "iterator Drop requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef self = values.back();
    values.pop_back();
    if (self == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef dropFn = LLVMBuildLoad2(
        builder_, LLVMPointerTypeInContext(context_, 0),
        byteOffset(self, 32, "iterator.dropfn.addr"), "iterator.dropfn");
    LLVMTypeRef fnParams[] = { LLVMPointerTypeInContext(context_, 0) };
    LLVMTypeRef fnType = LLVMFunctionType(
        LLVMVoidTypeInContext(context_), fnParams, 1, 0);
    LLVMValueRef args[] = { self };
    if (LLVMBuildCall2(builder_, fnType, dropFn, args, 1U, "") == nullptr)
    {
        values.push_back(failValue(error, "iterator Drop call failed"));
        return true;
    }
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame014IteratorCancel(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorCancel)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "iterator Cancel requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef self = values.back();
    values.pop_back();
    if (self == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef intType = llvmType("builtin:int", error);
    if (intType == nullptr) { values.push_back(nullptr); return true; }
    LLVMBuildStore(
        builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-3), 1), self);
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame015IteratorMoveNextAsync(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorMoveNextAsync)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "iterator MoveNextAsync requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef self = values.back();
    values.pop_back();
    if (self == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef moveNextFn = LLVMBuildLoad2(
        builder_, LLVMPointerTypeInContext(context_, 0),
        byteOffset(self, 24, "moveNextAsync.fn.addr"), "moveNextAsync.fn");
    LLVMTypeRef futureType = llvmType(current->typeKey, error);
    if (futureType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef sretSlot = entryBlockAlloca(futureType, 8, "moveNextAsync.sret");
    LLVMTypeRef fnParams[] = {
        LLVMPointerTypeInContext(context_, 0),
        LLVMPointerTypeInContext(context_, 0) };
    LLVMTypeRef fnType = LLVMFunctionType(LLVMVoidTypeInContext(context_), fnParams, 2, 0);
    LLVMValueRef args[] = { sretSlot, self };
    if (LLVMBuildCall2(builder_, fnType, moveNextFn, args, 2U, "") == nullptr)
    {
        values.push_back(failValue(error, "iterator MoveNextAsync call failed"));
        return true;
    }
    values.push_back(LLVMBuildLoad2(builder_, futureType, sretSlot, "moveNextAsync.future.load"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame016IteratorMoveNext(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::IteratorMoveNext)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "iterator MoveNext requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef self = values.back();
    values.pop_back();
    if (self == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef moveNextFn = LLVMBuildLoad2(
        builder_, LLVMPointerTypeInContext(context_, 0),
        byteOffset(self, 24, "iterator.movenextfn.addr"), "iterator.movenextfn");
    LLVMTypeRef fnParams[] = { LLVMPointerTypeInContext(context_, 0) };
    LLVMTypeRef fnType = LLVMFunctionType(
        LLVMInt1TypeInContext(context_), fnParams, 1, 0);
    LLVMValueRef args[] = { self };
    LLVMValueRef result = LLVMBuildCall2(
        builder_, fnType, moveNextFn, args, 1U, "iterator.movenext.call");
    // NS304-T06-F07: if the MoveNext errored (state == -2), rethrow the carried error
    // into the enclosing catch instead of ending the loop normally.
    const std::optional<std::string_view> errorInner =
        language::iteratorErrorTypeKey(current->elementTypeKey);
    if (errorInner.has_value() && current->exceptionTargetRegionOrdinal.has_value())
    {
        LLVMTypeRef intType = llvmType("builtin:int", error);
        LLVMValueRef state = LLVMBuildLoad2(
            builder_, intType, self, "iterator.movenext.state");
        LLVMValueRef erroredConst = LLVMConstInt(
            intType, static_cast<unsigned long long>(-2), 1);
        LLVMBasicBlockRef normalBlock = appendBlock("iterator.movenext.normal");
        LLVMBasicBlockRef errorBlock = appendBlock("iterator.movenext.error");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntEQ, state, erroredConst, "iterator.movenext.errored"),
            errorBlock, normalBlock);
        LLVMPositionBuilderAtEnd(builder_, errorBlock);
        std::uint64_t errorOffset = 0;
        if (const frontend::ProjectOopMirStructLayout* layout =
                findStructLayout(mirModel_, current->elementTypeKey))
        {
            for (const frontend::ObjectFieldLayout& field : layout->fields)
                if (field.memberOrdinal == 5) { errorOffset = field.offset; break; }
        }
        LLVMTypeRef errorType = llvmType(std::string(*errorInner), error);
        if (errorType == nullptr || errorOffset == 0)
        {
            values.push_back(failValue(error, "iterator MoveNext error propagation has no error slot"));
            return true;
        }
        LLVMValueRef errorValue = LLVMBuildLoad2(
            builder_, errorType,
            byteOffset(self, errorOffset, "iterator.movenext.error.addr"), "iterator.movenext.error");
        if (!lowerExceptionalTransfer(errorValue, current, error))
        {
            values.push_back(nullptr);
            return true;
        }
        LLVMPositionBuilderAtEnd(builder_, normalBlock);
    }
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame017BorrowValue(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::BorrowValue)) return false;
    if (current->children.size() != 1 || current->children.front() == nullptr)
    {
        values.push_back(failValue(error, "Stage 50.8 borrow value must wrap exactly one physical reference value"));
        return true;
    }
    // Transparent wrapper: the child's value IS the borrow value.
    frames.push_back({current->children.front().get(), 0, nullptr});
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame018DirectInstanceCall(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::DirectInstanceCall ||
        current->kind == frontend::ProjectOopMirNodeKind::VirtualInstanceCall ||
        current->kind == frontend::ProjectOopMirNodeKind::InterfaceInstanceCall)) return false;
    if (current->nullConditionalCall)
    {
        values.push_back(lowerNullConditionalCall(current, error));
        return true;
    }
    if (current->children.empty())
    {
        values.push_back(failValue(error, "Macro A instance call is missing its explicit receiver operand"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        const std::size_t argumentCount = current->children.size() - 1;
        if (argumentCount == 0)
        {
            std::vector<LLVMValueRef> noArgs;
            if (current->kind == frontend::ProjectOopMirNodeKind::DirectInstanceCall)
                values.push_back(lowerCall(current, error, receiver, &noArgs));
            else if (current->kind == frontend::ProjectOopMirNodeKind::VirtualInstanceCall)
                values.push_back(lowerVirtualCall(current, error, receiver, &noArgs));
            else
                values.push_back(lowerInterfaceCall(current, error, receiver, &noArgs));
            return true;
        }
        frames.push_back({current, 2, receiver, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    const std::size_t argumentCount = current->children.size() - 1;
    if (frame.retainedDepth + 1 < argumentCount)
    {
        const std::size_t nextIndex = frame.retainedDepth + 1;
        frames.push_back({current, 2, frame.lhs, nextIndex});
        frames.push_back({current->children[nextIndex + 1].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(argumentCount);
    for (std::size_t i = argumentCount; i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    if (current->kind == frontend::ProjectOopMirNodeKind::DirectInstanceCall)
        values.push_back(lowerCall(current, error, frame.lhs, &argumentOverrides));
    else if (current->kind == frontend::ProjectOopMirNodeKind::VirtualInstanceCall)
        values.push_back(lowerVirtualCall(current, error, frame.lhs, &argumentOverrides));
    else
        values.push_back(lowerInterfaceCall(current, error, frame.lhs, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame019DelegateInvoke(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::DelegateInvoke ||
        current->kind == frontend::ProjectOopMirNodeKind::EventInvoke)) return false;
    if (current->children.empty())
    {
        values.push_back(failValue(error, "Stage 42 delegate/event invocation has invalid MIR children"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        const std::size_t argumentCount = current->children.size() - 1;
        if (argumentCount == 0)
        {
            std::vector<LLVMValueRef> noArgs;
            values.push_back(current->kind == frontend::ProjectOopMirNodeKind::DelegateInvoke
                ? lowerDelegateInvoke(current, error, receiver, &noArgs)
                : lowerEventInvoke(current, error, receiver, &noArgs));
            return true;
        }
        frames.push_back({current, 2, receiver, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    const std::size_t argumentCount = current->children.size() - 1;
    if (frame.retainedDepth + 1 < argumentCount)
    {
        const std::size_t nextIndex = frame.retainedDepth + 1;
        frames.push_back({current, 2, frame.lhs, nextIndex});
        frames.push_back({current->children[nextIndex + 1].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(argumentCount);
    for (std::size_t i = argumentCount; i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(current->kind == frontend::ProjectOopMirNodeKind::DelegateInvoke
        ? lowerDelegateInvoke(current, error, frame.lhs, &argumentOverrides)
        : lowerEventInvoke(current, error, frame.lhs, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame020DirectStaticCall(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::DirectStaticCall)) return false;
    if (current->booleanValue)
    {
        values.push_back(lowerDirectClosureCall(current, error));
        return true;
    }
    if (frame.phase < current->children.size())
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(current->children.size());
    for (std::size_t i = current->children.size(); i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerStaticCall(current, error, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame021ObjectDestroy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::ObjectDestroy)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "item 319 object destruction requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef object = values.back();
    values.pop_back();
    if (object == nullptr) { values.push_back(nullptr); return true; }
    if (!current->dynamicDestruction)
    {
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
        return true;
    }
    if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
    {
        values.push_back(failValue(error, "item 319 object destruction has no descriptor ABI"));
        return true;
    }
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
    LLVMValueRef isNull = LLVMBuildICmp(
        builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "destroy.object.isnull");
    LLVMBasicBlockRef destroyBlock = appendBlock("destroy.object");
    LLVMBasicBlockRef doneBlock = appendBlock("destroy.object.done");
    LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
    LLVMPositionBuilderAtEnd(builder_, destroyBlock);
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
        values.push_back(nullptr);
        return true;
    }
    if (!terminated()) LLVMBuildBr(builder_, doneBlock);
    LLVMPositionBuilderAtEnd(builder_, doneBlock);
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame022Deallocate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Deallocate)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "Stage 35 deallocation MIR requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef operand = values.back();
    values.pop_back();
    if (operand == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(lowerDeallocation(current, error, operand));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame023ValueConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::ValueConstruct)) return false;
    if (frame.phase < current->argumentCount)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(current->argumentCount);
    for (std::size_t i = current->argumentCount; i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerValueConstruction(current, error, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame024AllocateAndConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::AllocateAndConstruct)) return false;
    if (frame.phase < current->argumentCount)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(current->argumentCount);
    for (std::size_t i = current->argumentCount; i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerConstruction(current, error, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame025PlacementConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::PlacementConstruct)) return false;
    if (current->children.empty() || current->children.front() == nullptr)
    {
        values.push_back(failValue(error, "item 318 placement construction requires destination storage"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef storage = values.back();
        values.pop_back();
        if (storage == nullptr) { values.push_back(nullptr); return true; }
        const std::size_t argumentCount = current->argumentCount;
        if (argumentCount == 0)
        {
            std::vector<LLVMValueRef> noArgs;
            values.push_back(lowerPlacementConstruction(current, error, storage, &noArgs));
            return true;
        }
        frames.push_back({current, 2, storage, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    const std::size_t argumentCount = current->argumentCount;
    if (frame.retainedDepth + 1 < argumentCount)
    {
        const std::size_t nextIndex = frame.retainedDepth + 1;
        frames.push_back({current, 2, frame.lhs, nextIndex});
        frames.push_back({current->children[nextIndex + 1].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(argumentCount);
    for (std::size_t i = argumentCount; i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerPlacementConstruction(current, error, frame.lhs, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame026AwaitFuture(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::AwaitFuture)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U)
        {
            values.push_back(failValue(error, "await future requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef child = values.back();
    values.pop_back();
    if (child == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(lowerAwaitExpression(current, error, child));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame027AggregateInitialize(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::AggregateInitialize)) return false;
    if (frame.phase < current->children.size())
    {
        const std::size_t index = frame.phase;
        const auto& child = current->children[index];
        if (child == nullptr || child->children.empty())
        {
            values.push_back(failValue(error, "C7 aggregate initializer has a malformed element"));
            return true;
        }
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({child->children.front().get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> fieldValues(current->children.size());
    for (std::size_t i = current->children.size(); i > 0; --i)
    {
        fieldValues[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerAggregateInitializer(current, error, &fieldValues));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame028AddressValue(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::AddressValue)) return false;
    if (current->children.size() != 1 || current->children.front() == nullptr)
    {
        values.push_back(failValue(error, "address-of requires exactly one addressable operand"));
        return true;
    }
    // Transparent wrapper: the child's value IS the address.
    frames.push_back({current->children.front().get(), 0, nullptr});
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame029MoveArrayElement(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::MoveArrayElement)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "Stage 52 move array element requires exactly one address operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef address = values.back();
    values.pop_back();
    if (address == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef elementType = llvmType(current->typeKey, error);
    if (elementType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef value = LLVMBuildLoad2(builder_, elementType, address, "move.array.element.load");
    if (current->booleanValue)
    {
        LLVMBuildStore(builder_, LLVMConstNull(elementType), address);
    }
    values.push_back(value);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame030PointerLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::PointerLoad)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "pointer load requires exactly one pointer operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef pointer = values.back();
    values.pop_back();
    if (pointer == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef pointee = llvmType(current->typeKey, error);
    if (pointee == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(LLVMBuildLoad2(builder_, pointee, pointer, "pointer.load"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame031MoveOwnedField(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::MoveOwnedField)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "Bloque 1 move owned field requires exactly one address operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef address = values.back();
    values.pop_back();
    if (address == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef fieldType = llvmType(current->typeKey, error);
    if (fieldType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef value = LLVMBuildLoad2(builder_, fieldType, address, "move.owned.field.load");
    if (current->booleanValue)
    {
        LLVMBuildStore(builder_, LLVMConstNull(fieldType), address);
    }
    values.push_back(value);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame032MoveIteratorCurrent(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::MoveIteratorCurrent)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "move iterator Current requires exactly one iterator operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef iterator = values.back();
    values.pop_back();
    if (iterator == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef itemType = llvmType(current->typeKey, error);
    if (itemType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef slot = byteOffset(iterator, 16, "move.iterator.item.addr");
    LLVMValueRef value = LLVMBuildLoad2(builder_, itemType, slot, "move.iterator.item.load");
    if (current->booleanValue)
    {
        LLVMBuildStore(builder_, LLVMConstNull(itemType), slot);
    }
    values.push_back(value);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame033RawElementAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 2 || current->immediate == 0)
        {
            values.push_back(failValue(
                error, "C2.1 raw element address requires a base, an index and an element size"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef base = values.back();
        values.pop_back();
        if (base == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, base, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef index = values.back();
    values.pop_back();
    if (index == nullptr) { values.push_back(nullptr); return true; }
    index = normalizeNativeIndexFromValue(current->children[1].get(), index, error);
    if (index == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(rawElementAddressFromParts(current, frame.lhs, index, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame034PointerArithmetic(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::PointerArithmetic)) return false;
    if (current->children.size() != 2 || current->immediate == 0)
    {
        values.push_back(failValue(error, "CC1 pointer arithmetic requires two operands and an element size"));
        return true;
    }
    if (current->children[0] == nullptr || current->children[1] == nullptr)
    {
        values.push_back(failValue(error, "CC1 pointer arithmetic has a null operand"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, left, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(pointerArithmeticFromParts(current, frame.lhs, right, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame035CompoundStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CompoundStore)) return false;
    if (current->children.size() != 2 || current->children[0] == nullptr ||
        current->children[1] == nullptr)
    {
        values.push_back(failValue(error, "compound store requires a target address and a value"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef address = values.back();
        values.pop_back();
        if (address == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, address, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(compoundStoreFromParts(current, frame.lhs, value, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame036StackAllocation(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::StackAllocation)) return false;
    if (current->children.empty() || current->immediate == 0)
    {
        values.push_back(failValue(error, "stack allocation requires extents and an element size"));
        return true;
    }
    if (frame.phase < current->children.size())
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> rawExtents(current->children.size());
    for (std::size_t i = current->children.size(); i > 0; --i)
    {
        rawExtents[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(stackAllocationFromParts(current, rawExtents, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame037NullConditionalFieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::NullConditionalFieldLoad)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "null-conditional field MIR requires one receiver"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef receiver = values.back();
    values.pop_back();
    if (receiver == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(lowerNullConditionalField(current, error, receiver));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame038NullConditionalArrayLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::NullConditionalArrayLoad)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "null-conditional array load MIR requires receiver and index (got " +
            std::to_string(current->children.size()) + ")"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        LLVMTypeRef resultType = nullptr;
        LLVMBasicBlockRef nullBlock = nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        if (!prepareNullConditionalArrayLoad(
                current, receiver, error, resultType, nullBlock, endBlock))
        {
            values.push_back(nullptr);
            return true;
        }
        frames.push_back({
            current, 2, receiver, 0, {}, nullptr, resultType, nullBlock, endBlock});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef rawIndex = values.back();
    values.pop_back();
    if (rawIndex == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef index = normalizeNativeIndexFromValue(
        current->children[1].get(), rawIndex, error);
    if (index == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(finishNullConditionalArrayLoad(
        current, frame.lhs, index, frame.resultType,
        frame.nullBlock, frame.endBlock, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame039NullConditionalFieldStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::NullConditionalFieldStore)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "null-conditional field store MIR requires receiver and value"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        LLVMValueRef address = nullptr;
        LLVMValueRef currentValue = nullptr;
        LLVMTypeRef resultType = nullptr;
        LLVMBasicBlockRef nullBlock = nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        if (!prepareNullConditionalFieldStore(
                current, receiver, error, address, currentValue, resultType,
                nullBlock, endBlock))
        {
            values.push_back(nullptr);
            return true;
        }
        frames.push_back({
            current, 2, address, 0, {}, currentValue, resultType, nullBlock, endBlock});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(finishNullConditionalFieldStore(
        current, frame.lhs, frame.auxiliary, value, frame.resultType,
        frame.nullBlock, frame.endBlock, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame040NullConditionalArrayStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::NullConditionalArrayStore)) return false;
    if (current->children.size() != 3)
    {
        values.push_back(failValue(error, "null-conditional array store MIR requires receiver, index, value"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        LLVMTypeRef resultType = nullptr;
        LLVMBasicBlockRef nullBlock = nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        if (!prepareNullConditionalArrayStore(
                current, receiver, error, resultType, nullBlock, endBlock))
        {
            values.push_back(nullptr);
            return true;
        }
        frames.push_back({
            current, 2, receiver, 0, {}, nullptr, resultType, nullBlock, endBlock});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 2)
    {
        LLVMValueRef rawIndex = values.back();
        values.pop_back();
        if (rawIndex == nullptr) { values.push_back(nullptr); return true; }
        LLVMValueRef index = normalizeNativeIndexFromValue(
            current->children[1].get(), rawIndex, error);
        if (index == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({
            current, 3, frame.lhs, 0, {}, index, frame.resultType,
            frame.nullBlock, frame.endBlock});
        frames.push_back({current->children[2].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(finishNullConditionalArrayStore(
        current, frame.lhs, frame.auxiliary, value, frame.resultType,
        frame.nullBlock, frame.endBlock, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame041Convert(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Convert)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "convert MIR requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    if (terminated()) { values.push_back(value); return true; }
    values.push_back(convert(value, current->children.front()->typeKey, current->typeKey, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame042Assert(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Assert ||
        current->kind == frontend::ProjectOopMirNodeKind::Assume)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U || current->typeKey != "builtin:void")
        {
            values.push_back(failValue(error, "T07-F06 assertion MIR lost its bool/void contract"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef condition = values.back();
    values.pop_back();
    if (condition == nullptr || !error.empty()) { values.push_back(nullptr); return true; }
    if (current->kind == frontend::ProjectOopMirNodeKind::Assert)
    {
        if (!emitGuard(condition, LlvmTrapReason::AssertionFailed, "assert", error))
        {
            values.push_back(nullptr);
            return true;
        }
    }
    else
    {
        if (assumeFunction_ == nullptr || assumeFunctionType_ == nullptr)
        {
            values.push_back(failValue(error, "T07-F06 llvm.assume intrinsic is unavailable"));
            return true;
        }
        LLVMValueRef arguments[] = {condition};
        buildNoUnwindCall(assumeFunctionType_, assumeFunction_, arguments, 1, "");
    }
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame043ResultPropagation(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::ResultPropagation)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1U || current->children.front() == nullptr)
        {
            values.push_back(failValue(error, "T07-F01 Result propagation MIR is incomplete"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef carrier = values.back();
    values.pop_back();
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    values.push_back(lowerResultPropagation(current, error, carrier));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame044Store(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::Store)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "store MIR requires target and value"));
        return true;
    }
    if (frame.phase == 0)
    {
        const std::string fieldRecord =
            runtimeDebugFieldStoreRecord(*current, "FIELD-STORE-BEGIN");
        if (!fieldRecord.empty() &&
            !emitRuntimeDebugTrace(4U, fieldRecord, error))
        {
            values.push_back(nullptr);
            return true;
        }
        if (current->children[0]->kind ==
            frontend::ProjectOopMirNodeKind::ArrayElementAddress)
        {
            const std::string arrayRecord = runtimeDebugArrayRecord(
                *current->children[0], "ARRAY-STORE-BEGIN");
            if (!arrayRecord.empty() &&
                !emitRuntimeDebugTrace(4U, arrayRecord, error))
            {
                values.push_back(nullptr);
                return true;
            }
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef address = values.back();
        values.pop_back();
        if (address == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, address, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef stored = storeFromParts(current, frame.lhs, value, error);
    if (stored != nullptr)
    {
        const std::string fieldRecord =
            runtimeDebugFieldStoreRecord(*current, "FIELD-STORE-END");
        if (!fieldRecord.empty() &&
            !emitRuntimeDebugTrace(4U, fieldRecord, error))
        {
            stored = nullptr;
        }
        if (stored != nullptr && current->children[0]->kind ==
            frontend::ProjectOopMirNodeKind::ArrayElementAddress)
        {
            const std::string arrayRecord = runtimeDebugArrayRecord(
                *current->children[0], "ARRAY-STORE-END");
            if (!arrayRecord.empty() &&
                !emitRuntimeDebugTrace(4U, arrayRecord, error))
            {
                stored = nullptr;
            }
        }
    }
    values.push_back(stored);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame045TupleDeconstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::TupleDeconstruct)) return false;
    if (current->children.empty())
    {
        values.push_back(failValue(error, "tuple deconstruction has no source value"));
        return true;
    }
    for (std::size_t index = 1; index < current->children.size(); ++index)
    {
        const frontend::ProjectOopMirNode* element = current->children[index].get();
        if (element == nullptr ||
            element->kind != frontend::ProjectOopMirNodeKind::TupleDeconstructElementStore ||
            element->children.size() != 1U)
        {
            values.push_back(failValue(error, "tuple deconstruction has a malformed target descriptor"));
            continue;
        }
    }
    const std::size_t operandCount = current->children.size();
    if (frame.phase < operandCount)
    {
        const std::size_t index = frame.phase;
        const frontend::ProjectOopMirNode* operand = index == 0
            ? current->children[0].get()
            : current->children[index]->children.front().get();
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({operand, 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> targetAddresses(operandCount - 1);
    for (std::size_t i = operandCount - 1; i > 0; --i)
    {
        targetAddresses[i - 1] = values.back();
        values.pop_back();
    }
    LLVMValueRef sourceValue = values.back();
    values.pop_back();
    bool allValid = sourceValue != nullptr;
    for (const LLVMValueRef addr : targetAddresses)
    {
        if (addr == nullptr) allValid = false;
    }
    if (!allValid) { values.push_back(nullptr); return true; }
    values.push_back(tupleDeconstructFromParts(current, sourceValue, targetAddresses, error));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame046ParameterPack(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::ParameterPack)) return false;
    if (frame.phase < current->children.size())
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    std::vector<LLVMValueRef> argumentOverrides(current->children.size());
    for (std::size_t i = current->children.size(); i > 0; --i)
    {
        argumentOverrides[i - 1] = values.back();
        values.pop_back();
    }
    values.push_back(lowerParameterPack(current, error, &argumentOverrides));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame047CoreConsoleWriteLine(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreConsoleWriteLine)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || consoleWriteLineFunction_ == nullptr)
        {
            values.push_back(failValue(error, "Console write requires one string argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef mode = LLVMConstInt(
        LLVMInt32TypeInContext(context_), static_cast<unsigned long long>(current->immediate), 0);
    LLVMValueRef args[] = {value, mode};
    buildNoUnwindCall(consoleWriteLineFunctionType_, consoleWriteLineFunction_, args, 2, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame048CoreConsoleReadLine(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreConsoleReadLine)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || consoleReadLineFunction_ == nullptr)
        {
            values.push_back(failValue(error, "Console.ReadLine requires one out status and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef status = values.back();
    values.pop_back();
    if (status == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {status};
    values.push_back(buildNoUnwindCall(
        consoleReadLineFunctionType_, consoleReadLineFunction_, args, 1, "console.readline"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame049CoreEnvironmentExit(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreEnvironmentExit)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || environmentExitFunction_ == nullptr)
        {
            values.push_back(failValue(
                error, "Stage 53.2 Environment.Exit requires one int argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {value};
    buildNoUnwindCall(environmentExitFunctionType_, environmentExitFunction_, args, 1, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame050CoreEnvironmentGetVariable(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreEnvironmentGetVariable)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || environmentGetVariableFunction_ == nullptr)
        {
            values.push_back(failValue(
                error,
                "Stage 53.2b Environment.GetEnvironmentVariable requires one string argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef name = values.back();
    values.pop_back();
    if (name == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {name};
    values.push_back(buildNoUnwindCall(
        environmentGetVariableFunctionType_, environmentGetVariableFunction_, args, 1, "environment.getvariable"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame051CoreEnvironmentSetVariable(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreEnvironmentSetVariable)) return false;
    if (current->children.size() != 2 || environmentSetVariableFunction_ == nullptr)
    {
        values.push_back(failValue(
            error,
            "Stage 53.2b Environment.SetEnvironmentVariable requires two string arguments and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef name = values.back();
        values.pop_back();
        if (name == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, name, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, value};
    buildNoUnwindCall(environmentSetVariableFunctionType_, environmentSetVariableFunction_, args, 2, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame052CoreFileExists(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileExists ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryExists ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreFileDelete ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryCreate)) return false;
    if (frame.phase == 0)
    {
        const bool exists = current->kind == frontend::ProjectOopMirNodeKind::CoreFileExists;
        const bool dirExists = current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryExists;
        const bool del = current->kind == frontend::ProjectOopMirNodeKind::CoreFileDelete;
        if (current->children.size() != 1 ||
            (exists && fileExistsFunction_ == nullptr) ||
            (dirExists && directoryExistsFunction_ == nullptr) ||
            (del && fileDeleteFunction_ == nullptr) ||
            (current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryCreate &&
                directoryCreateFunction_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 53.3 filesystem intrinsic requires one string argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef path = values.back();
    values.pop_back();
    if (path == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {path};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreFileExists)
        values.push_back(buildNoUnwindCall(fileExistsFunctionType_, fileExistsFunction_, args, 1, "file.exists"));
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryExists)
        values.push_back(buildNoUnwindCall(
            directoryExistsFunctionType_, directoryExistsFunction_, args, 1, "directory.exists"));
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreFileDelete)
    {
        buildNoUnwindCall(fileDeleteFunctionType_, fileDeleteFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else
    {
        buildNoUnwindCall(directoryCreateFunctionType_, directoryCreateFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame053CoreFileReadAllText(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileReadAllText ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreFileGetSize ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreFileReadAllBytes)) return false;
    if (frame.phase == 0)
    {
        const bool readText = current->kind == frontend::ProjectOopMirNodeKind::CoreFileReadAllText;
        const bool getSize = current->kind == frontend::ProjectOopMirNodeKind::CoreFileGetSize;
        if (current->children.size() != 1 ||
            (readText && fileReadAllTextFunction_ == nullptr) ||
            (getSize && fileGetSizeFunction_ == nullptr) ||
            (!readText && !getSize && fileReadAllBytesFunction_ == nullptr))
        {
            values.push_back(failValue(
                error, "Bloque 4 file read intrinsic requires one string argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef path = values.back();
    values.pop_back();
    if (path == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {path};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreFileReadAllText)
        values.push_back(buildNoUnwindCall(
            fileReadAllTextFunctionType_, fileReadAllTextFunction_, args, 1, "file.readalltext"));
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreFileGetSize)
        values.push_back(buildNoUnwindCall(
            fileGetSizeFunctionType_, fileGetSizeFunction_, args, 1, "file.getsize"));
    else
        values.push_back(buildNoUnwindCall(
            fileReadAllBytesFunctionType_, fileReadAllBytesFunction_, args, 1, "file.readallbytes"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame054CoreFileReadBytes(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileReadBytes)) return false;
    if (current->children.size() != 2 || fileReadBytesFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Bloque 4 File.ReadBytes requires a path and a byte[] argument and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef path = values.back();
        values.pop_back();
        if (path == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, path, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef destination = values.back();
    values.pop_back();
    if (destination == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, destination};
    values.push_back(buildNoUnwindCall(
        fileReadBytesFunctionType_, fileReadBytesFunction_, args, 2, "file.readbytes"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame055CoreFileWriteAllBytes(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileWriteAllBytes)) return false;
    if (current->children.size() != 3 || fileWriteAllBytesFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Bloque 4 File.WriteAllBytes requires a path, a byte[] and a count, plus the runtime hook"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef count = values.back();
    values.pop_back();
    LLVMValueRef data = values.back();
    values.pop_back();
    LLVMValueRef path = values.back();
    values.pop_back();
    if (path == nullptr || data == nullptr || count == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMValueRef args[] = {path, data, count};
    values.push_back(buildNoUnwindCall(
        fileWriteAllBytesFunctionType_, fileWriteAllBytesFunction_, args, 3, "file.writeallbytes"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame056CoreFileWriteAllText(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileWriteAllText ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreFileAppendAllText)) return false;
    const bool append = current->kind == frontend::ProjectOopMirNodeKind::CoreFileAppendAllText;
    if (current->children.size() != 2 ||
        (!append && fileWriteAllTextFunction_ == nullptr) ||
        (append && fileAppendAllTextFunction_ == nullptr))
    {
        values.push_back(failValue(
            error, "Stage 53.3b File text write intrinsic requires two string arguments and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef path = values.back();
        values.pop_back();
        if (path == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, path, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef content = values.back();
    values.pop_back();
    if (content == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, content};
    if (append)
        buildNoUnwindCall(fileAppendAllTextFunctionType_, fileAppendAllTextFunction_, args, 2, "");
    else
        buildNoUnwindCall(fileWriteAllTextFunctionType_, fileWriteAllTextFunction_, args, 2, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame057CoreDirectoryDelete(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryDelete ||
        current->kind == frontend::ProjectOopMirNodeKind::CorePathGetFullPath ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount)) return false;
    if (frame.phase == 0)
    {
        const bool del = current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryDelete;
        const bool fullPath = current->kind == frontend::ProjectOopMirNodeKind::CorePathGetFullPath;
        const bool fileCount = current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount;
        if (current->children.size() != 1 ||
            (del && directoryDeleteFunction_ == nullptr) ||
            (fullPath && pathGetFullPathFunction_ == nullptr) ||
            (fileCount && directoryGetFileCountFunction_ == nullptr) ||
            (!del && !fullPath && !fileCount && directoryGetDirectoryCountFunction_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 53 directory/path intrinsic requires one string argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef path = values.back();
    values.pop_back();
    if (path == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {path};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryDelete)
    {
        buildNoUnwindCall(directoryDeleteFunctionType_, directoryDeleteFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else if (current->kind == frontend::ProjectOopMirNodeKind::CorePathGetFullPath)
        values.push_back(buildNoUnwindCall(
            pathGetFullPathFunctionType_, pathGetFullPathFunction_, args, 1, "path.getfullpath"));
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetFileCount)
        values.push_back(buildNoUnwindCall(
            directoryGetFileCountFunctionType_, directoryGetFileCountFunction_, args, 1, "directory.getfilecount"));
    else
        values.push_back(buildNoUnwindCall(
            directoryGetDirectoryCountFunctionType_,
            directoryGetDirectoryCountFunction_, args, 1, "directory.getdirectorycount"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame058CoreEnvironmentGetCommandLineArgs(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || environmentGetCommandLineArgsFunction_ == nullptr)
        {
            values.push_back(failValue(
                error,
                "Stage 53.5 Environment.GetCommandLineArgs requires one string[] argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef destination = values.back();
    values.pop_back();
    if (destination == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {destination};
    values.push_back(buildNoUnwindCall(
        environmentGetCommandLineArgsFunctionType_, environmentGetCommandLineArgsFunction_,
        args, 1, "environment.getcommandlineargs"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame059CoreDirectoryGetFiles(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetFiles ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetDirectories)) return false;
    const bool files = current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryGetFiles;
    if (current->children.size() != 2 ||
        (files && directoryGetFilesFunction_ == nullptr) ||
        (!files && directoryGetDirectoriesFunction_ == nullptr))
    {
        values.push_back(failValue(
            error,
            "Stage 53.6 Directory enumeration requires a path and a string[] argument and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef path = values.back();
        values.pop_back();
        if (path == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, path, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef destination = values.back();
    values.pop_back();
    if (destination == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, destination};
    if (files)
        values.push_back(buildNoUnwindCall(
            directoryGetFilesFunctionType_, directoryGetFilesFunction_, args, 2, "directory.getfiles"));
    else
        values.push_back(buildNoUnwindCall(
            directoryGetDirectoriesFunctionType_,
            directoryGetDirectoriesFunction_, args, 2, "directory.getdirectories"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame060CoreThreadCreate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreThreadCreate)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 || threadCreateFunction_ == nullptr)
        {
            values.push_back(failValue(
                error, "Stage 54.1 Thread.ThreadCreate requires one delegate argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef delegateCell = values.back();
    values.pop_back();
    if (delegateCell == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {delegateCell};
    values.push_back(buildNoUnwindCall(
        threadCreateFunctionType_, threadCreateFunction_, args, 1, "thread.create"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame061CoreThreadJoin(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreThreadJoin ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreThreadDetach)) return false;
    const bool join = current->kind == frontend::ProjectOopMirNodeKind::CoreThreadJoin;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1 ||
            (join && threadJoinFunction_ == nullptr) ||
            (!join && threadDetachFunction_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.1 Thread join/detach requires one nint argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef handle = values.back();
    values.pop_back();
    if (handle == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {handle};
    if (join)
        buildNoUnwindCall(threadJoinFunctionType_, threadJoinFunction_, args, 1, "");
    else
        buildNoUnwindCall(threadDetachFunctionType_, threadDetachFunction_, args, 1, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame062CoreMutexCreateNamed(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreMutexCreateNamed ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMutexLock ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMutexUnlock ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMutexClose)) return false;
    if (frame.phase == 0)
    {
        const bool createNamed = current->kind == frontend::ProjectOopMirNodeKind::CoreMutexCreateNamed;
        const bool lock = current->kind == frontend::ProjectOopMirNodeKind::CoreMutexLock;
        const bool unlock = current->kind == frontend::ProjectOopMirNodeKind::CoreMutexUnlock;
        if (current->children.size() != 1 ||
            (createNamed && mutexCreateNamedFunction_ == nullptr) ||
            (lock && mutexLockFunction_ == nullptr) ||
            (unlock && mutexUnlockFunction_ == nullptr) ||
            (!createNamed && !lock && !unlock && mutexCloseFunction_ == nullptr))
        {
            values.push_back(failValue(error, "Stage 54.3 Mutex intrinsic requires one argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef operand = values.back();
    values.pop_back();
    if (operand == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {operand};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreMutexCreateNamed)
        values.push_back(buildNoUnwindCall(
            mutexCreateNamedFunctionType_, mutexCreateNamedFunction_, args, 1, "mutex.create.named"));
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreMutexLock)
    {
        buildNoUnwindCall(mutexLockFunctionType_, mutexLockFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreMutexUnlock)
    {
        buildNoUnwindCall(mutexUnlockFunctionType_, mutexUnlockFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else
    {
        buildNoUnwindCall(mutexCloseFunctionType_, mutexCloseFunction_, args, 1, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame063CoreMonitorEnter(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorEnter ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorExit ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorPulse ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorPulseAll)) return false;
    if (frame.phase == 0)
    {
        const bool enter = current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorEnter;
        const bool exit = current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorExit;
        const bool pulse = current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorPulse;
        if (current->children.size() != 1 ||
            (enter && monitorEnterFunction_ == nullptr) ||
            (exit && monitorExitFunction_ == nullptr) ||
            (pulse && monitorPulseFunction_ == nullptr) ||
            (!enter && !exit && !pulse && monitorPulseAllFunction_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.4 Monitor intrinsic requires one pointer argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef pointer = values.back();
    values.pop_back();
    if (pointer == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {pointer};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorEnter)
        buildNoUnwindCall(monitorEnterFunctionType_, monitorEnterFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorExit)
        buildNoUnwindCall(monitorExitFunctionType_, monitorExitFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorPulse)
        buildNoUnwindCall(monitorPulseFunctionType_, monitorPulseFunction_, args, 1, "");
    else
        buildNoUnwindCall(monitorPulseAllFunctionType_, monitorPulseAllFunction_, args, 1, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame064CoreSemaphoreRelease(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreClose ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventSet ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventReset ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventClose)) return false;
    if (frame.phase == 0)
    {
        const bool semRelease = current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease;
        const bool semClose = current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreClose;
        const bool evSet = current->kind == frontend::ProjectOopMirNodeKind::CoreEventSet;
        const bool evReset = current->kind == frontend::ProjectOopMirNodeKind::CoreEventReset;
        if (current->children.size() != 1 ||
            (semRelease && semaphoreReleaseFunction_ == nullptr) ||
            (semClose && semaphoreCloseFunction_ == nullptr) ||
            (evSet && eventSetFunction_ == nullptr) ||
            (evReset && eventResetFunction_ == nullptr) ||
            (!semRelease && !semClose && !evSet && !evReset && eventCloseFunction_ == nullptr))
        {
            values.push_back(failValue(error, "Stage 54.5 sync primitive requires one nint argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef handle = values.back();
    values.pop_back();
    if (handle == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {handle};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreRelease)
        buildNoUnwindCall(semaphoreReleaseFunctionType_, semaphoreReleaseFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreClose)
        buildNoUnwindCall(semaphoreCloseFunctionType_, semaphoreCloseFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreEventSet)
        buildNoUnwindCall(eventSetFunctionType_, eventSetFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreEventReset)
        buildNoUnwindCall(eventResetFunctionType_, eventResetFunction_, args, 1, "");
    else
        buildNoUnwindCall(eventCloseFunctionType_, eventCloseFunction_, args, 1, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame065CoreSemaphoreWait(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreWait ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventWait)) return false;
    if (current->children.size() != 2 || waitForHandleTimeoutFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Stage 54.5 Semaphore/Event Wait requires a handle + timeout argument and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef handle = values.back();
        values.pop_back();
        if (handle == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, handle, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef timeout = values.back();
    values.pop_back();
    if (timeout == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, timeout};
    values.push_back(buildNoUnwindCall(
        waitForHandleTimeoutFunctionType_, waitForHandleTimeoutFunction_, args, 2, "wait.timeout"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame066CoreSemaphoreCreate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreCreate ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventCreate)) return false;
    const bool sem = current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreCreate;
    if (current->children.size() != 2 ||
        (sem && semaphoreCreateFunction_ == nullptr) ||
        (!sem && eventCreateFunction_ == nullptr))
    {
        values.push_back(failValue(error, "Stage 54.5 sync create requires two arguments and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef first = values.back();
        values.pop_back();
        if (first == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, first, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef second = values.back();
    values.pop_back();
    if (second == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, second};
    if (sem)
        values.push_back(buildNoUnwindCall(
            semaphoreCreateFunctionType_, semaphoreCreateFunction_, args, 2, "semaphore.create"));
    else
        values.push_back(buildNoUnwindCall(
            eventCreateFunctionType_, eventCreateFunction_, args, 2, "event.create"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame067CoreSemaphoreCreateNamed(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreCreateNamed ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreEventCreateNamed)) return false;
    const bool sem = current->kind == frontend::ProjectOopMirNodeKind::CoreSemaphoreCreateNamed;
    if (current->children.size() != 3 ||
        (sem && semaphoreCreateNamedFunction_ == nullptr) ||
        (!sem && eventCreateNamedFunction_ == nullptr))
    {
        values.push_back(failValue(error, "Item 370 named sync create requires three arguments and runtime hook"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef name = values.back();
    values.pop_back();
    LLVMValueRef second = values.back();
    values.pop_back();
    LLVMValueRef first = values.back();
    values.pop_back();
    if (first == nullptr || second == nullptr || name == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMValueRef args[] = {first, second, name};
    if (sem)
        values.push_back(buildNoUnwindCall(
            semaphoreCreateNamedFunctionType_, semaphoreCreateNamedFunction_, args, 3,
            "semaphore.create.named"));
    else
        values.push_back(buildNoUnwindCall(
            eventCreateNamedFunctionType_, eventCreateNamedFunction_, args, 3,
            "event.create.named"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame068CoreProcessRun(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreProcessRun)) return false;
    if (current->children.size() != 3 || processRunFunction_ == nullptr)
    {
        values.push_back(failValue(error, "Bloque 4 Process.Run requires three string arguments and runtime hook"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef outputPath = values.back();
    values.pop_back();
    LLVMValueRef commandLine = values.back();
    values.pop_back();
    LLVMValueRef executable = values.back();
    values.pop_back();
    if (executable == nullptr || commandLine == nullptr || outputPath == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMValueRef args[] = {executable, commandLine, outputPath};
    values.push_back(buildNoUnwindCall(
        processRunFunctionType_, processRunFunction_, args, 3, "process.run"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame069CoreProcessSetPriorityClass(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDebugOutputString ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreThreadSleep)) return false;
    if (frame.phase == 0)
    {
        const bool setPri = current->kind == frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass;
        const bool dbgOut = current->kind == frontend::ProjectOopMirNodeKind::CoreDebugOutputString;
        if (current->children.size() != 1 ||
            (setPri && processSetPriorityClassFunction_ == nullptr) ||
            (dbgOut && debugOutputStringFunction_ == nullptr) ||
            (!setPri && !dbgOut && threadSleepFunction_ == nullptr))
        {
            values.push_back(failValue(error, "Stage 55 utility intrinsic requires one argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef operand = values.back();
    values.pop_back();
    if (operand == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {operand};
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreProcessSetPriorityClass)
        buildNoUnwindCall(processSetPriorityClassFunctionType_, processSetPriorityClassFunction_, args, 1, "");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreDebugOutputString)
        buildNoUnwindCall(debugOutputStringFunctionType_, debugOutputStringFunction_, args, 1, "");
    else
        buildNoUnwindCall(threadSleepFunctionType_, threadSleepFunction_, args, 1, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame070CoreFileCopy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileCopy)) return false;
    if (current->children.size() != 3 || fileCopyFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Pre-Stage-56 File.CopyRaw requires source, dest, overwrite arguments and runtime hook"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef overwrite = values.back();
    values.pop_back();
    LLVMValueRef dest = values.back();
    values.pop_back();
    LLVMValueRef source = values.back();
    values.pop_back();
    if (source == nullptr || dest == nullptr || overwrite == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMValueRef args[] = {source, dest, overwrite};
    values.push_back(buildNoUnwindCall(
        fileCopyFunctionType_, fileCopyFunction_, args, 3, "file.copy"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame071CoreFileMove(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreFileMove ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreDirectoryMove)) return false;
    if (current->children.size() != 2 || moveFileFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Pre-Stage-56 File/Directory MoveRaw requires source + dest arguments and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef source = values.back();
        values.pop_back();
        if (source == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, source, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef dest = values.back();
    values.pop_back();
    if (dest == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef args[] = {frame.lhs, dest};
    values.push_back(buildNoUnwindCall(
        moveFileFunctionType_, moveFileFunction_, args, 2, "move.file"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame072CoreInterlockedIncrementInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt32 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt32)) return false;
    const bool inc = current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt32;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 1)
            {
                values.push_back(failValue(
                    error,
                    inc ? "ordered atomic Increment requires one pointer argument"
                        : "ordered atomic Decrement requires one pointer argument"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicRmwOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic RMW carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 1 ||
                 (inc && interlockedIncrementInt32Function_ == nullptr) ||
                 (!inc && interlockedDecrementInt32Function_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.2 Interlocked int32 requires one pointer argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicRmwOrdering(current->integerValue);
        LLVMTypeRef type = LLVMInt32TypeInContext(context_);
        LLVMValueRef delta = LLVMConstInt(type, inc ? 1U : ~0ULL, 1);
        values.push_back(buildOrderedAtomicRmw(
            builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 4U,
            true, inc ? "atomic.increment.ordered.i32" : "atomic.decrement.ordered.i32"));
    }
    else
    {
        LLVMValueRef args[] = {location};
        if (inc)
            values.push_back(buildNoUnwindCall(
                interlockedIncrementInt32FunctionType_,
                interlockedIncrementInt32Function_, args, 1, "interlocked.increment.i32"));
        else
            values.push_back(buildNoUnwindCall(
                interlockedDecrementInt32FunctionType_,
                interlockedDecrementInt32Function_, args, 1, "interlocked.decrement.i32"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame073CoreInterlockedIncrementInt64(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt64 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedDecrementInt64)) return false;
    const bool inc = current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedIncrementInt64;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 1)
            {
                values.push_back(failValue(
                    error,
                    inc ? "ordered atomic Increment requires one pointer argument"
                        : "ordered atomic Decrement requires one pointer argument"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicRmwOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic RMW carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 1 ||
                 (inc && interlockedIncrementInt64Function_ == nullptr) ||
                 (!inc && interlockedDecrementInt64Function_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.2 Interlocked int64 requires one pointer argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicRmwOrdering(current->integerValue);
        LLVMTypeRef type = LLVMInt64TypeInContext(context_);
        LLVMValueRef delta = LLVMConstInt(type, inc ? 1U : ~0ULL, 1);
        values.push_back(buildOrderedAtomicRmw(
            builder_, LLVMAtomicRMWBinOpAdd, location, delta, *ordering, 8U,
            true, inc ? "atomic.increment.ordered.i64" : "atomic.decrement.ordered.i64"));
    }
    else
    {
        LLVMValueRef args[] = {location};
        if (inc)
            values.push_back(buildNoUnwindCall(
                interlockedIncrementInt64FunctionType_,
                interlockedIncrementInt64Function_, args, 1, "interlocked.increment.i64"));
        else
            values.push_back(buildNoUnwindCall(
                interlockedDecrementInt64FunctionType_,
                interlockedDecrementInt64Function_, args, 1, "interlocked.decrement.i64"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame074CoreInterlockedAddInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt32 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt32)) return false;
    const bool add = current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt32;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 2)
            {
                values.push_back(failValue(
                    error,
                    add ? "ordered atomic Add requires pointer and value arguments"
                        : "ordered atomic Exchange requires pointer and value arguments"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicRmwOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic RMW carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 2 ||
                 (add && interlockedAddInt32Function_ == nullptr) ||
                 (!add && interlockedExchangeInt32Function_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.2 Interlocked int32 RMW requires two arguments and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef location = values.back();
        values.pop_back();
        if (location == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, location, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicRmwOrdering(current->integerValue);
        values.push_back(buildOrderedAtomicRmw(
            builder_, add ? LLVMAtomicRMWBinOpAdd : LLVMAtomicRMWBinOpXchg,
            frame.lhs, value, *ordering, 4U, add,
            add ? "atomic.add.ordered.i32" : "atomic.exchange.ordered.i32"));
    }
    else
    {
        LLVMValueRef args[] = {frame.lhs, value};
        if (add)
            values.push_back(buildNoUnwindCall(
                interlockedAddInt32FunctionType_, interlockedAddInt32Function_, args, 2, "interlocked.add.i32"));
        else
            values.push_back(buildNoUnwindCall(
                interlockedExchangeInt32FunctionType_,
                interlockedExchangeInt32Function_, args, 2, "interlocked.exchange.i32"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame075CoreInterlockedAddInt64(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt64 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedExchangeInt64)) return false;
    const bool add = current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedAddInt64;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 2)
            {
                values.push_back(failValue(
                    error,
                    add ? "ordered atomic Add requires pointer and value arguments"
                        : "ordered atomic Exchange requires pointer and value arguments"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicRmwOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic RMW carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 2 ||
                 (add && interlockedAddInt64Function_ == nullptr) ||
                 (!add && interlockedExchangeInt64Function_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.2 Interlocked int64 RMW requires two arguments and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef location = values.back();
        values.pop_back();
        if (location == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, location, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicRmwOrdering(current->integerValue);
        values.push_back(buildOrderedAtomicRmw(
            builder_, add ? LLVMAtomicRMWBinOpAdd : LLVMAtomicRMWBinOpXchg,
            frame.lhs, value, *ordering, 8U, add,
            add ? "atomic.add.ordered.i64" : "atomic.exchange.ordered.i64"));
    }
    else
    {
        LLVMValueRef args[] = {frame.lhs, value};
        if (add)
            values.push_back(buildNoUnwindCall(
                interlockedAddInt64FunctionType_, interlockedAddInt64Function_, args, 2, "interlocked.add.i64"));
        else
            values.push_back(buildNoUnwindCall(
                interlockedExchangeInt64FunctionType_,
                interlockedExchangeInt64Function_, args, 2, "interlocked.exchange.i64"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame076CoreAtomicLoadInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt32 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt64)) return false;
    const bool i32 = current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicLoadInt32;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 1)
            {
                values.push_back(failValue(error, "ordered atomic Load requires one pointer argument"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicLoadOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic Load carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 1 ||
                 (i32 && atomicLoadInt32Function_ == nullptr) ||
                 (!i32 && atomicLoadInt64Function_ == nullptr))
        {
            values.push_back(failValue(error, "atomic Load requires one pointer argument and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicLoadOrdering(current->integerValue);
        LLVMValueRef load = LLVMBuildLoad2(
            builder_, i32 ? LLVMInt32TypeInContext(context_) : LLVMInt64TypeInContext(context_),
            location, i32 ? "atomic.load.ordered.i32" : "atomic.load.ordered.i64");
        LLVMSetOrdering(load, *ordering);
        LLVMSetAlignment(load, i32 ? 4U : 8U);
        values.push_back(load);
    }
    else
    {
        LLVMValueRef args[] = {location};
        if (i32)
            values.push_back(buildNoUnwindCall(
                atomicLoadInt32FunctionType_, atomicLoadInt32Function_, args, 1, "atomic.load.i32"));
        else
            values.push_back(buildNoUnwindCall(
                atomicLoadInt64FunctionType_, atomicLoadInt64Function_, args, 1, "atomic.load.i64"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame077CoreAtomicStoreInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt32 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt64)) return false;
    const bool i32 = current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicStoreInt32;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 2)
            {
                values.push_back(failValue(error, "ordered atomic Store requires pointer and value arguments"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> ordering =
                orderedAtomicStoreOrdering(current->integerValue);
            if (!ordering.has_value())
            {
                values.push_back(failValue(error, "ordered atomic Store carries an invalid MemoryOrder"));
                return true;
            }
        }
        else if (current->children.size() != 2 ||
                 (i32 && atomicStoreInt32Function_ == nullptr) ||
                 (!i32 && atomicStoreInt64Function_ == nullptr))
        {
            values.push_back(failValue(error, "atomic Store requires two arguments and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef location = values.back();
        values.pop_back();
        if (location == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, location, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> ordering =
            orderedAtomicStoreOrdering(current->integerValue);
        LLVMValueRef store = LLVMBuildStore(builder_, value, frame.lhs);
        LLVMSetOrdering(store, *ordering);
        LLVMSetAlignment(store, i32 ? 4U : 8U);
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else
    {
        LLVMValueRef args[] = {frame.lhs, value};
        if (i32)
            buildNoUnwindCall(atomicStoreInt32FunctionType_, atomicStoreInt32Function_, args, 2, "");
        else
            buildNoUnwindCall(atomicStoreInt64FunctionType_, atomicStoreInt64Function_, args, 2, "");
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame078CoreAtomicLoadPointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicLoadPointer)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "AtomicPointer.Load requires one pointer-location argument"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr) { values.push_back(nullptr); return true; }
    const LLVMAtomicOrdering ordering = current->integerValue == 0U
        ? LLVMAtomicOrderingSequentiallyConsistent
        : orderedAtomicLoadOrdering(current->integerValue).value_or(LLVMAtomicOrderingNotAtomic);
    if (ordering == LLVMAtomicOrderingNotAtomic)
    {
        values.push_back(failValue(error, "AtomicPointer.Load carries an invalid MemoryOrder"));
        return true;
    }
    LLVMTypeRef pointerType = llvmType("builtin:void*", error);
    if (pointerType == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef load = LLVMBuildLoad2(builder_, pointerType, location, "atomic.load.pointer");
    LLVMSetOrdering(load, ordering);
    LLVMSetAlignment(load, 8U);
    values.push_back(load);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame079CoreAtomicStorePointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicStorePointer ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedExchangePointer)) return false;
    const bool store = current->kind == frontend::ProjectOopMirNodeKind::CoreAtomicStorePointer;
    if (frame.phase == 0)
    {
        if (current->children.size() != 2)
        {
            values.push_back(failValue(
                error,
                store ? "AtomicPointer.Store requires pointer-location and value arguments"
                      : "AtomicPointer.Exchange requires pointer-location and value arguments"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef location = values.back();
        values.pop_back();
        if (location == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, location, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    const LLVMAtomicOrdering ordering = current->integerValue == 0U
        ? LLVMAtomicOrderingSequentiallyConsistent
        : (store
            ? orderedAtomicStoreOrdering(current->integerValue).value_or(LLVMAtomicOrderingNotAtomic)
            : orderedAtomicRmwOrdering(current->integerValue).value_or(LLVMAtomicOrderingNotAtomic));
    if (ordering == LLVMAtomicOrderingNotAtomic)
    {
        values.push_back(failValue(
            error,
            store ? "AtomicPointer.Store carries an invalid MemoryOrder"
                  : "AtomicPointer.Exchange carries an invalid MemoryOrder"));
        return true;
    }
    if (store)
    {
        LLVMValueRef storeInsn = LLVMBuildStore(builder_, value, frame.lhs);
        LLVMSetOrdering(storeInsn, ordering);
        LLVMSetAlignment(storeInsn, 8U);
        values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    }
    else
    {
        values.push_back(buildOrderedAtomicRmw(
            builder_, LLVMAtomicRMWBinOpXchg, frame.lhs, value, ordering, 8U,
            false, "atomic.exchange.pointer"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame080CoreInterlockedCompareExchangeInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64)) return false;
    const bool i32 = current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32;
    if (frame.phase == 0)
    {
        if (current->integerValue != 0U)
        {
            if (current->children.size() != 3)
            {
                values.push_back(failValue(
                    error, "ordered atomic CompareExchange requires pointer, value, and comparand arguments"));
                return true;
            }
            const std::optional<LLVMAtomicOrdering> successOrder =
                orderedAtomicRmwOrdering(current->integerValue & 0xffU);
            const std::optional<LLVMAtomicOrdering> failureOrder =
                orderedAtomicLoadOrdering((current->integerValue >> 8U) & 0xffU);
            if (!successOrder.has_value() || !failureOrder.has_value())
            {
                values.push_back(failValue(
                    error, "ordered atomic CompareExchange carries invalid MemoryOrder metadata"));
                return true;
            }
        }
        else if (current->children.size() != 3 ||
                 (i32 && interlockedCompareExchangeInt32Function_ == nullptr) ||
                 (!i32 && interlockedCompareExchangeInt64Function_ == nullptr))
        {
            values.push_back(failValue(
                error, "Stage 54.2 Interlocked.CompareExchange requires three arguments and runtime hook"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef comparand = values.back();
    values.pop_back();
    LLVMValueRef value = values.back();
    values.pop_back();
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr || value == nullptr || comparand == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> successOrder =
            orderedAtomicRmwOrdering(current->integerValue & 0xffU);
        const std::optional<LLVMAtomicOrdering> failureOrder =
            orderedAtomicLoadOrdering((current->integerValue >> 8U) & 0xffU);
        values.push_back(buildOrderedAtomicCompareExchange(
            builder_, location, value, comparand, *successOrder, *failureOrder, i32 ? 4U : 8U,
            i32 ? "atomic.compare_exchange.ordered.i32" : "atomic.compare_exchange.ordered.i64"));
    }
    else
    {
        LLVMValueRef args[] = {location, value, comparand};
        if (i32)
            values.push_back(buildNoUnwindCall(
                interlockedCompareExchangeInt32FunctionType_,
                interlockedCompareExchangeInt32Function_, args, 3, "interlocked.cmpxchg.i32"));
        else
            values.push_back(buildNoUnwindCall(
                interlockedCompareExchangeInt64FunctionType_,
                interlockedCompareExchangeInt64Function_, args, 3, "interlocked.cmpxchg.i64"));
    }
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame081CoreInterlockedCompareExchangePointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer)) return false;
    if (current->children.size() != 3)
    {
        values.push_back(failValue(
            error,
            "AtomicPointer.CompareExchange requires pointer-location, value, and comparand arguments"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef comparand = values.back();
    values.pop_back();
    LLVMValueRef value = values.back();
    values.pop_back();
    LLVMValueRef location = values.back();
    values.pop_back();
    if (location == nullptr || value == nullptr || comparand == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMAtomicOrdering successOrder = LLVMAtomicOrderingSequentiallyConsistent;
    LLVMAtomicOrdering failureOrder = LLVMAtomicOrderingSequentiallyConsistent;
    if (current->integerValue != 0U)
    {
        const std::optional<LLVMAtomicOrdering> orderedSuccess =
            orderedAtomicRmwOrdering(current->integerValue & 0xffU);
        const std::optional<LLVMAtomicOrdering> orderedFailure =
            orderedAtomicLoadOrdering((current->integerValue >> 8U) & 0xffU);
        if (!orderedSuccess.has_value() || !orderedFailure.has_value())
        {
            values.push_back(failValue(error, "AtomicPointer.CompareExchange carries invalid MemoryOrder metadata"));
            return true;
        }
        successOrder = *orderedSuccess;
        failureOrder = *orderedFailure;
    }
    values.push_back(buildOrderedAtomicCompareExchange(
        builder_, location, value, comparand, successOrder, failureOrder, 8U,
        "atomic.compare_exchange.pointer"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame082CoreSimdLoadAligned128(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdLoadAligned128 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdLoadUnaligned128)) return false;
    const bool aligned = current->kind == frontend::ProjectOopMirNodeKind::CoreSimdLoadAligned128;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(
                error,
                aligned ? "aligned SIMD load requires one pointer operand"
                        : "unaligned SIMD load requires one pointer operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef source = values.back();
    values.pop_back();
    if (source == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef vectorType = LLVMVectorType(LLVMInt32TypeInContext(context_), 4U);
    LLVMValueRef vector = LLVMBuildLoad2(
        builder_, vectorType, source, aligned ? "simd.load.aligned" : "simd.load.unaligned");
    LLVMSetAlignment(vector, aligned ? 16U : 1U);
    LLVMValueRef slot = entryBlockAlloca(
        carrier, 16U, aligned ? "simd.load.carrier.addr" : "simd.unaligned.load.carrier.addr");
    LLVMValueRef store = LLVMBuildStore(builder_, vector, slot);
    LLVMSetAlignment(store, 16U);
    LLVMValueRef result = LLVMBuildLoad2(
        builder_, carrier, slot, aligned ? "simd.load.carrier" : "simd.unaligned.load.carrier");
    LLVMSetAlignment(result, 16U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame083CoreSimdStoreAligned128(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdStoreAligned128 ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdStoreUnaligned128)) return false;
    const bool aligned = current->kind == frontend::ProjectOopMirNodeKind::CoreSimdStoreAligned128;
    if (frame.phase == 0)
    {
        if (current->children.size() != 2)
        {
            values.push_back(failValue(
                error,
                aligned ? "aligned SIMD store requires pointer and value operands"
                        : "unaligned SIMD store requires pointer and value operands"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef destination = values.back();
        values.pop_back();
        if (destination == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, destination, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->children[1]->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef vectorType = LLVMVectorType(LLVMInt32TypeInContext(context_), 4U);
    LLVMValueRef slot = entryBlockAlloca(
        carrier, 16U, aligned ? "simd.store.carrier.addr" : "simd.unaligned.store.carrier.addr");
    LLVMValueRef carrierStore = LLVMBuildStore(builder_, value, slot);
    LLVMSetAlignment(carrierStore, 16U);
    LLVMValueRef vector = LLVMBuildLoad2(
        builder_, vectorType, slot, aligned ? "simd.store.vector" : "simd.unaligned.store.vector");
    LLVMSetAlignment(vector, 16U);
    LLVMValueRef store = LLVMBuildStore(builder_, vector, frame.lhs);
    LLVMSetAlignment(store, aligned ? 16U : 1U);
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame084CoreCpuPrefetchRead(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreCpuPrefetchRead)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "PrefetchRead requires one pointer operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef address = values.back();
    values.pop_back();
    if (address == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
    LLVMTypeRef parameters[] = {LLVMPointerTypeInContext(context_, 0), i32, i32, i32};
    LLVMTypeRef type = LLVMFunctionType(LLVMVoidTypeInContext(context_), parameters, 4U, 0);
    LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, "llvm.prefetch.p0");
    if (intrinsic == nullptr) intrinsic = LLVMAddFunction(module_, "llvm.prefetch.p0", type);
    LLVMValueRef arguments[] = {address, LLVMConstInt(i32, 0, 0),
        LLVMConstInt(i32, 3, 0), LLVMConstInt(i32, 1, 0)};
    LLVMBuildCall2(builder_, type, intrinsic, arguments, 4U, "");
    values.push_back(LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame085CoreHalfFromFloat(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreHalfFromFloat)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "Half.FromFloat requires one float operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef binary16 = LLVMBuildFPTrunc(
        builder_, value, LLVMHalfTypeInContext(context_), "half.from.float");
    LLVMValueRef slot = entryBlockAlloca(carrier, 2U, "half.result.addr");
    LLVMValueRef store = LLVMBuildStore(builder_, binary16, slot);
    LLVMSetAlignment(store, 2U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, slot, "half.result");
    LLVMSetAlignment(result, 2U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame086CoreHalfToFloat(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreHalfToFloat)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "Half.ToFloat requires one Half operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->children.front()->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef slot = entryBlockAlloca(carrier, 2U, "half.source.addr");
    LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
    LLVMSetAlignment(store, 2U);
    LLVMValueRef binary16 = LLVMBuildLoad2(
        builder_, LLVMHalfTypeInContext(context_), slot, "half.source");
    LLVMSetAlignment(binary16, 2U);
    values.push_back(LLVMBuildFPExt(
        builder_, binary16, LLVMFloatTypeInContext(context_), "half.to.float"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame087CoreMonitorWait(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreMonitorWait)) return false;
    if (current->children.size() != 2 || monitorWaitFunction_ == nullptr)
    {
        values.push_back(failValue(
            error, "Stage 54.4 Monitor.Wait requires a receiver + timeout argument and runtime hook"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef receiver = values.back();
        values.pop_back();
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, receiver, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef timeout = values.back();
    values.pop_back();
    if (timeout == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef cvOffset = LLVMConstInt(LLVMInt64TypeInContext(context_), current->immediate, 0);
    LLVMValueRef cvIndices[] = {cvOffset};
    LLVMValueRef cvPointer = LLVMBuildGEP2(
        builder_, LLVMInt8TypeInContext(context_), frame.lhs, cvIndices, 1, "monitor.cv.addr");
    LLVMValueRef lockOffset = LLVMConstInt(LLVMInt64TypeInContext(context_), current->secondaryImmediate, 0);
    LLVMValueRef lockIndices[] = {lockOffset};
    LLVMValueRef lockPointer = LLVMBuildGEP2(
        builder_, LLVMInt8TypeInContext(context_), frame.lhs, lockIndices, 1, "monitor.lock.addr");
    LLVMValueRef args[] = {cvPointer, lockPointer, timeout};
    values.push_back(buildNoUnwindCall(monitorWaitFunctionType_, monitorWaitFunction_, args, 3, "monitor.wait"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame088CoreSimdFloat32x4Multiply(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "Float32x4 SIMD operation requires two operands"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, left, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    const unsigned laneCount = compactFloatVectorLaneCount(current->typeKey);
    if (laneCount < 2U || laneCount > 4U)
    {
        values.push_back(failValue(error, "compact floating SIMD intrinsic carries an unsupported type"));
        return true;
    }
    const unsigned alignment = laneCount == 4U ? 16U : 4U;
    LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), laneCount);
    LLVMValueRef leftSlot = entryBlockAlloca(carrier, alignment, "simd.left.addr");
    LLVMValueRef rightSlot = entryBlockAlloca(carrier, alignment, "simd.right.addr");
    LLVMValueRef leftStore = LLVMBuildStore(builder_, frame.lhs, leftSlot);
    LLVMValueRef rightStore = LLVMBuildStore(builder_, right, rightSlot);
    LLVMSetAlignment(leftStore, alignment);
    LLVMSetAlignment(rightStore, alignment);
    LLVMValueRef leftVector = LLVMBuildLoad2(builder_, vectorType, leftSlot, "simd.left");
    LLVMValueRef rightVector = LLVMBuildLoad2(builder_, vectorType, rightSlot, "simd.right");
    LLVMSetAlignment(leftVector, alignment);
    LLVMSetAlignment(rightVector, alignment);
    LLVMValueRef vectorResult = LLVMBuildFMul(builder_, leftVector, rightVector, "simd.multiply");
    vectorResult = applyFloatingPointMode(vectorResult, true);
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, alignment, "simd.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, vectorResult, resultSlot);
    LLVMSetAlignment(resultStore, alignment);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.result");
    LLVMSetAlignment(result, alignment);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame089CoreSimdInt128Multiply(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt128Multiply)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "128-bit integer SIMD operation requires two operands"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, left, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    const unsigned laneBits = integerSimd128LaneBits(current->typeKey);
    if (laneBits == 0U)
    {
        values.push_back(failValue(error, "integer SIMD intrinsic carries an unsupported type"));
        return true;
    }
    LLVMTypeRef elementType = laneBits == 32U
        ? LLVMInt32TypeInContext(context_)
        : LLVMInt64TypeInContext(context_);
    LLVMTypeRef vectorType = LLVMVectorType(elementType, 128U / laneBits);
    LLVMValueRef leftSlot = entryBlockAlloca(carrier, 16U, "simd.int.left.addr");
    LLVMValueRef rightSlot = entryBlockAlloca(carrier, 16U, "simd.int.right.addr");
    LLVMValueRef leftStore = LLVMBuildStore(builder_, frame.lhs, leftSlot);
    LLVMValueRef rightStore = LLVMBuildStore(builder_, right, rightSlot);
    LLVMSetAlignment(leftStore, 16U);
    LLVMSetAlignment(rightStore, 16U);
    LLVMValueRef leftVector = LLVMBuildLoad2(builder_, vectorType, leftSlot, "simd.int.left");
    LLVMValueRef rightVector = LLVMBuildLoad2(builder_, vectorType, rightSlot, "simd.int.right");
    LLVMSetAlignment(leftVector, 16U);
    LLVMSetAlignment(rightVector, 16U);
    LLVMValueRef vectorResult = LLVMBuildMul(builder_, leftVector, rightVector, "simd.int.multiply");
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, 16U, "simd.int.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, vectorResult, resultSlot);
    LLVMSetAlignment(resultStore, 16U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.int.result");
    LLVMSetAlignment(result, 16U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame090TemporaryAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::TemporaryAddress)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "C6 temporary address requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef slot = entryBlockAlloca(
        carrier, structAlignmentFor(current->typeKey), "struct.temp");
    if (slot == nullptr)
    {
        values.push_back(failValue(error, "C6 could not reserve storage for a struct temporary"));
        return true;
    }
    LLVMBuildStore(builder_, value, slot);
    values.push_back(slot);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame091CoreBitPopulationCount(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreBitPopulationCount ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreBitLeadingZeroCount ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreBitTrailingZeroCount)) return false;
    if (frame.phase == 0)
    {
        if (current->children.size() != 1)
        {
            values.push_back(failValue(error, "bit-count intrinsic requires one operand"));
            return true;
        }
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children.front().get(), 0, nullptr});
        return true;
    }
    LLVMValueRef value = values.back();
    values.pop_back();
    if (value == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef integerType = llvmType(current->typeKey, error);
    if (integerType == nullptr) { values.push_back(nullptr); return true; }
    const bool wide = current->typeKey == "builtin:ulong";
    const char* baseName = current->kind == frontend::ProjectOopMirNodeKind::CoreBitPopulationCount
        ? "llvm.ctpop." : current->kind == frontend::ProjectOopMirNodeKind::CoreBitLeadingZeroCount
        ? "llvm.ctlz." : "llvm.cttz.";
    const std::string intrinsicName = std::string(baseName) + (wide ? "i64" : "i32");
    LLVMTypeRef parameters[2] = {integerType, LLVMInt1TypeInContext(context_)};
    const bool hasZeroFlag =
        current->kind != frontend::ProjectOopMirNodeKind::CoreBitPopulationCount;
    LLVMTypeRef intrinsicType = LLVMFunctionType(
        integerType, parameters, hasZeroFlag ? 2U : 1U, 0);
    LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, intrinsicName.c_str());
    if (intrinsic == nullptr)
        intrinsic = LLVMAddFunction(module_, intrinsicName.c_str(), intrinsicType);
    LLVMValueRef arguments[2] = {value, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
    values.push_back(LLVMBuildCall2(builder_, intrinsicType, intrinsic, arguments,
        hasZeroFlag ? 2U : 1U, "bit.count"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame092CoreBitRotateLeft(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreBitRotateLeft ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreBitRotateRight ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreBitByteSwap)) return false;
    const bool byteSwap = current->kind == frontend::ProjectOopMirNodeKind::CoreBitByteSwap;
    if (current->children.size() != (byteSwap ? 1U : 2U))
    {
        values.push_back(failValue(error, "rotate/byte-swap intrinsic has invalid operands"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef value = values.back();
        values.pop_back();
        if (value == nullptr) { values.push_back(nullptr); return true; }
        if (byteSwap)
        {
            LLVMTypeRef integerType = llvmType(current->typeKey, error);
            if (integerType == nullptr) { values.push_back(nullptr); return true; }
            const bool wide = current->typeKey == "builtin:ulong";
            const std::string intrinsicName = std::string("llvm.bswap.") + (wide ? "i64" : "i32");
            LLVMTypeRef parameters[3] = {integerType, integerType, integerType};
            LLVMTypeRef intrinsicType = LLVMFunctionType(integerType, parameters, 1U, 0);
            LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, intrinsicName.c_str());
            if (intrinsic == nullptr) intrinsic = LLVMAddFunction(module_, intrinsicName.c_str(), intrinsicType);
            LLVMValueRef arguments[3] = {value, value, nullptr};
            values.push_back(LLVMBuildCall2(builder_, intrinsicType, intrinsic, arguments, 1U, "bit.byte.swap"));
            return true;
        }
        frames.push_back({current, 2, value, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef offset = values.back();
    values.pop_back();
    if (offset == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef integerType = llvmType(current->typeKey, error);
    if (integerType == nullptr) { values.push_back(nullptr); return true; }
    const bool wide = current->typeKey == "builtin:ulong";
    const char* operation = current->kind == frontend::ProjectOopMirNodeKind::CoreBitRotateLeft ? "fshl" : "fshr";
    const std::string intrinsicName = std::string("llvm.") + operation + (wide ? ".i64" : ".i32");
    LLVMTypeRef parameters[3] = {integerType, integerType, integerType};
    LLVMTypeRef intrinsicType = LLVMFunctionType(integerType, parameters, 3U, 0);
    LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, intrinsicName.c_str());
    if (intrinsic == nullptr) intrinsic = LLVMAddFunction(module_, intrinsicName.c_str(), intrinsicType);
    LLVMValueRef arguments[3] = {frame.lhs, frame.lhs, offset};
    values.push_back(LLVMBuildCall2(builder_, intrinsicType, intrinsic, arguments, 3U, "bit.rotate"));
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame093CoreSimdFloat256Add(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat256Add ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat256Subtract ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat256Multiply)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "256-bit floating SIMD operation requires two operands"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, left, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    const unsigned laneBits = floatingSimd256LaneBits(current->typeKey);
    if (laneBits == 0U)
    {
        values.push_back(failValue(error, "floating SIMD intrinsic carries an unsupported type"));
        return true;
    }
    LLVMTypeRef elementType = laneBits == 32U
        ? LLVMFloatTypeInContext(context_)
        : LLVMDoubleTypeInContext(context_);
    LLVMTypeRef vectorType = LLVMVectorType(elementType, 256U / laneBits);
    LLVMValueRef leftSlot = entryBlockAlloca(carrier, 32U, "simd.fp256.left.addr");
    LLVMValueRef rightSlot = entryBlockAlloca(carrier, 32U, "simd.fp256.right.addr");
    LLVMValueRef leftStore = LLVMBuildStore(builder_, frame.lhs, leftSlot);
    LLVMValueRef rightStore = LLVMBuildStore(builder_, right, rightSlot);
    LLVMSetAlignment(leftStore, 32U);
    LLVMSetAlignment(rightStore, 32U);
    LLVMValueRef leftVector = LLVMBuildLoad2(builder_, vectorType, leftSlot, "simd.fp256.left");
    LLVMValueRef rightVector = LLVMBuildLoad2(builder_, vectorType, rightSlot, "simd.fp256.right");
    LLVMSetAlignment(leftVector, 32U);
    LLVMSetAlignment(rightVector, 32U);
    LLVMValueRef vectorResult = nullptr;
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat256Add)
        vectorResult = LLVMBuildFAdd(builder_, leftVector, rightVector, "simd.fp256.add");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFloat256Subtract)
        vectorResult = LLVMBuildFSub(builder_, leftVector, rightVector, "simd.fp256.subtract");
    else
        vectorResult = LLVMBuildFMul(builder_, leftVector, rightVector, "simd.fp256.multiply");
    vectorResult = applyFloatingPointMode(vectorResult, true);
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, 32U, "simd.fp256.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, vectorResult, resultSlot);
    LLVMSetAlignment(resultStore, 32U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.fp256.result");
    LLVMSetAlignment(result, 32U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame094CoreSimdInt256Add(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt256Add ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt256Subtract ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt256Multiply)) return false;
    if (current->children.size() != 2)
    {
        values.push_back(failValue(error, "256-bit integer SIMD operation requires two operands"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1)
    {
        LLVMValueRef left = values.back();
        values.pop_back();
        if (left == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, left, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef right = values.back();
    values.pop_back();
    if (right == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    const unsigned laneBits = integerSimd256LaneBits(current->typeKey);
    if (laneBits == 0U)
    {
        values.push_back(failValue(error, "256-bit integer SIMD intrinsic carries an unsupported type"));
        return true;
    }
    LLVMTypeRef elementType = laneBits == 32U
        ? LLVMInt32TypeInContext(context_)
        : LLVMInt64TypeInContext(context_);
    LLVMTypeRef vectorType = LLVMVectorType(elementType, 256U / laneBits);
    LLVMValueRef leftSlot = entryBlockAlloca(carrier, 32U, "simd.int256.left.addr");
    LLVMValueRef rightSlot = entryBlockAlloca(carrier, 32U, "simd.int256.right.addr");
    LLVMValueRef leftStore = LLVMBuildStore(builder_, frame.lhs, leftSlot);
    LLVMValueRef rightStore = LLVMBuildStore(builder_, right, rightSlot);
    LLVMSetAlignment(leftStore, 32U);
    LLVMSetAlignment(rightStore, 32U);
    LLVMValueRef leftVector = LLVMBuildLoad2(builder_, vectorType, leftSlot, "simd.int256.left");
    LLVMValueRef rightVector = LLVMBuildLoad2(builder_, vectorType, rightSlot, "simd.int256.right");
    LLVMSetAlignment(leftVector, 32U);
    LLVMSetAlignment(rightVector, 32U);
    LLVMValueRef vectorResult = nullptr;
    if (current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt256Add)
        vectorResult = LLVMBuildAdd(builder_, leftVector, rightVector, "simd.int256.add");
    else if (current->kind == frontend::ProjectOopMirNodeKind::CoreSimdInt256Subtract)
        vectorResult = LLVMBuildSub(builder_, leftVector, rightVector, "simd.int256.subtract");
    else
        vectorResult = LLVMBuildMul(builder_, leftVector, rightVector, "simd.int256.multiply");
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, 32U, "simd.int256.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, vectorResult, resultSlot);
    LLVMSetAlignment(resultStore, 32U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.int256.result");
    LLVMSetAlignment(result, 32U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame095CoreSimdSqrt(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdSqrt ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdReciprocalSqrt ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd)) return false;
    const bool fma = current->kind == frontend::ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd;
    if (current->children.size() != (fma ? 3U : 1U))
    {
        values.push_back(failValue(error, "SIMD sqrt/FMA carries an invalid operand count"));
        return true;
    }
    if (frame.phase < current->children.size())
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), 4U);
    std::vector<LLVMValueRef> sourceValues(current->children.size());
    bool allValid = true;
    for (std::size_t i = current->children.size(); i > 0; --i)
    {
        sourceValues[i - 1] = values.back();
        values.pop_back();
        if (sourceValues[i - 1] == nullptr) allValid = false;
    }
    if (!allValid) { values.push_back(nullptr); return true; }
    LLVMValueRef vectors[3]{};
    for (std::size_t index = 0; index < current->children.size(); ++index)
    {
        LLVMValueRef slot = entryBlockAlloca(carrier, 16U, "simd.math.source.addr");
        LLVMValueRef store = LLVMBuildStore(builder_, sourceValues[index], slot);
        LLVMSetAlignment(store, 16U);
        vectors[index] = LLVMBuildLoad2(builder_, vectorType, slot, "simd.math.source");
        LLVMSetAlignment(vectors[index], 16U);
    }
    LLVMValueRef vectorResult = nullptr;
    if (fma)
    {
        LLVMTypeRef parameters[] = {vectorType, vectorType, vectorType};
        LLVMTypeRef intrinsicType = LLVMFunctionType(vectorType, parameters, 3U, 0);
        LLVMValueRef intrinsic = LLVMAddFunction(module_, "llvm.fma.v4f32", intrinsicType);
        vectorResult = LLVMBuildCall2(builder_, intrinsicType, intrinsic, vectors, 3U, "simd.fma");
    }
    else
    {
        LLVMTypeRef intrinsicType = LLVMFunctionType(vectorType, &vectorType, 1U, 0);
        LLVMValueRef intrinsic = LLVMGetNamedFunction(module_, "llvm.sqrt.v4f32");
        if (intrinsic == nullptr)
            intrinsic = LLVMAddFunction(module_, "llvm.sqrt.v4f32", intrinsicType);
        LLVMValueRef squareRoot = LLVMBuildCall2(
            builder_, intrinsicType, intrinsic, vectors, 1U, "simd.sqrt");
        if (current->kind == frontend::ProjectOopMirNodeKind::CoreSimdReciprocalSqrt)
        {
            LLVMValueRef ones[4]{};
            for (LLVMValueRef& one : ones)
                one = LLVMConstReal(LLVMFloatTypeInContext(context_), 1.0);
            vectorResult = LLVMBuildFDiv(
                builder_, LLVMConstVector(ones, 4U), squareRoot, "simd.rsqrt");
            vectorResult = applyFloatingPointMode(vectorResult);
        }
        else
        {
            vectorResult = squareRoot;
        }
    }
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, 16U, "simd.math.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, vectorResult, resultSlot);
    LLVMSetAlignment(resultStore, 16U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "simd.math.result");
    LLVMSetAlignment(result, 16U);
    values.push_back(result);
    return true;
}

bool OopLlvmImplementation::lowerExpressionFrame096CoreSimdSwizzleWzyx(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::CoreSimdSwizzleWzyx ||
        current->kind == frontend::ProjectOopMirNodeKind::CoreSimdShuffleLowHigh)) return false;
    const bool twoSources = current->kind == frontend::ProjectOopMirNodeKind::CoreSimdShuffleLowHigh;
    if (current->children.size() != (twoSources ? 2U : 1U))
    {
        values.push_back(failValue(error, "SIMD shuffle carries an invalid operand count"));
        return true;
    }
    if (frame.phase == 0)
    {
        frames.push_back({current, 1, nullptr, 0});
        frames.push_back({current->children[0].get(), 0, nullptr});
        return true;
    }
    if (frame.phase == 1 && twoSources)
    {
        LLVMValueRef first = values.back();
        values.pop_back();
        if (first == nullptr) { values.push_back(nullptr); return true; }
        frames.push_back({current, 2, first, 0});
        frames.push_back({current->children[1].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef firstValue;
    LLVMValueRef secondValue;
    if (twoSources)
    {
        secondValue = values.back();
        values.pop_back();
        firstValue = frame.lhs;
    }
    else
    {
        firstValue = values.back();
        values.pop_back();
        secondValue = nullptr;
    }
    if (firstValue == nullptr || (twoSources && secondValue == nullptr))
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), 4U);
    LLVMValueRef first = loadShuffleVector(
        carrier, vectorType, firstValue, "shuffle.first");
    LLVMValueRef second = twoSources
        ? loadShuffleVector(carrier, vectorType, secondValue, "shuffle.second")
        : LLVMGetUndef(vectorType);
    const unsigned swizzleIndices[] = {3U, 2U, 1U, 0U};
    const unsigned shuffleIndices[] = {0U, 1U, 6U, 7U};
    LLVMValueRef maskValues[4]{};
    for (unsigned index = 0; index < 4U; ++index)
        maskValues[index] = LLVMConstInt(LLVMInt32TypeInContext(context_),
            twoSources ? shuffleIndices[index] : swizzleIndices[index], 0);
    LLVMValueRef shuffled = LLVMBuildShuffleVector(
        builder_, first, second, LLVMConstVector(maskValues, 4U), "simd.shuffle");
    LLVMValueRef resultSlot = entryBlockAlloca(carrier, 16U, "shuffle.result.addr");
    LLVMValueRef resultStore = LLVMBuildStore(builder_, shuffled, resultSlot);
    LLVMSetAlignment(resultStore, 16U);
    LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "shuffle.result");
    LLVMSetAlignment(result, 16U);
    values.push_back(result);
    return true;
}

LLVMValueRef OopLlvmImplementation::loadShuffleVector(
        LLVMTypeRef carrier, LLVMTypeRef vectorType,
        LLVMValueRef value, const char* name)
{
    LLVMValueRef slot = entryBlockAlloca(carrier, 16U, name);
    LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
    LLVMSetAlignment(store, 16U);
    LLVMValueRef vector = LLVMBuildLoad2(builder_, vectorType, slot, name);
    LLVMSetAlignment(vector, 16U);
    return vector;
}

bool OopLlvmImplementation::lowerExpressionFrame097StringViewFromString(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::StringViewFromString)) return false;
    if (current->children.size() != 3 && current->children.size() != 2)
    {
        values.push_back(failValue(error, "StringView rest capture requires receiver, start and length"));
        return true;
    }
    const std::size_t operandCount = current->children.size();
    if (frame.phase < operandCount)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef countRaw = nullptr;
    if (operandCount == 3)
    {
        countRaw = values.back();
        values.pop_back();
    }
    LLVMValueRef rawIndex = values.back();
    values.pop_back();
    LLVMValueRef object = values.back();
    values.pop_back();
    if (object == nullptr || rawIndex == nullptr || (operandCount == 3 && countRaw == nullptr))
    {
        values.push_back(nullptr);
        return true;
    }
    object = checkedReceiver(object, error);
    if (object == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef index = normalizeNativeIndexFromValue(current->children[1].get(), rawIndex, error);
    if (index == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef length = LLVMBuildLoad2(
        builder_, LLVMInt64TypeInContext(context_), object, "stringview.length");
    LLVMValueRef count = operandCount == 3 ? countRaw : length;
    LLVMValueRef countIsZero = LLVMBuildICmp(
        builder_, LLVMIntEQ, count, LLVMConstInt(LLVMTypeOf(count), 0, 0), "stringview.empty");
    LLVMValueRef inRange = LLVMBuildICmp(
        builder_, LLVMIntULT, index, length, "stringview.inrange");
    if (isSignedInteger(current->children[1]->typeKey))
    {
        LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
        LLVMValueRef nonNegative = LLVMBuildICmp(
            builder_, LLVMIntSGE, index, zero, "stringview.nonneg");
        inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "stringview.nonempty.inrange");
    }
    LLVMValueRef valid = LLVMBuildOr(builder_, countIsZero, inRange, "stringview.valid");
    if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "stringview.index", error))
    {
        values.push_back(nullptr);
        return true;
    }
    finishStringViewFrame(current, object, index, count, countIsZero, values, error);
    return true;
}

void OopLlvmImplementation::finishStringViewFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, LLVMValueRef count, LLVMValueRef countIsZero,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    LLVMValueRef offset = LLVMBuildAdd(
        builder_, index, LLVMConstInt(LLVMInt64TypeInContext(context_), 8, 0),
        "stringview.offset");
    LLVMValueRef indices[] = {offset};
    LLVMValueRef gepData = LLVMBuildGEP2(
        builder_, LLVMInt8TypeInContext(context_), object, indices, 1, "stringview.data.gep");
    LLVMValueRef nullData = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
    LLVMValueRef data = LLVMBuildSelect(
        builder_, countIsZero, nullData, gepData, "stringview.data");
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return; }
    const unsigned alignment = structAlignmentFor(current->typeKey);
    LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "stringview.init");
    if (slot == nullptr)
    {
        values.push_back(failValue(error, "StringView rest capture could not reserve storage"));
        return;
    }
    LLVMBuildStore(builder_, data, byteOffset(slot, 0, "stringview.start.addr"));
    LLVMBuildStore(builder_, count, byteOffset(slot, 8, "stringview.count.addr"));
    LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "stringview.init.value");
    if (alignment > 1U) LLVMSetAlignment(loaded, alignment);
    values.push_back(loaded);
}

bool OopLlvmImplementation::lowerExpressionFrame098SpanFromArray(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::SpanFromArray ||
        current->kind == frontend::ProjectOopMirNodeKind::ReadOnlySpanFromArray)) return false;
    if (current->children.size() != 3 && current->children.size() != 2)
    {
        values.push_back(failValue(error, "Span rest capture requires receiver, start and length"));
        return true;
    }
    const std::size_t operandCount = current->children.size();
    if (frame.phase < operandCount)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef countRaw = nullptr;
    if (operandCount == 3)
    {
        countRaw = values.back();
        values.pop_back();
    }
    LLVMValueRef rawIndex = values.back();
    values.pop_back();
    LLVMValueRef object = values.back();
    values.pop_back();
    if (object == nullptr || rawIndex == nullptr || (operandCount == 3 && countRaw == nullptr))
    {
        values.push_back(nullptr);
        return true;
    }
    object = checkedReceiver(object, error);
    if (object == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef index = normalizeNativeIndexFromValue(current->children[1].get(), rawIndex, error);
    if (index == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef length = LLVMBuildLoad2(
        builder_, LLVMInt64TypeInContext(context_), object, "span.length");
    LLVMValueRef count = operandCount == 3 ? countRaw : length;
    LLVMValueRef countIsZero = LLVMBuildICmp(
        builder_, LLVMIntEQ, count, LLVMConstInt(LLVMTypeOf(count), 0, 0), "span.empty");
    LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, length, "span.inrange");
    if (isSignedInteger(current->children[1]->typeKey))
    {
        LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
        LLVMValueRef nonNegative = LLVMBuildICmp(
            builder_, LLVMIntSGE, index, zero, "span.nonneg");
        inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "span.nonempty.inrange");
    }
    LLVMValueRef valid = LLVMBuildOr(builder_, countIsZero, inRange, "span.valid");
    if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "span.index", error))
    {
        values.push_back(nullptr);
        return true;
    }
    finishArraySpanFrame(current, object, index, count, countIsZero, values, error);
    return true;
}

void OopLlvmImplementation::finishArraySpanFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, LLVMValueRef count, LLVMValueRef countIsZero,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const auto [elementSize, elementAlignment] = captureValueLayout(current->elementTypeKey, error);
    if (elementSize == 0) { values.push_back(nullptr); return; }
    const std::uint64_t dataOffset =
        (8U + elementAlignment - 1U) / elementAlignment * elementAlignment;
    LLVMValueRef scaled = LLVMBuildMul(
        builder_, index,
        LLVMConstInt(LLVMInt64TypeInContext(context_), elementSize, 0), "span.index.scaled");
    LLVMValueRef offset = LLVMBuildAdd(
        builder_, scaled,
        LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0), "span.offset");
    LLVMValueRef indices[] = {offset};
    LLVMValueRef gepData = LLVMBuildGEP2(
        builder_, LLVMInt8TypeInContext(context_), object, indices, 1, "span.data.gep");
    LLVMValueRef nullData = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
    LLVMValueRef data = LLVMBuildSelect(builder_, countIsZero, nullData, gepData, "span.data");
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return; }
    const unsigned carrierAlignment = structAlignmentFor(current->typeKey);
    LLVMValueRef slot = entryBlockAlloca(carrier, carrierAlignment, "span.init");
    if (slot == nullptr)
    {
        values.push_back(failValue(error, "Span rest capture could not reserve storage"));
        return;
    }
    LLVMBuildStore(builder_, data, byteOffset(slot, 0, "span.start.addr"));
    LLVMBuildStore(builder_, count, byteOffset(slot, 8, "span.count.addr"));
    LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "span.init.value");
    if (carrierAlignment > 1U) LLVMSetAlignment(loaded, carrierAlignment);
    values.push_back(loaded);
}

bool OopLlvmImplementation::lowerExpressionFrame099SpanFromFixedArray(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error)
{
    const frontend::ProjectOopMirNode* current = frame.node;
    if (!(current->kind == frontend::ProjectOopMirNodeKind::SpanFromFixedArray ||
        current->kind == frontend::ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray)) return false;
    if (current->children.size() != 3)
    {
        values.push_back(failValue(error, "fixed-array Span rest capture requires receiver, start and length"));
        return true;
    }
    const std::optional<std::uint64_t> fixedCount =
        frontend::ProjectMemberModel::fixedArrayCount(current->children[0]->typeKey);
    if (!fixedCount.has_value())
    {
        values.push_back(failValue(error, "fixed-array Span rest capture lost its source extent"));
        return true;
    }
    if (frame.phase < 3)
    {
        const std::size_t index = frame.phase;
        frames.push_back({current, static_cast<std::uint8_t>(index + 1), nullptr, 0});
        frames.push_back({current->children[index].get(), 0, nullptr});
        return true;
    }
    LLVMValueRef count = values.back();
    values.pop_back();
    LLVMValueRef rawStart = values.back();
    values.pop_back();
    LLVMValueRef base = values.back();
    values.pop_back();
    if (base == nullptr || rawStart == nullptr || count == nullptr)
    {
        values.push_back(nullptr);
        return true;
    }
    LLVMValueRef start = normalizeNativeIndexFromValue(current->children[1].get(), rawStart, error);
    if (start == nullptr) { values.push_back(nullptr); return true; }
    LLVMValueRef countNative = convert(
        count, current->children[2]->typeKey, "builtin:nuint", error);
    if (countNative == nullptr) { values.push_back(nullptr); return true; }
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
    LLVMValueRef zero64 = LLVMConstInt(i64, 0, 0);
    if (isSignedInteger(current->children[1]->typeKey))
    {
        LLVMValueRef valid = LLVMBuildICmp(
            builder_, LLVMIntSGE, start, zero64, "fixed.span.start.nonnegative");
        if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "fixed.span.start", error))
        {
            values.push_back(nullptr);
            return true;
        }
    }
    if (isSignedInteger(current->children[2]->typeKey))
    {
        LLVMValueRef valid = LLVMBuildICmp(
            builder_, LLVMIntSGE, countNative, zero64, "fixed.span.count.nonnegative");
        if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "fixed.span.count", error))
        {
            values.push_back(nullptr);
            return true;
        }
    }
    finishFixedArraySpanFrame(
        current, *fixedCount, base, start, countNative, zero64, values, error);
    return true;
}

void OopLlvmImplementation::finishFixedArraySpanFrame(
        const frontend::ProjectOopMirNode* current, std::uint64_t fixedCount,
        LLVMValueRef base, LLVMValueRef start, LLVMValueRef countNative,
        LLVMValueRef zero64, std::vector<LLVMValueRef>& values, std::string& error)
{
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
    LLVMValueRef extent = LLVMConstInt(i64, fixedCount, 0);
    LLVMValueRef startFits = LLVMBuildICmp(
        builder_, LLVMIntULE, start, extent, "fixed.span.start.fits");
    if (!emitGuard(startFits, LlvmTrapReason::BoundsViolation, "fixed.span.start.bounds", error))
    {
        values.push_back(nullptr);
        return;
    }
    LLVMValueRef remaining = LLVMBuildSub(builder_, extent, start, "fixed.span.remaining");
    LLVMValueRef countFits = LLVMBuildICmp(
        builder_, LLVMIntULE, countNative, remaining, "fixed.span.count.fits");
    if (!emitGuard(countFits, LlvmTrapReason::BoundsViolation, "fixed.span.count.bounds", error))
    {
        values.push_back(nullptr);
        return;
    }
    const auto [elementSize, ignoredAlignment] =
        captureValueLayout(current->elementTypeKey, error);
    (void)ignoredAlignment;
    if (elementSize == 0) { values.push_back(nullptr); return; }
    LLVMValueRef scaled = LLVMBuildMul(
        builder_, start, LLVMConstInt(i64, elementSize, 0), "fixed.span.start.scaled");
    LLVMValueRef indices[] = {scaled};
    LLVMValueRef dataAddress = LLVMBuildGEP2(
        builder_, LLVMInt8TypeInContext(context_), base, indices, 1, "fixed.span.data.gep");
    LLVMValueRef countIsZero = LLVMBuildICmp(
        builder_, LLVMIntEQ, countNative, zero64, "fixed.span.empty");
    LLVMValueRef nullData = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
    LLVMValueRef data = LLVMBuildSelect(
        builder_, countIsZero, nullData, dataAddress, "fixed.span.data");
    LLVMTypeRef carrier = llvmType(current->typeKey, error);
    if (carrier == nullptr) { values.push_back(nullptr); return; }
    const unsigned carrierAlignment = structAlignmentFor(current->typeKey);
    LLVMValueRef slot = entryBlockAlloca(carrier, carrierAlignment, "fixed.span.init");
    if (slot == nullptr)
    {
        values.push_back(failValue(error, "fixed-array Span rest capture could not reserve storage"));
        return;
    }
    LLVMBuildStore(builder_, data, byteOffset(slot, 0, "fixed.span.start.addr"));
    LLVMBuildStore(builder_, count, byteOffset(slot, 8, "fixed.span.count.addr"));
    LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "fixed.span.init.value");
    if (carrierAlignment > 1U) LLVMSetAlignment(loaded, carrierAlignment);
    values.push_back(loaded);
}


}
