#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"
#include "PackedArrayAlignment.h"
#include "CRecordBoolStorage.h"
#include "CVolatileRecordField.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::normalizeNativeIndex(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        LLVMValueRef value = lowerExpression(node, error);
        if (value == nullptr) return nullptr;
        return convert(value, node->typeKey, "builtin:nuint", error);
    }

LLVMValueRef OopLlvmImplementation::normalizeNativeIndexFromValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef value,
        std::string& error)
{
        if (value == nullptr) return nullptr;
        return convert(value, node->typeKey, "builtin:nuint", error);
    }

LLVMValueRef OopLlvmImplementation::lowerNativeLength(
        const frontend::ProjectOopMirNode* node,
        bool stringValue,
        std::string& error)
{
        (void)stringValue;
        if (node->children.size() != 1)
            return failValue(error, "Stage 41 native Length requires one receiver");
        const std::string beginRecord =
            runtimeDebugArrayRecord(*node, "ARRAY-LENGTH-BEGIN");
        if (!beginRecord.empty() && !emitRuntimeDebugTrace(4U, beginRecord, error))
            return nullptr;
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;
        object = checkedReceiver(object, error);
        if (object == nullptr) return nullptr;
        LLVMValueRef length64 = LLVMBuildLoad2(builder_, LLVMInt64TypeInContext(context_), object, "native.length64");
        LLVMValueRef length =
            LLVMBuildTrunc(builder_, length64, LLVMInt32TypeInContext(context_), "native.length");
        const std::string endRecord =
            runtimeDebugArrayRecord(*node, "ARRAY-LENGTH-END");
        return !endRecord.empty() && !emitRuntimeDebugTrace(4U, endRecord, error)
            ? nullptr
            : length;
    }

LLVMValueRef OopLlvmImplementation::arrayElementAddressFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        LLVMValueRef index,
        bool stringValue,
        std::string& error)
{
        LLVMValueRef length = LLVMBuildLoad2(builder_, LLVMInt64TypeInContext(context_), object, "native.index.length");
        if (node->booleanValue)
        {
            index = LLVMBuildSub(builder_, length, index, "native.index.fromend");
        }
        LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, length, "native.index.inrange");
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(builder_, LLVMIntSGE, index, zero, "native.index.nonnegative");
            inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "native.index.valid");
        }
        if (!emitGuard(inRange, LlvmTrapReason::BoundsViolation, "native.index", error)) return nullptr;
        const std::uint64_t elementSize = stringValue ? 1U : node->immediate;
        const std::uint64_t elementAlignment = stringValue ? 1U : node->alignment;
        const std::uint64_t dataOffset = (8U + elementAlignment - 1U) / elementAlignment * elementAlignment;
        LLVMValueRef scaled = LLVMBuildMul(
            builder_, index, LLVMConstInt(LLVMInt64TypeInContext(context_), elementSize, 0), "native.index.scaled");
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, scaled, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0), "native.index.offset");
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(builder_, LLVMInt8TypeInContext(context_), object, indices, 1, "native.element.addr");
    }

LLVMValueRef OopLlvmImplementation::rawElementAddressFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef base,
        LLVMValueRef index,
        std::string& error)
{
        if (node->hasTrailingLengthBounds)
        {
            LLVMValueRef lengthAddress = byteOffset(
                base, static_cast<std::uint64_t>(node->trailingLengthRelativeOffset),
                "flexible.length.addr");
            LLVMValueRef length = LLVMBuildLoad2(
                builder_, LLVMInt32TypeInContext(context_), lengthAddress, "flexible.length");
            LLVMValueRef lengthWide = LLVMBuildZExt(
                builder_, length, LLVMInt64TypeInContext(context_), "flexible.length.i64");
            LLVMValueRef inBounds = LLVMBuildICmp(
                builder_, LLVMIntULT, index, lengthWide, "flexible.inbounds");
            if (!emitGuard(inBounds, LlvmTrapReason::BoundsViolation, "flexible.index", error))
                return nullptr;
        }
        LLVMValueRef offset = LLVMBuildMul(
            builder_, index,
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            "raw.element.offset");
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), base, indices, 1, "raw.element.addr");
    }

LLVMValueRef OopLlvmImplementation::pointerArithmeticFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error)
{
        LLVMTypeRef i64Type = LLVMInt64TypeInContext(context_);
        LLVMValueRef stride = LLVMConstInt(i64Type, node->immediate, 0);
        if (!node->typeKey.ends_with("*"))
        {
            LLVMValueRef leftInt = LLVMBuildPtrToInt(builder_, left, i64Type, "ptrdiff.left");
            LLVMValueRef rightInt = LLVMBuildPtrToInt(builder_, right, i64Type, "ptrdiff.right");
            LLVMValueRef bytes = LLVMBuildSub(builder_, leftInt, rightInt, "ptrdiff.bytes");
            return LLVMBuildSDiv(builder_, bytes, stride, "ptrdiff.elements");
        }
        const bool leftIsPointer = node->children[0]->typeKey.ends_with("*");
        LLVMValueRef pointer = leftIsPointer ? left : right;
        const frontend::ProjectOopMirNode* offsetNode =
            leftIsPointer ? node->children[1].get() : node->children[0].get();
        LLVMValueRef index = normalizeNativeIndexFromValue(
            offsetNode, leftIsPointer ? right : left, error);
        if (index == nullptr) return nullptr;
        LLVMValueRef offset = LLVMBuildMul(builder_, index, stride, "pointer.offset");
        if (node->operation == lexer::TokenKind::Minus)
        {
            offset = LLVMBuildNeg(builder_, offset, "pointer.offset.neg");
        }
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), pointer, indices, 1, "pointer.arith");
    }

LLVMValueRef OopLlvmImplementation::compoundStoreFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error)
{
        const bool boolOperand = node->children[1]->typeKey == "builtin:bool";
        const auto& integerOperandType = node->children[1]->typeKey;
        const bool intOperand = boolOperand || integerOperandType == "builtin:sbyte" || integerOperandType == "builtin:byte" ||
            integerOperandType == "builtin:short" || integerOperandType == "builtin:ushort" ||
            integerOperandType == "builtin:int" || integerOperandType == "builtin:uint" ||
            integerOperandType == "builtin:long" || integerOperandType == "builtin:ulong";
        const bool floatOperand = integerOperandType == "builtin:float" || integerOperandType == "builtin:double";
        const bool boolBitwise = node->operation == lexer::TokenKind::Amp ||
            node->operation == lexer::TokenKind::Pipe || node->operation == lexer::TokenKind::Caret;
        const bool boolIntArithmetic = node->operation == lexer::TokenKind::Plus ||
            node->operation == lexer::TokenKind::Minus || node->operation == lexer::TokenKind::Star ||
            node->operation == lexer::TokenKind::Slash || node->operation == lexer::TokenKind::Percent ||
            node->operation == lexer::TokenKind::LessLess || node->operation == lexer::TokenKind::GreaterGreater;
        const bool boolFloatArithmetic = node->operation == lexer::TokenKind::Plus ||
            node->operation == lexer::TokenKind::Minus || node->operation == lexer::TokenKind::Star ||
            node->operation == lexer::TokenKind::Slash;
        const auto* compoundBoolTarget = node->children[0].get();
        const bool cVolatileBoolScalar = !compoundBoolTarget->cppAtomicAccess &&
            compoundBoolTarget->kind == frontend::ProjectOopMirNodeKind::NamespaceStateAddress &&
            compoundBoolTarget->typeKey == "builtin:bool" && compoundBoolTarget->volatileAccess &&
            compoundBoolTarget->cppMicrosoftVolatileSemantics == std::optional<bool>{true} &&
            compoundBoolTarget->cppAtomicAlignmentBytes == std::optional<std::size_t>{1};
        if ((clangMsVolatileGlobalBoolArray(compoundBoolTarget) || cVolatileBoolScalar) &&
            ((boolOperand && boolBitwise) || (intOperand && (boolBitwise || boolIntArithmetic)) ||
             (floatOperand && boolFloatArithmetic)))
        {
            LLVMTypeRef byteType = LLVMInt8TypeInContext(context_);
            LLVMValueRef raw = LLVMBuildLoad2(builder_, byteType, address, "c.bool.compound.load");
            LLVMSetVolatile(raw, 1);
            LLVMSetOrdering(raw, LLVMAtomicOrderingAcquire);
            LLVMSetAlignment(raw, 1);
            LLVMValueRef previous = LLVMBuildICmp(builder_, LLVMIntNE, raw,
                LLVMConstInt(byteType, 0, 0), "c.bool.compound.previous");
            LLVMValueRef updated = nullptr;
            if (floatOperand)
            {
                LLVMTypeRef floatingType = llvmType(integerOperandType, error);
                if (!floatingType) return nullptr;
                LLVMValueRef promoted = LLVMBuildUIToFP(builder_, previous, floatingType, "c.bool.compound.fp.promoted");
                LLVMValueRef calculated = applyBinaryOperation(node->operation, promoted, value,
                    integerOperandType, error, false);
                if (!calculated) return nullptr;
                updated = LLVMBuildFCmp(builder_, LLVMRealUNE, calculated,
                    LLVMConstReal(floatingType, 0.0), "c.bool.compound.fp.normalized");
            }
            else if (!boolOperand || !boolBitwise)
            {
                const bool shift = node->operation == lexer::TokenKind::LessLess ||
                    node->operation == lexer::TokenKind::GreaterGreater;
                const std::string calculationType = shift ? "builtin:int" :
                    integerOperandType == "builtin:ulong" ? "builtin:ulong" :
                    integerOperandType == "builtin:long" ? "builtin:long" :
                    integerOperandType == "builtin:uint" ? "builtin:uint" : "builtin:int";
                LLVMTypeRef intType = llvmType(calculationType, error);
                if (!intType) return nullptr;
                LLVMValueRef promoted = LLVMBuildZExt(builder_, previous, intType, "c.bool.compound.promoted");
                LLVMValueRef operand = boolOperand
                    ? LLVMBuildZExt(builder_, value, intType, "c.bool.compound.rhs")
                    : shift ? normalizeShiftCount(value, integerOperandType, calculationType, error)
                            : convert(value, integerOperandType, calculationType, error);
                if (!operand) return nullptr;
                LLVMValueRef calculated = applyBinaryOperation(node->operation, promoted, operand,
                    calculationType, error, false);
                if (!calculated) return nullptr;
                updated = LLVMBuildICmp(builder_, LLVMIntNE, calculated,
                    LLVMConstInt(intType, 0, 0), "c.bool.compound.normalized");
            }
            else
                updated = node->operation == lexer::TokenKind::Amp
                    ? LLVMBuildAnd(builder_, previous, value, "c.bool.compound.and")
                    : node->operation == lexer::TokenKind::Pipe
                        ? LLVMBuildOr(builder_, previous, value, "c.bool.compound.or")
                        : LLVMBuildXor(builder_, previous, value, "c.bool.compound.xor");
            LLVMValueRef bits = LLVMBuildZExt(builder_, updated, byteType, "c.bool.compound.bits");
            LLVMValueRef store = LLVMBuildStore(builder_, bits, address);
            LLVMSetVolatile(store, 1);
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, 1);
            return node->booleanValue ? previous : updated;
        }
        if (node->children[0]->cppAtomicAccess)
        {
            const auto* target = node->children[0].get();
            if (const auto* enumInfo = findEnumUnderlyingInfo(nameLookup_, target->typeKey))
            {
                const unsigned alignment = enumInfo->width / 8;
                const auto& rhsOriginalType = node->children[1]->typeKey;
                const auto* rhsEnumInfo = findEnumUnderlyingInfo(nameLookup_, rhsOriginalType);
                const std::string rhsType = rhsEnumInfo ? std::string(rhsEnumInfo->key) : rhsOriginalType;
                if (target->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
                    !alignment || target->cppAtomicAlignmentBytes != std::optional<std::size_t>{alignment} ||
                    node->immediate != 0 || !frontend::ProjectTypeConversion::isIntegral(rhsType))
                    return failValue(error, "C atomic enum compound operation lacks supported alignment or operand");
                const std::string underlying(enumInfo->key);
                const std::string promoted = enumInfo->width < 32 ? "builtin:int" : underlying;
                const bool shift = node->operation == lexer::TokenKind::LessLess ||
                    node->operation == lexer::TokenKind::GreaterGreater;
                const std::string calculationType = shift ? promoted :
                    (promoted == "builtin:u128" || rhsType == "builtin:u128") ? "builtin:u128" :
                    (promoted == "builtin:i128" || rhsType == "builtin:i128") ? "builtin:i128" :
                    (promoted == "builtin:ulong" || rhsType == "builtin:ulong" || rhsType == "builtin:nuint") ? "builtin:ulong" :
                    (promoted == "builtin:long" || rhsType == "builtin:long" || rhsType == "builtin:nint") ? "builtin:long" :
                    (promoted == "builtin:uint" || rhsType == "builtin:uint") ? "builtin:uint" : "builtin:int";
                LLVMTypeRef storage = llvmType(target->typeKey, error);
                LLVMValueRef operand = shift ? normalizeShiftCount(value, rhsType, calculationType, error) :
                    convert(value, rhsType, calculationType, error);
                if (!storage || !operand) return nullptr;
                LLVMValueRef initial = LLVMBuildLoad2(builder_, storage, address, "atomic.enum.initial");
                LLVMSetOrdering(initial, LLVMAtomicOrderingSequentiallyConsistent);
                LLVMSetAlignment(initial, alignment);
                if (target->volatileAccess) LLVMSetVolatile(initial, 1);
                LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
                LLVMBasicBlockRef retry = appendBlock("atomic.enum.retry");
                LLVMBasicBlockRef done = appendBlock("atomic.enum.done");
                LLVMBuildBr(builder_, retry);
                enterBlock(retry);
                LLVMValueRef current = LLVMBuildPhi(builder_, storage, "atomic.enum.current");
                LLVMAddIncoming(current, &initial, &preheader, 1);
                // Clang promotes an atomic enum's carrier with zero extension
                // when the compound calculation uses a wider integer type.
                const auto* calculationInfo = frontend::builtins::findIntegerType(calculationType);
                LLVMTypeRef calculationLlvmType = llvmType(calculationType, error);
                if (!calculationLlvmType) return nullptr;
                LLVMValueRef lhs = enumInfo->isSigned && calculationInfo &&
                    calculationInfo->width > enumInfo->width
                    ? LLVMBuildZExt(builder_, current, calculationLlvmType, "atomic.enum.promoted")
                    : convert(current, underlying, calculationType, error);
                if (!lhs) return nullptr;
                LLVMValueRef computed = applyBinaryOperation(node->operation, lhs, operand, calculationType, error, false);
                if (!computed) return nullptr;
                LLVMValueRef updated = convert(computed, calculationType, underlying, error);
                if (!updated) return nullptr;
                LLVMValueRef pair = LLVMBuildAtomicCmpXchg(builder_, address, current, updated,
                    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(pair, alignment);
                if (target->volatileAccess) LLVMSetVolatile(pair, 1);
                LLVMValueRef observed = LLVMBuildExtractValue(builder_, pair, 0, "atomic.enum.observed");
                LLVMValueRef success = LLVMBuildExtractValue(builder_, pair, 1, "atomic.enum.success");
                LLVMBasicBlockRef attempt = LLVMGetInsertBlock(builder_);
                LLVMBuildCondBr(builder_, success, done, retry);
                LLVMAddIncoming(current, &observed, &attempt, 1);
                enterBlock(done);
                return node->booleanValue ? current : updated;
            }
            if (target->typeKey.ends_with("*"))
            {
                if (target->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
                    target->cppAtomicAlignmentBytes != std::optional<std::size_t>{8} || node->immediate == 0 ||
                    (node->operation != lexer::TokenKind::Plus && node->operation != lexer::TokenKind::Minus))
                    return failValue(error, "C atomic pointer compound operation is not yet supported");
                LLVMValueRef index = normalizeNativeIndexFromValue(node->children[1].get(), value, error);
                LLVMTypeRef pointerType = llvmType(target->typeKey, error);
                if (index == nullptr || pointerType == nullptr) return nullptr;
                LLVMValueRef stride = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
                LLVMValueRef offset = LLVMBuildMul(builder_, index, stride, "atomic.pointer.offset");
                if (node->operation == lexer::TokenKind::Minus)
                    offset = LLVMBuildNeg(builder_, offset, "atomic.pointer.negative.offset");
                LLVMValueRef previous = LLVMBuildAtomicRMW(builder_, LLVMAtomicRMWBinOpAdd, address, offset,
                    LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(previous, 8);
                if (target->volatileAccess) LLVMSetVolatile(previous, 1);
                LLVMValueRef result = node->booleanValue ? previous :
                    LLVMBuildAdd(builder_, previous, offset, "atomic.pointer.updated");
                return LLVMBuildIntToPtr(builder_, result, pointerType, "atomic.pointer.result");
            }
            if (target->typeKey == "builtin:float" || target->typeKey == "builtin:double")
            {
                const unsigned alignment = target->typeKey == "builtin:float" ? 4U : 8U;
                if (target->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
                    target->cppAtomicAlignmentBytes != std::optional<std::size_t>{alignment} || node->immediate != 0)
                    return failValue(error, "C atomic floating compound operation lacks supported alignment");
                LLVMValueRef operand = convert(value, node->children[1]->typeKey, target->typeKey, error);
                LLVMTypeRef floatingType = llvmType(target->typeKey, error);
                if (operand == nullptr || floatingType == nullptr) return nullptr;
                LLVMTypeRef storage = alignment == 4 ? LLVMInt32TypeInContext(context_) : LLVMInt64TypeInContext(context_);
                LLVMValueRef initial = LLVMBuildLoad2(builder_, storage, address, "atomic.fp.initial");
                LLVMSetOrdering(initial, LLVMAtomicOrderingSequentiallyConsistent);
                LLVMSetAlignment(initial, alignment);
                if (target->volatileAccess) LLVMSetVolatile(initial, 1);
                LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
                LLVMBasicBlockRef retry = appendBlock("atomic.fp.retry");
                LLVMBasicBlockRef done = appendBlock("atomic.fp.done");
                LLVMBuildBr(builder_, retry);
                enterBlock(retry);
                LLVMValueRef current = LLVMBuildPhi(builder_, storage, "atomic.fp.current");
                LLVMAddIncoming(current, &initial, &preheader, 1);
                LLVMValueRef previous = LLVMBuildBitCast(builder_, current, floatingType, "atomic.fp.previous");
                LLVMValueRef updated = applyBinaryOperation(node->operation, previous, operand, target->typeKey, error, false);
                if (updated == nullptr) return nullptr;
                LLVMValueRef updatedBits = LLVMBuildBitCast(builder_, updated, storage, "atomic.fp.bits");
                LLVMValueRef pair = LLVMBuildAtomicCmpXchg(builder_, address, current, updatedBits,
                    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(pair, alignment);
                if (target->volatileAccess) LLVMSetVolatile(pair, 1);
                LLVMValueRef observed = LLVMBuildExtractValue(builder_, pair, 0, "atomic.fp.observed");
                LLVMValueRef success = LLVMBuildExtractValue(builder_, pair, 1, "atomic.fp.success");
                LLVMBasicBlockRef attempt = LLVMGetInsertBlock(builder_);
                LLVMBuildCondBr(builder_, success, done, retry);
                LLVMAddIncoming(current, &observed, &attempt, 1);
                enterBlock(done);
                return node->booleanValue ? previous : updated;
            }
            const auto narrowAlignment = (target->typeKey == "builtin:sbyte" || target->typeKey == "builtin:byte") ? 1U :
                (target->typeKey == "builtin:short" || target->typeKey == "builtin:ushort") ? 2U : 0U;
            if (narrowAlignment)
            {
                const auto& operandType = node->children[1]->typeKey;
                const bool shift = node->operation == lexer::TokenKind::LessLess || node->operation == lexer::TokenKind::GreaterGreater;
                const bool validOperand = operandType == "builtin:sbyte" || operandType == "builtin:byte" ||
                    operandType == "builtin:short" || operandType == "builtin:ushort" || operandType == "builtin:int" ||
                    operandType == "builtin:uint" || operandType == "builtin:long" || operandType == "builtin:ulong";
                if (target->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
                    target->cppAtomicAlignmentBytes != std::optional<std::size_t>{narrowAlignment} ||
                    node->immediate != 0 || !validOperand)
                    return failValue(error, "C narrow atomic compound operation requires an integer operand");
                LLVMTypeRef storage = llvmType(target->typeKey, error);
                if (storage == nullptr) return nullptr;
                const std::string calculationType = shift ? "builtin:int" :
                    operandType == "builtin:ulong" ? "builtin:ulong" :
                    operandType == "builtin:long" ? "builtin:long" :
                    operandType == "builtin:uint" ? "builtin:uint" : "builtin:int";
                LLVMValueRef operand = shift ? normalizeShiftCount(value, operandType, calculationType, error) :
                    convert(value, operandType, calculationType, error);
                if (operand == nullptr) return nullptr;
                LLVMValueRef initial = LLVMBuildLoad2(builder_, storage, address, "atomic.narrow.initial");
                LLVMSetOrdering(initial, LLVMAtomicOrderingSequentiallyConsistent);
                LLVMSetAlignment(initial, narrowAlignment);
                if (target->volatileAccess) LLVMSetVolatile(initial, 1);
                LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
                LLVMBasicBlockRef retry = appendBlock("atomic.narrow.retry");
                LLVMBasicBlockRef done = appendBlock("atomic.narrow.done");
                LLVMBuildBr(builder_, retry);
                enterBlock(retry);
                LLVMValueRef current = LLVMBuildPhi(builder_, storage, "atomic.narrow.current");
                LLVMAddIncoming(current, &initial, &preheader, 1);
                LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
                LLVMValueRef promoted = LLVMBuildZExt(builder_, current, i32, "atomic.narrow.promoted");
                promoted = convert(promoted, "builtin:int", calculationType, error);
                if (promoted == nullptr) return nullptr;
                LLVMValueRef computed = applyBinaryOperation(node->operation, promoted, operand,
                    calculationType, error, false);
                if (computed == nullptr) return nullptr;
                LLVMValueRef updated = LLVMBuildTrunc(builder_, computed, storage, "atomic.narrow.updated");
                LLVMValueRef pair = LLVMBuildAtomicCmpXchg(builder_, address, current, updated,
                    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(pair, narrowAlignment);
                if (target->volatileAccess) LLVMSetVolatile(pair, 1);
                LLVMValueRef observed = LLVMBuildExtractValue(builder_, pair, 0, "atomic.narrow.observed");
                LLVMValueRef success = LLVMBuildExtractValue(builder_, pair, 1, "atomic.narrow.success");
                LLVMBasicBlockRef attempt = LLVMGetInsertBlock(builder_);
                LLVMBuildCondBr(builder_, success, done, retry);
                LLVMAddIncoming(current, &observed, &attempt, 1);
                enterBlock(done);
                return node->booleanValue ? current : updated;
            }
            const auto alignment = (target->typeKey == "builtin:int" || target->typeKey == "builtin:uint") ? 4U :
                (target->typeKey == "builtin:long" || target->typeKey == "builtin:ulong") ? 8U : 0U;
            if (target->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
                !alignment || target->cppAtomicAlignmentBytes != std::optional<std::size_t>{alignment} || node->immediate != 0)
                return failValue(error, "C atomic global compound operation is not yet supported");
            const auto& rhsType = node->children[1]->typeKey;
            const bool shiftOperation = node->operation == lexer::TokenKind::LessLess || node->operation == lexer::TokenKind::GreaterGreater;
            const bool validRhs = rhsType == "builtin:sbyte" || rhsType == "builtin:byte" || rhsType == "builtin:short" ||
                rhsType == "builtin:ushort" || rhsType == "builtin:int" || rhsType == "builtin:uint" ||
                rhsType == "builtin:long" || rhsType == "builtin:ulong";
            if (!validRhs) return failValue(error, "C atomic compound operation requires an integer operand");
            const std::string calculationType = shiftOperation ? target->typeKey :
                (target->typeKey == "builtin:ulong" || rhsType == "builtin:ulong") ? "builtin:ulong" :
                (target->typeKey == "builtin:long" || rhsType == "builtin:long") ? "builtin:long" :
                (target->typeKey == "builtin:uint" || rhsType == "builtin:uint") ? "builtin:uint" : "builtin:int";
            if (calculationType != target->typeKey)
            {
                LLVMTypeRef storage = llvmType(target->typeKey, error);
                if (storage == nullptr) return nullptr;
                LLVMValueRef operand = convert(value, rhsType, calculationType, error);
                if (operand == nullptr) return nullptr;
                LLVMValueRef initial = LLVMBuildLoad2(builder_, storage, address, "atomic.mixed.initial");
                LLVMSetOrdering(initial, LLVMAtomicOrderingSequentiallyConsistent);
                LLVMSetAlignment(initial, alignment);
                if (target->volatileAccess) LLVMSetVolatile(initial, 1);
                LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
                LLVMBasicBlockRef retry = appendBlock("atomic.mixed.retry");
                LLVMBasicBlockRef done = appendBlock("atomic.mixed.done");
                LLVMBuildBr(builder_, retry);
                enterBlock(retry);
                LLVMValueRef current = LLVMBuildPhi(builder_, storage, "atomic.mixed.current");
                LLVMAddIncoming(current, &initial, &preheader, 1);
                LLVMValueRef promoted = target->typeKey == "builtin:int" &&
                    (calculationType == "builtin:long" || calculationType == "builtin:ulong")
                    ? LLVMBuildZExt(builder_, current, LLVMInt64TypeInContext(context_), "atomic.mixed.promoted")
                    : convert(current, target->typeKey, calculationType, error);
                if (promoted == nullptr) return nullptr;
                LLVMValueRef computed = applyBinaryOperation(node->operation, promoted, operand, calculationType, error, false);
                if (computed == nullptr) return nullptr;
                LLVMValueRef updated = convert(computed, calculationType, target->typeKey, error);
                if (updated == nullptr) return nullptr;
                LLVMValueRef pair = LLVMBuildAtomicCmpXchg(builder_, address, current, updated,
                    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(pair, alignment);
                if (target->volatileAccess) LLVMSetVolatile(pair, 1);
                LLVMValueRef observed = LLVMBuildExtractValue(builder_, pair, 0, "atomic.mixed.observed");
                LLVMValueRef success = LLVMBuildExtractValue(builder_, pair, 1, "atomic.mixed.success");
                LLVMBasicBlockRef attempt = LLVMGetInsertBlock(builder_);
                LLVMBuildCondBr(builder_, success, done, retry);
                LLVMAddIncoming(current, &observed, &attempt, 1);
                enterBlock(done);
                return node->booleanValue ? current : updated;
            }
            LLVMValueRef operandValue = (node->operation == lexer::TokenKind::LessLess || node->operation == lexer::TokenKind::GreaterGreater)
                ? normalizeShiftCount(value, node->children[1]->typeKey, target->typeKey, error)
                : convert(value, node->children[1]->typeKey, target->typeKey, error);
            if (operandValue == nullptr) return nullptr;
            if (node->operation == lexer::TokenKind::Star ||
                node->operation == lexer::TokenKind::LessLess ||
                node->operation == lexer::TokenKind::GreaterGreater ||
                node->operation == lexer::TokenKind::Slash ||
                node->operation == lexer::TokenKind::Percent)
            {
                LLVMTypeRef type = llvmType(target->typeKey, error);
                if (type == nullptr) return nullptr;
                LLVMValueRef initial = LLVMBuildLoad2(builder_, type, address, "atomic.mul.initial");
                LLVMSetOrdering(initial, LLVMAtomicOrderingSequentiallyConsistent);
                LLVMSetAlignment(initial, alignment);
                if (target->volatileAccess) LLVMSetVolatile(initial, 1);
                LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
                LLVMBasicBlockRef retry = appendBlock("atomic.mul.retry");
                LLVMBasicBlockRef done = appendBlock("atomic.mul.done");
                LLVMBuildBr(builder_, retry);
                enterBlock(retry);
                LLVMValueRef current = LLVMBuildPhi(builder_, type, "atomic.mul.current");
                LLVMAddIncoming(current, &initial, &preheader, 1);
                LLVMValueRef updated = nullptr;
                switch (node->operation)
                {
                    case lexer::TokenKind::Star:
                        updated = LLVMBuildMul(builder_, current, operandValue, "atomic.mul.updated"); break;
                    case lexer::TokenKind::LessLess:
                        updated = LLVMBuildShl(builder_, current, operandValue, "atomic.shift.updated"); break;
                    case lexer::TokenKind::GreaterGreater:
                        updated = isSignedInteger(target->typeKey)
                            ? LLVMBuildAShr(builder_, current, operandValue, "atomic.shift.updated")
                            : LLVMBuildLShr(builder_, current, operandValue, "atomic.shift.updated"); break;
                    default:
                        updated = applyIntegerDivision(node->operation, current, operandValue, target->typeKey, error, false);
                        if (updated == nullptr) return nullptr;
                        break;
                }
                LLVMValueRef pair = LLVMBuildAtomicCmpXchg(builder_, address, current, updated,
                    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
                LLVMSetAlignment(pair, alignment);
                if (target->volatileAccess) LLVMSetVolatile(pair, 1);
                LLVMValueRef observed = LLVMBuildExtractValue(builder_, pair, 0, "atomic.mul.observed");
                LLVMValueRef success = LLVMBuildExtractValue(builder_, pair, 1, "atomic.mul.success");
                LLVMBasicBlockRef attempt = LLVMGetInsertBlock(builder_);
                LLVMBuildCondBr(builder_, success, done, retry);
                LLVMAddIncoming(current, &observed, &attempt, 1);
                enterBlock(done);
                return node->booleanValue ? current : updated;
            }
            LLVMAtomicRMWBinOp operation;
            switch (node->operation)
            {
                case lexer::TokenKind::Plus:
                case lexer::TokenKind::Minus: operation = LLVMAtomicRMWBinOpAdd; break;
                case lexer::TokenKind::Amp: operation = LLVMAtomicRMWBinOpAnd; break;
                case lexer::TokenKind::Pipe: operation = LLVMAtomicRMWBinOpOr; break;
                case lexer::TokenKind::Caret: operation = LLVMAtomicRMWBinOpXor; break;
                default: return failValue(error, "C atomic global compound operation is not yet supported");
            }
            LLVMValueRef operand = node->operation == lexer::TokenKind::Minus
                ? LLVMBuildNeg(builder_, operandValue, "atomic.subtract.operand") : operandValue;
            LLVMValueRef previous = LLVMBuildAtomicRMW(builder_, operation, address, operand,
                                                       LLVMAtomicOrderingSequentiallyConsistent, 0);
            LLVMSetAlignment(previous, alignment);
            if (target->volatileAccess) LLVMSetVolatile(previous, 1);
            if (node->booleanValue) return previous;
            switch (operation)
            {
                case LLVMAtomicRMWBinOpAnd: return LLVMBuildAnd(builder_, previous, operand, "atomic.updated");
                case LLVMAtomicRMWBinOpOr: return LLVMBuildOr(builder_, previous, operand, "atomic.updated");
                case LLVMAtomicRMWBinOpXor: return LLVMBuildXor(builder_, previous, operand, "atomic.updated");
                default: return LLVMBuildAdd(builder_, previous, operand, "atomic.updated");
            }
        }
        LLVMTypeRef targetType = llvmType(node->typeKey, error);
        if (targetType == nullptr) return nullptr;
        const bool bitField =
            node->children[0]->kind == frontend::ProjectOopMirNodeKind::BitFieldAddress;
        LLVMValueRef previousStorage = nullptr;
        LLVMValueRef previous = nullptr;
        if (bitField)
        {
            LLVMTypeRef storageType = bitFieldStorageType(node->children[0].get(), error);
            if (storageType == nullptr) return nullptr;
            previousStorage = LLVMBuildLoad2(
                builder_, storageType, address, "bitfield.compound.storage.load");
            LLVMSetAlignment(
                previousStorage, static_cast<unsigned>(node->children[0]->alignment));
            if (node->children[0]->volatileAccess) LLVMSetVolatile(previousStorage, 1);
            previous = extractBitField(node->children[0].get(), previousStorage, error);
        }
        else
        {
            previous = LLVMBuildLoad2(builder_, targetType, address, "compound.load");
            if (const auto alignment = fieldArrayElementAlignment(node->children[0].get()))
                LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
            if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::FieldAddress &&
                node->children[0]->alignment != 0)
                if (const auto alignment = fieldOrArrayAddressAlignment(node->children[0].get()))
                    LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
            if (node->children[0]->volatileAccess) LLVMSetVolatile(previous, 1);
            if (const auto width = clangMsVolatileRecordFieldWidth(memberModel_, nameLookup_, node->children[0].get()))
            {
                LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
                LLVMSetAlignment(previous, static_cast<unsigned>(*width));
            }
            if (const auto alignment = clangMsVolatileFunctionFieldAlignment(memberModel_, node->children[0].get()))
            {
                LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
                LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangMsVolatileEnumElementAlignment(node->children[0].get()))
            {
                LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
                LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangMsVolatileGlobalScalarArrayAlignment(node->children[0].get()))
            {
                LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
                LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangVolatileDirectCPointeeAlignment(node->children[0].get()))
            {
                LLVMSetAlignment(previous, static_cast<unsigned>(*alignment));
                if (node->children[0]->cppMicrosoftVolatileSemantics == std::optional<bool>{true})
                    LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
            }
            if (isClangMsVolatileScalarGlobal(node->children[0].get()))
            {
                LLVMSetOrdering(previous, LLVMAtomicOrderingAcquire);
                LLVMSetAlignment(previous, static_cast<unsigned>(*node->children[0]->cppAtomicAlignmentBytes));
            }
        }
        if (previous == nullptr) return nullptr;

        LLVMValueRef updated = compoundUpdatedValue(node, previous, value, error);
        if (updated == nullptr) return nullptr;
        LLVMValueRef storedValue = updated;
        if (bitField)
        {
            const auto* field = node->children[0].get();
            if (field->volatileAccess && field->cppMicrosoftVolatileSemantics.has_value())
            {
                LLVMTypeRef storageType = bitFieldStorageType(field, error);
                if (storageType == nullptr) return nullptr;
                previousStorage = LLVMBuildLoad2(
                    builder_, storageType, address, "bitfield.compound.storage.reload");
                LLVMSetAlignment(previousStorage, static_cast<unsigned>(field->alignment));
                LLVMSetVolatile(previousStorage, 1);
            }
            LLVMValueRef merged = insertBitField(
                node->children[0].get(), previousStorage, updated, error);
            if (merged == nullptr) return nullptr;
            LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
            LLVMSetAlignment(store, static_cast<unsigned>(node->children[0]->alignment));
            if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
            storedValue = extractBitField(node->children[0].get(), merged, error);
        }
        else
        {
            LLVMValueRef store = LLVMBuildStore(builder_, updated, address);
            if (const auto alignment = fieldArrayElementAlignment(node->children[0].get()))
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::FieldAddress &&
                node->children[0]->alignment != 0)
                if (const auto alignment = fieldOrArrayAddressAlignment(node->children[0].get()))
                    LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
            if (const auto width = clangMsVolatileRecordFieldWidth(memberModel_, nameLookup_, node->children[0].get()))
            {
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
                LLVMSetAlignment(store, static_cast<unsigned>(*width));
            }
            if (const auto alignment = clangMsVolatileFunctionFieldAlignment(memberModel_, node->children[0].get()))
            {
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangMsVolatileEnumElementAlignment(node->children[0].get()))
            {
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangMsVolatileGlobalScalarArrayAlignment(node->children[0].get()))
            {
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            }
            if (const auto alignment = clangVolatileDirectCPointeeAlignment(node->children[0].get()))
            {
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
                if (node->children[0]->cppMicrosoftVolatileSemantics == std::optional<bool>{true})
                    LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            }
            if (isClangMsVolatileScalarGlobal(node->children[0].get()))
            {
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
                LLVMSetAlignment(store, static_cast<unsigned>(*node->children[0]->cppAtomicAlignmentBytes));
            }
        }
        if (storedValue == nullptr) return nullptr;
        return node->booleanValue ? previous : storedValue;
    }

LLVMValueRef OopLlvmImplementation::compoundUpdatedValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef previous,
        LLVMValueRef value,
        std::string& error)
{
        LLVMValueRef updated = nullptr;
        if (node->immediate != 0)
        {
            if (node->operation != lexer::TokenKind::Plus &&
                node->operation != lexer::TokenKind::Minus)
                return failValue(error, "a pointer target supports only '+' and '-' updates");
            LLVMValueRef index = normalizeNativeIndexFromValue(node->children[1].get(), value, error);
            if (index == nullptr) return nullptr;
            LLVMValueRef stride = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
            LLVMValueRef offset = LLVMBuildMul(builder_, index, stride, "compound.offset");
            if (node->operation == lexer::TokenKind::Minus)
            {
                offset = LLVMBuildNeg(builder_, offset, "compound.offset.neg");
            }
            LLVMValueRef indices[] = {offset};
            updated = LLVMBuildGEP2(
                builder_, LLVMInt8TypeInContext(context_), previous, indices, 1, "compound.arith");
        }
        else
        {
            // An enum target is updated in its underlying integer: shift width, signed right shift and signed
            // division come from that type.
            // Un destino enum se actualiza en su entero subyacente: el ancho del desplazamiento, el desplazamiento
            // a la derecha con signo y la división con signo salen de ese tipo.
            const auto* enumInfo = findEnumUnderlyingInfo(nameLookup_, node->typeKey);
            const std::string valueType = enumInfo ? std::string(enumInfo->key) : node->typeKey;
            LLVMValueRef operand = value;
            if (node->operation == lexer::TokenKind::LessLess ||
                node->operation == lexer::TokenKind::GreaterGreater)
            {
                operand = normalizeShiftCount(
                    operand, node->children[1]->typeKey, valueType, error);
                if (operand == nullptr) return nullptr;
            }
            updated = applyBinaryOperation(
                node->operation, previous, operand, valueType, error, false);
        }
        return updated;
    }

LLVMValueRef OopLlvmImplementation::stackAllocationFromParts(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>& rawExtents,
        std::string& error)
{
        constexpr std::uint64_t kMaximumStackAllocationBytes = std::uint64_t{1} << 20;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef total = LLVMConstInt(i64, 1, 0);
        const std::uint64_t maximumElements =
            kMaximumStackAllocationBytes / node->immediate;
        LLVMValueRef maximum = LLVMConstInt(i64, maximumElements, 0);
        for (std::size_t extentIndex = 0; extentIndex < node->children.size(); ++extentIndex)
        {
            const frontend::ProjectOopMirNode* extentNode = node->children[extentIndex].get();
            LLVMValueRef rawExtent = rawExtents[extentIndex];
            if (isSignedInteger(extentNode->typeKey))
            {
                LLVMTypeRef extentType = llvmType(extentNode->typeKey, error);
                if (extentType == nullptr) return nullptr;
                LLVMValueRef zero = LLVMConstNull(extentType);
                LLVMValueRef nonNegative = LLVMBuildICmp(
                    builder_, LLVMIntSGE, rawExtent, zero, "stack.extent.nonnegative");
                if (!emitGuard(
                        nonNegative,
                        LlvmTrapReason::AllocationSizeOverflow,
                        "stack.extent",
                        error))
                    return nullptr;
            }
            LLVMValueRef extent = convert(
                rawExtent, extentNode->typeKey, "builtin:nuint", error);
            if (extent == nullptr) return nullptr;

            LLVMValueRef totalIsZero = LLVMBuildICmp(
                builder_, LLVMIntEQ, total, LLVMConstInt(i64, 0, 0),
                "stack.total.zero");
            LLVMValueRef safeDivisor = LLVMBuildSelect(
                builder_, totalIsZero, LLVMConstInt(i64, 1, 0), total,
                "stack.total.divisor");
            LLVMValueRef remaining = LLVMBuildUDiv(
                builder_, maximum, safeDivisor, "stack.extent.maximum");
            LLVMValueRef extentFits = LLVMBuildICmp(
                builder_, LLVMIntULE, extent, remaining, "stack.extent.fits");
            LLVMValueRef fits = LLVMBuildOr(
                builder_, totalIsZero, extentFits, "stack.extent.safe");
            if (!emitGuard(
                    fits,
                    LlvmTrapReason::AllocationSizeOverflow,
                    "stack.size",
                    error))
                return nullptr;
            total = LLVMBuildMul(builder_, total, extent, "stack.element.count");
        }
        LLVMValueRef bytes = LLVMBuildMul(
            builder_, total,
            LLVMConstInt(i64, node->immediate, 0),
            "stackalloc.bytes");
        LLVMValueRef storage = LLVMBuildArrayAlloca(
            builder_, LLVMInt8TypeInContext(context_), bytes, "stackalloc.storage");
        if (storage == nullptr)
            return failValue(error, "CC1 stackalloc failed to emit its array alloca");
        LLVMSetAlignment(storage, static_cast<unsigned>(node->alignment == 0 ? 1 : node->alignment));
        return storage;
    }

LLVMValueRef OopLlvmImplementation::storeFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error)
{
        // EN: Freeze 3.0 DEC-11: an element store of an array with live bits drops the old value
        //     only when it was alive, and leaves the slot alive.
        // ES: Congelamiento 3.0 DEC-11: guardar un elemento de un arreglo con bits de vida destruye
        //     el valor anterior solo si estaba vivo, y deja el slot vivo.
        const std::optional<OopLlvmArrayElementLiveness> liveness =
            takeArrayElementLiveness(node->children[0].get());
        if (terminated()) return value;
        if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::BitFieldAddress)
        {
            return storeBitFieldFromParts(node->children[0].get(), address, value, error);
        }
        if (node->booleanValue && node->localOrdinal.has_value())
        {
            const auto cleanup = std::find_if(
                cleanupRegions_.begin(), cleanupRegions_.end(),
                [&](const auto& entry)
                {
                    return entry.second != nullptr &&
                        entry.second->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                        entry.second->localOrdinal == node->localOrdinal;
                });
            if (cleanup == cleanupRegions_.end() || !emitRaiiDestroy(cleanup->second, error)) return nullptr;
        }
        else if (node->booleanValue && memberModel_ != nullptr &&
                 memberModel_->typeRequiresValueDrop(node->typeKey))
        {
            if (liveness.has_value())
            {
                if (!dropReplacedArrayElement(address, node->typeKey, *liveness, error)) return nullptr;
            }
            else if (!emitValueDropAddress(address, node->typeKey, "value.replace", error))
                return nullptr;
        }
        else if (node->booleanValue && requiresOwnedDestruction(node->typeKey))
        {
            LLVMTypeRef ptrType = LLVMPointerTypeInContext(context_, 0);
            LLVMValueRef previous = LLVMBuildLoad2(
                builder_, ptrType, address, "owned.field.previous");
            LLVMValueRef previousIsNull = LLVMBuildICmp(
                builder_, LLVMIntEQ, previous, LLVMConstNull(ptrType),
                "owned.field.previous.isnull");
            LLVMBasicBlockRef releaseBlock = appendBlock("owned.field.replace");
            LLVMBasicBlockRef readyBlock = appendBlock("owned.field.ready");
            LLVMBuildCondBr(builder_, previousIsNull, readyBlock, releaseBlock);

            enterBlock(releaseBlock);
            LLVMBuildStore(builder_, LLVMConstNull(ptrType), address);
            if (node->typeKey == "builtin:string")
            {
                if (deallocationFunction_ == nullptr)
                {
                    return failValue(error, "Bloque 1 owned string overwrite has no deallocation ABI");
                }
                LLVMValueRef stringArguments[] = {previous};
                buildNoUnwindCall(
                    deallocationFunctionType_, deallocationFunction_, stringArguments, 1, "");
            }
            else
            {
                if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                {
                    return failValue(error, "Bloque 1 owned field overwrite has no destruction ABI");
                }
                LLVMValueRef previousDescriptor = LLVMBuildLoad2(
                    builder_, ptrType, previous, "owned.field.previous.descriptor");
                LLVMValueRef previousEntryAddress = LLVMBuildStructGEP2(
                    builder_, descriptorType_, previousDescriptor, 5,
                    "owned.field.previous.entry.addr");
                LLVMValueRef previousEntry = LLVMBuildLoad2(
                    builder_, ptrType, previousEntryAddress, "owned.field.previous.entry");
                LLVMValueRef releaseArguments[] = {previous};
                buildNoUnwindCall(
                    destructionEntryFunctionType_, previousEntry, releaseArguments, 1, "");
            }
            LLVMBuildBr(builder_, readyBlock);
            enterBlock(readyBlock);
        }
        const auto* boolTarget = node->children[0].get();
        const bool cVolatileBool = clangMsVolatileGlobalBoolArray(boolTarget) ||
            (!boolTarget->cppAtomicAccess && boolTarget->kind == frontend::ProjectOopMirNodeKind::NamespaceStateAddress &&
             boolTarget->typeKey == "builtin:bool" && boolTarget->volatileAccess &&
             boolTarget->cppMicrosoftVolatileSemantics == std::optional<bool>{true} &&
             boolTarget->cppAtomicAlignmentBytes == std::optional<std::size_t>{1});
        const bool boolPointer = node->typeKey == "builtin:bool" &&
            (boolTarget->kind == frontend::ProjectOopMirNodeKind::RawElementAddress ||
             boolTarget->kind == frontend::ProjectOopMirNodeKind::AddressValue ||
             boolTarget->typeKey == "builtin:bool*");
        const bool cBool = boolPointer || cBoolGlobalArrayElement(boolTarget) ||
            (boolTarget->kind == frontend::ProjectOopMirNodeKind::NamespaceStateAddress &&
             boolTarget->cppCBoolStorage && boolTarget->typeKey == "builtin:bool");
        LLVMValueRef storedValue = (cBool || cVolatileBool || cRecordBoolStorage(nameLookup_, boolTarget))
            ? LLVMBuildZExt(builder_, value, LLVMInt8TypeInContext(context_), "c.bool.storage")
            : value;
        const auto& storageTypeKey = node->children[0]->typeKey;
        if (node->children[0]->cppAtomicAccess && storageTypeKey.ends_with("*"))
            storedValue = LLVMBuildPtrToInt(builder_, value, LLVMInt64TypeInContext(context_), "atomic.pointer.bits");
        if (node->children[0]->cppAtomicAccess && storageTypeKey == "builtin:bool")
            storedValue = LLVMBuildZExt(builder_, value, LLVMInt8TypeInContext(context_), "atomic.bool.bits");
        if (node->children[0]->cppAtomicAccess &&
            (storageTypeKey == "builtin:float" || storageTypeKey == "builtin:double"))
            storedValue = LLVMBuildBitCast(builder_, value,
                storageTypeKey == "builtin:float" ? LLVMInt32TypeInContext(context_) : LLVMInt64TypeInContext(context_),
                "atomic.float.bits");
        LLVMValueRef store = LLVMBuildStore(builder_, storedValue, address);
        if (node->children[0]->cppAtomicAccess)
        {
            const auto& typeKey = node->children[0]->typeKey;
            const auto* enumInfo = findEnumUnderlyingInfo(nameLookup_, typeKey);
            const auto alignment = enumInfo ? enumInfo->width / 8 : typeKey.ends_with("*") ? 8U : typeKey == "builtin:bool" ? 1U : typeKey == "builtin:float" ? 4U : typeKey == "builtin:double" ? 8U :
                (typeKey == "builtin:sbyte" || typeKey == "builtin:byte") ? 1U :
                (typeKey == "builtin:short" || typeKey == "builtin:ushort") ? 2U :
                (typeKey == "builtin:int" || typeKey == "builtin:uint") ? 4U :
                (typeKey == "builtin:long" || typeKey == "builtin:ulong") ? 8U : 0U;
            if (!alignment || node->children[0]->cppAtomicAlignmentBytes != std::optional<std::size_t>{alignment})
                return failValue(error, "C atomic global store lacks supported type or alignment");
            LLVMSetOrdering(store, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMSetAlignment(store, alignment);
            if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
            return value;
        }
        if (cVolatileBool)
        {
            LLVMSetVolatile(store, 1);
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, 1);
        }
        if (const auto alignment = fieldArrayElementAlignment(node->children[0].get()))
            LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
        if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::FieldAddress &&
            node->children[0]->alignment != 0)
            if (const auto alignment = fieldOrArrayAddressAlignment(node->children[0].get()))
                LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
        if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
        if (const auto width = clangMsVolatileRecordFieldWidth(memberModel_, nameLookup_, node->children[0].get()))
        {
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, static_cast<unsigned>(*width));
        }
        if (const auto alignment = clangMsVolatileFunctionFieldAlignment(memberModel_, node->children[0].get()))
        {
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
        }
        if (const auto alignment = clangMsVolatileEnumElementAlignment(node->children[0].get()))
        {
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
        }
        if (const auto alignment = clangMsVolatileGlobalScalarArrayAlignment(node->children[0].get()))
        {
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
        }
        if (const auto alignment = clangVolatileDirectCPointeeAlignment(node->children[0].get()))
        {
            LLVMSetAlignment(store, static_cast<unsigned>(*alignment));
            if (node->children[0]->cppMicrosoftVolatileSemantics == std::optional<bool>{true})
                LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
        }
        if (isClangMsVolatileScalarGlobal(node->children[0].get()))
        {
            LLVMSetOrdering(store, LLVMAtomicOrderingRelease);
            LLVMSetAlignment(store, static_cast<unsigned>(*node->children[0]->cppAtomicAlignmentBytes));
        }
        if (liveness.has_value()) setArrayElementAlive(*liveness, true);
        if (node->localOrdinal.has_value())
        {
            const auto flag = localValueDropFlags_.find(*node->localOrdinal);
            if (flag != localValueDropFlags_.end())
                LLVMBuildStore(
                    builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0),
                    flag->second);
        }
        return value;
    }

LLVMValueRef OopLlvmImplementation::storeBitFieldFromParts(
        const frontend::ProjectOopMirNode* field,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error)
{
        LLVMTypeRef storageType = bitFieldStorageType(field, error);
        if (storageType == nullptr) return nullptr;
        LLVMValueRef previous = LLVMBuildLoad2(
            builder_, storageType, address, "bitfield.store.storage.load");
        LLVMSetAlignment(previous, static_cast<unsigned>(field->alignment));
        if (field->volatileAccess) LLVMSetVolatile(previous, 1);
        LLVMValueRef merged = insertBitField(field, previous, value, error);
        if (merged == nullptr) return nullptr;
        LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
        LLVMSetAlignment(store, static_cast<unsigned>(field->alignment));
        if (field->volatileAccess) LLVMSetVolatile(store, 1);
        return extractBitField(field, merged, error);
    }

LLVMValueRef OopLlvmImplementation::tupleDeconstructFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef sourceValue,
        const std::vector<LLVMValueRef>& targetAddresses,
        std::string& error)
{
        const frontend::ProjectOopMirStructLayout* sourceLayout = findStructLayout(mirModel_, node->typeKey);
        LLVMTypeRef sourceCarrier = llvmType(node->typeKey, error);
        if (sourceLayout == nullptr || sourceCarrier == nullptr)
            return failValue(error, "tuple deconstruction source has no physical carrier");
        if (terminated()) return sourceValue;
        LLVMValueRef sourceSlot = entryBlockAlloca(
            sourceCarrier,
            static_cast<unsigned>(sourceLayout->alignment),
            "tuple.deconstruct.source");
        LLVMValueRef sourceStore = LLVMBuildStore(builder_, sourceValue, sourceSlot);
        if (sourceLayout->alignment > 1U)
            LLVMSetAlignment(sourceStore, static_cast<unsigned>(sourceLayout->alignment));
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* element = node->children[index].get();
            LLVMTypeRef sourceType = llvmType(element->elementTypeKey, error);
            if (sourceType == nullptr) return nullptr;
            LLVMValueRef sourceAddress = byteOffset(
                sourceSlot, element->immediate, "tuple.deconstruct.element.source");
            LLVMValueRef value = LLVMBuildLoad2(
                builder_, sourceType, sourceAddress, "tuple.deconstruct.element.value");
            const auto sourceField = std::ranges::find_if(
                sourceLayout->fields,
                [&](const frontend::ObjectFieldLayout& field)
                {
                    return field.offset == element->immediate &&
                        field.typeKey == element->elementTypeKey;
                });
            if (sourceField != sourceLayout->fields.end() && sourceField->alignment > 1U)
                LLVMSetAlignment(value, static_cast<unsigned>(sourceField->alignment));
            value = convert(value, element->elementTypeKey, element->typeKey, error);
            if (value == nullptr) return nullptr;
            LLVMValueRef store = LLVMBuildStore(builder_, value, targetAddresses[index - 1]);
            const frontend::ProjectOopMirStructLayout* targetStruct =
                findStructLayout(mirModel_, element->typeKey);
            if (targetStruct != nullptr && targetStruct->alignment > 1U)
                LLVMSetAlignment(store, static_cast<unsigned>(targetStruct->alignment));
        }
        return sourceValue;
    }

LLVMValueRef OopLlvmImplementation::collectArrayAllocationCount(
        const frontend::ProjectOopMirNode* node,
        std::vector<LLVMValueRef>& segmentValues,
        std::vector<LLVMValueRef>& segmentLengths,
        std::string& error)
{
        const bool hasCollectionSegments = !node->collectionSegments.empty();
        if (hasCollectionSegments)
        {
            // Evaluate every source segment exactly once and in source order before allocation.
            LLVMValueRef count = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            segmentValues.reserve(node->children.size());
            segmentLengths.reserve(node->children.size());
            for (std::size_t index = 0; index < node->children.size(); ++index)
            {
                LLVMValueRef value = lowerExpression(node->children[index].get(), error);
                if (value == nullptr) return nullptr;
                segmentValues.push_back(value);
                LLVMValueRef length = LLVMConstInt(LLVMInt64TypeInContext(context_), 1, 0);
                if (node->collectionSegments[index] ==
                    frontend::ProjectCollectionSegmentKind::Spread)
                {
                    length = LLVMBuildLoad2(
                        builder_, LLVMInt64TypeInContext(context_), value,
                        "array.spread.length");
                }
                segmentLengths.push_back(length);
                LLVMValueRef nextCount = LLVMBuildAdd(
                    builder_, count, length, "array.segment.count");
                LLVMValueRef noOverflow = LLVMBuildICmp(
                    builder_, LLVMIntUGE, nextCount, count, "array.segment.count.valid");
                if (!emitGuard(
                        noOverflow, LlvmTrapReason::AllocationSizeOverflow,
                        "array.segment.count", error))
                    return nullptr;
                count = nextCount;
            }
            return count;
        }
        LLVMValueRef count = normalizeNativeIndex(node->children.front().get(), error);
        if (count == nullptr) return nullptr;
        if (isSignedInteger(node->children.front()->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(
                builder_, LLVMIntSGE, count, zero, "array.count.nonnegative");
            if (!emitGuard(
                    nonNegative, LlvmTrapReason::AllocationSizeOverflow, "array.count", error))
                return nullptr;
        }
        return count;
    }

bool OopLlvmImplementation::initializeArrayElements(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef memory,
        std::uint64_t dataOffset,
        const std::vector<LLVMValueRef>& segmentValues,
        const std::vector<LLVMValueRef>& segmentLengths,
        std::string& error)
{
        LLVMTypeRef elementType = llvmType(node->elementTypeKey, error);
        if (elementType == nullptr) return false;
        if (!node->collectionSegments.empty())
        {
            LLVMValueRef cursor = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            for (std::size_t index = 0; index < segmentValues.size(); ++index)
            {
                LLVMValueRef byteBase = LLVMBuildMul(
                    builder_, cursor,
                    LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                    "array.segment.bytebase");
                LLVMValueRef offset = LLVMBuildAdd(
                    builder_, byteBase, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0),
                    "array.segment.offset");
                LLVMValueRef destination = dynamicByteOffset(memory, offset, "array.segment.dest");
                if (node->collectionSegments[index] == frontend::ProjectCollectionSegmentKind::Spread)
                {
                    LLVMValueRef sourceData = byteOffset(
                        segmentValues[index], dataOffset, "array.spread.source");
                    LLVMValueRef sourceBytes = LLVMBuildMul(
                        builder_, segmentLengths[index],
                        LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                        "array.spread.bytes");
                    if (!emitMemcpyBytes(
                            destination, sourceData, sourceBytes, "array.spread.copy", error))
                        return false;
                }
                else
                {
                    LLVMBuildStore(builder_, segmentValues[index], destination);
                }
                cursor = LLVMBuildAdd(
                    builder_, cursor, segmentLengths[index], "array.segment.cursor");
            }
            return true;
        }
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return false;
            const std::uint64_t bytePosition = dataOffset + (index - 1U) * node->immediate;
            LLVMValueRef address = byteOffset(memory, bytePosition, "array.init.element.addr");
            LLVMBuildStore(builder_, value, address);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerArrayAllocate(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.empty() || node->immediate == 0 || node->alignment == 0)
            return failValue(error, "Stage 41 array allocation has invalid MIR layout/count");
        const bool hasCollectionSegments = !node->collectionSegments.empty();
        if (!node->booleanValue && !hasCollectionSegments && node->children.size() != 1)
            return failValue(error, "Stage 41 counted array allocation cannot carry initializer elements");
        if (hasCollectionSegments && node->collectionSegments.size() != node->children.size())
            return failValue(error, "T09-F05 collection segment metadata does not match its children");
        std::vector<LLVMValueRef> segmentValues;
        std::vector<LLVMValueRef> segmentLengths;
        LLVMValueRef count = collectArrayAllocationCount(
            node, segmentValues, segmentLengths, error);
        if (count == nullptr) return nullptr;
        const std::uint64_t dataOffset = (8U + node->alignment - 1U) / node->alignment * node->alignment;
        const std::uint64_t storageMaxCount = (UINT64_MAX - dataOffset) / node->immediate;
        const std::uint64_t maxCount = std::min<std::uint64_t>(storageMaxCount, 0x7fffffffULL);
        LLVMValueRef fits = LLVMBuildICmp(
            builder_, LLVMIntULE, count,
            LLVMConstInt(LLVMInt64TypeInContext(context_), maxCount, 0), "array.count.fits");
        if (!emitGuard(fits, LlvmTrapReason::AllocationSizeOverflow, "array.size", error)) return nullptr;
        LLVMValueRef bytes = LLVMBuildMul(
            builder_, count, LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0), "array.data.bytes");
        LLVMValueRef total = LLVMBuildAdd(
            builder_, bytes, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0), "array.total.bytes");
        // EN: Freeze 3.0 DEC-11: an array of structs with a destructor carries one live bit per
        //     element after its data. The block is zeroed below, so every slot starts dead.
        // ES: Congelamiento 3.0 DEC-11: un arreglo de structs con destructor lleva un bit de vida por
        //     elemento despues de sus datos. El bloque se pone en cero abajo: todo slot nace muerto.
        const bool tracked = arrayElementLivenessTracked(arrayAllocationElementType(node));
        LLVMValueRef livenessBytes = tracked ? arrayLivenessByteCount(count) : nullptr;
        if (tracked) total = LLVMBuildAdd(builder_, total, livenessBytes, "array.total.live.bytes");
        LLVMValueRef args[] = {total};
        LLVMValueRef memory = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, args, 1, "array.memory");
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, memory, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "array.alloc.null");
        LLVMBasicBlockRef nullBlock = appendBlock("array.alloc.null");
        LLVMBasicBlockRef initBlock = appendBlock("array.alloc.init");
        LLVMBasicBlockRef mergeBlock = appendBlock("array.alloc.merge");
        LLVMBuildCondBr(builder_, isNull, nullBlock, initBlock);
        enterBlock(nullBlock);
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);
        enterBlock(initBlock);
        LLVMValueRef memsetArgs[] = {
            memory,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            total,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");
        LLVMBuildStore(builder_, count, memory);
        // NSC-LIMIT-017. The first child is the inferred count; the remaining children are
        // already converted element values. Initialize directly in the successful-allocation
        // block, in source order, so every expression is evaluated once and a null allocation is
        // never dereferenced.
        if (!initializeArrayElements(
                node, memory, dataOffset, segmentValues, segmentLengths, error)) return nullptr;
        if (tracked && (node->booleanValue || hasCollectionSegments))
        {
            // EN: Freeze 3.0 DEC-11: an initializer writes every element, so all start alive.
            // ES: Congelamiento 3.0 DEC-11: un inicializador escribe cada elemento: todos nacen vivos.
            LLVMValueRef bitmapOffset = LLVMBuildAdd(
                builder_, bytes, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0),
                "array.live.offset");
            LLVMValueRef bitmapIndices[] = {bitmapOffset};
            LLVMValueRef aliveArguments[] = {
                LLVMBuildGEP2(builder_, LLVMInt8TypeInContext(context_), memory, bitmapIndices, 1,
                    "array.live.bitmap"),
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0xFF, 0),
                livenessBytes,
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, aliveArguments, 4, "");
        }
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef initIncoming = LLVMGetInsertBlock(builder_);
        enterBlock(mergeBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, LLVMPointerTypeInContext(context_, 0), "array.result");
        LLVMValueRef incomingValues[] = {LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), memory};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, initIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

bool OopLlvmImplementation::collectRangeArrayDimensions(
        const frontend::ProjectOopMirNode* node,
        std::vector<LLVMValueRef>& dimensions,
        std::string& error)
{
        const std::size_t rank = node->nativeArrayRank;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        dimensions.reserve(rank);
        for (std::size_t dimension = 0; dimension < rank; ++dimension)
        {
            LLVMValueRef extent = normalizeNativeIndex(node->children[dimension].get(), error);
            if (extent == nullptr) return false;
            if (isSignedInteger(node->children[dimension]->typeKey))
            {
                LLVMValueRef nonNegative = LLVMBuildICmp(
                    builder_, LLVMIntSGE, extent, LLVMConstInt(i64, 0, 0),
                    "range.dimension.nonnegative");
                if (!emitGuard(nonNegative, LlvmTrapReason::AllocationSizeOverflow,
                        "range.dimension", error)) return false;
            }
            dimensions.push_back(extent);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::computeRangeArrayElementCount(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>& dimensions,
        std::uint64_t dataOffset,
        std::string& error)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        const std::uint64_t storageMaxCount = (UINT64_MAX - dataOffset) / node->immediate;
        const std::uint64_t maxCount = std::min<std::uint64_t>(storageMaxCount, 0x7fffffffULL);
        LLVMValueRef totalCount = LLVMConstInt(i64, 1, 0);
        for (LLVMValueRef extent : dimensions)
        {
            LLVMValueRef isZero = LLVMBuildICmp(
                builder_, LLVMIntEQ, extent, LLVMConstInt(i64, 0, 0), "range.dimension.zero");
            LLVMValueRef safeExtent = LLVMBuildSelect(
                builder_, isZero, LLVMConstInt(i64, 1, 0), extent, "range.dimension.safe");
            LLVMValueRef allowed = LLVMBuildUDiv(
                builder_, LLVMConstInt(i64, maxCount, 0), safeExtent, "range.product.allowed");
            LLVMValueRef fits = LLVMBuildICmp(
                builder_, LLVMIntULE, totalCount, allowed, "range.product.fits");
            if (!emitGuard(fits, LlvmTrapReason::AllocationSizeOverflow, "range.product", error))
                return nullptr;
            totalCount = LLVMBuildMul(builder_, totalCount, extent, "range.product");
        }
        return totalCount;
    }

void OopLlvmImplementation::initializeRangeArrayHeader(
        LLVMValueRef memory,
        LLVMValueRef totalBytes,
        LLVMValueRef totalCount,
        const std::vector<LLVMValueRef>& dimensions)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        const std::size_t rank = dimensions.size();
        LLVMValueRef memsetArguments[] = {
            memory, LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0), totalBytes,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArguments, 4, "");
        LLVMBuildStore(builder_, totalCount, memory);
        std::vector<LLVMValueRef> strides(rank);
        LLVMValueRef stride = LLVMConstInt(i64, 1, 0);
        for (std::size_t reverse = rank; reverse-- > 0;)
        {
            strides[reverse] = stride;
            stride = LLVMBuildMul(builder_, stride, dimensions[reverse], "range.stride.next");
        }
        for (std::size_t dimension = 0; dimension < rank; ++dimension)
        {
            LLVMBuildStore(builder_, dimensions[dimension],
                byteOffset(memory, 8U * (1U + dimension), "range.dimension.addr"));
            LLVMBuildStore(builder_, strides[dimension],
                byteOffset(memory, 8U * (1U + rank + dimension), "range.stride.addr"));
        }
    }

LLVMValueRef OopLlvmImplementation::lowerRangeArrayAllocate(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        const std::size_t rank = node->nativeArrayRank;
        if (rank < 2 || node->children.size() != rank || node->immediate == 0 ||
            node->alignment == 0)
            return failValue(error, "NativeSharp 3.0 rectangular allocation has invalid rank/layout");
        std::vector<LLVMValueRef> dimensions;
        if (!collectRangeArrayDimensions(node, dimensions, error)) return nullptr;
        const std::uint64_t rawHeader = 8U * (1U + 2U * rank);
        const std::uint64_t dataOffset =
            (rawHeader + node->alignment - 1U) / node->alignment * node->alignment;
        LLVMValueRef totalCount = computeRangeArrayElementCount(
            node, dimensions, dataOffset, error);
        if (totalCount == nullptr) return nullptr;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef bytes = LLVMBuildMul(
            builder_, totalCount, LLVMConstInt(i64, node->immediate, 0), "range.data.bytes");
        LLVMValueRef totalBytes = LLVMBuildAdd(
            builder_, bytes, LLVMConstInt(i64, dataOffset, 0), "range.total.bytes");
        // EN: Freeze 3.0 DEC-11: live bits after the data, zeroed with the block (all dead).
        // ES: Congelamiento 3.0 DEC-11: bits de vida tras los datos, en cero con el bloque (muertos).
        if (arrayElementLivenessTracked(arrayAllocationElementType(node)))
            totalBytes = LLVMBuildAdd(
                builder_, totalBytes, arrayLivenessByteCount(totalCount), "range.total.live.bytes");
        LLVMValueRef arguments[] = {totalBytes};
        LLVMValueRef memory = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, arguments, 1, "range.memory");
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, memory, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
            "range.alloc.null");
        LLVMBasicBlockRef nullBlock = appendBlock("range.alloc.null");
        LLVMBasicBlockRef initBlock = appendBlock("range.alloc.init");
        LLVMBasicBlockRef mergeBlock = appendBlock("range.alloc.merge");
        LLVMBuildCondBr(builder_, isNull, nullBlock, initBlock);
        enterBlock(nullBlock);
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        enterBlock(initBlock);
        initializeRangeArrayHeader(memory, totalBytes, totalCount, dimensions);
        LLVMBuildBr(builder_, mergeBlock);
        LLVMBasicBlockRef initIncoming = LLVMGetInsertBlock(builder_);
        enterBlock(mergeBlock);
        LLVMValueRef result = LLVMBuildPhi(
            builder_, LLVMPointerTypeInContext(context_, 0), "range.result");
        LLVMValueRef incomingValues[] = {
            LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), memory};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, initIncoming};
        LLVMAddIncoming(result, incomingValues, incomingBlocks, 2);
        return result;
    }

LLVMValueRef OopLlvmImplementation::lowerRangeArrayElementAddress(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        const std::size_t rank = node->nativeArrayRank;
        if (rank < 2 || node->children.size() != rank + 1)
            return failValue(error, "NativeSharp 3.0 rectangular index lost its rank");
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;
        object = checkedReceiver(object, error);
        if (object == nullptr) return nullptr;
        LLVMValueRef linear = LLVMConstInt(i64, 0, 0);
        for (std::size_t dimension = 0; dimension < rank; ++dimension)
        {
            const frontend::ProjectOopMirNode* indexNode = node->children[dimension + 1].get();
            LLVMValueRef index = normalizeNativeIndex(indexNode, error);
            if (index == nullptr) return nullptr;
            LLVMValueRef extent = LLVMBuildLoad2(builder_, i64,
                byteOffset(object, 8U * (1U + dimension), "range.index.dimension.addr"),
                "range.index.dimension");
            if (dimension < 64U &&
                (node->integerValue & (std::uint64_t{1} << dimension)) != 0U)
            {
                index = LLVMBuildSub(builder_, extent, index, "range.index.fromend");
            }
            LLVMValueRef valid = LLVMBuildICmp(
                builder_, LLVMIntULT, index, extent, "range.index.inrange");
            if (isSignedInteger(indexNode->typeKey))
            {
                LLVMValueRef nonNegative = LLVMBuildICmp(
                    builder_, LLVMIntSGE, index, LLVMConstInt(i64, 0, 0),
                    "range.index.nonnegative");
                valid = LLVMBuildAnd(builder_, valid, nonNegative, "range.index.valid");
            }
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "range.index", error))
                return nullptr;
            LLVMValueRef stride = LLVMBuildLoad2(builder_, i64,
                byteOffset(object, 8U * (1U + rank + dimension), "range.index.stride.addr"),
                "range.index.stride");
            linear = LLVMBuildAdd(builder_, linear,
                LLVMBuildMul(builder_, index, stride, "range.index.scaled"),
                "range.index.linear");
        }
        const std::uint64_t rawHeader = 8U * (1U + 2U * rank);
        const std::uint64_t dataOffset =
            (rawHeader + node->alignment - 1U) / node->alignment * node->alignment;
        LLVMValueRef bytePosition = LLVMBuildAdd(builder_,
            LLVMBuildMul(builder_, linear, LLVMConstInt(i64, node->immediate, 0),
                "range.index.byte"),
            LLVMConstInt(i64, dataOffset, 0), "range.index.offset");
        LLVMValueRef indices[] = {bytePosition};
        LLVMValueRef address = LLVMBuildGEP2(builder_, LLVMInt8TypeInContext(context_), object, indices, 1,
            "range.element.addr");
        if (!applyArrayElementAccess(node, object, address, dataOffset, node->immediate, error))
            return nullptr;
        return address;
    }

// EN: Freeze 3.0 DEC-11: whether arrays of `elementType` carry live bits: exactly the element
//     types that need a value drop (a struct with a destructor or owned fields).
// ES: Congelamiento 3.0 DEC-11: si los arreglos de `elementType` llevan bits de vida: justo los
//     tipos de elemento que requieren drop de valor (un struct con destructor o campos owned).
bool OopLlvmImplementation::arrayElementLivenessTracked(std::string_view elementType) const
{
        return memberModel_ != nullptr && !elementType.empty() &&
            memberModel_->typeRequiresValueDrop(elementType);
}

// EN: The element type of an ArrayAllocate/RangeArrayAllocate node: its element key, else the
//     element of its array type key. Empty when neither names one.
// ES: El tipo de elemento de un nodo ArrayAllocate/RangeArrayAllocate: su clave de elemento, o el
//     elemento de su clave de arreglo. Vacio si ninguna lo nombra.
std::string OopLlvmImplementation::arrayAllocationElementType(const frontend::ProjectOopMirNode* node) const
{
        if (!node->elementTypeKey.empty()) return node->elementTypeKey;
        return frontend::ProjectTypeConversion::nativeArrayElementType(node->typeKey).value_or("");
}

// EN: The element type of an element access node: the element of its array operand's type, else
//     its own element key.
// ES: El tipo de elemento de un nodo de acceso: el elemento del tipo de su operando arreglo, o su
//     propia clave de elemento.
std::string OopLlvmImplementation::arrayAccessElementType(const frontend::ProjectOopMirNode* node) const
{
        if (!node->children.empty() && node->children.front() != nullptr)
        {
            const std::optional<std::string> element =
                frontend::ProjectTypeConversion::nativeArrayElementType(node->children.front()->typeKey);
            if (element.has_value()) return *element;
        }
        return node->elementTypeKey;
}

// EN: Records, for every element access of a tracked array in the tree under `root`, what its
//     surrounding expression does with the element. Iterative (explicit stack); O(nodes). Entries
//     accumulate across trees (nodes are unique), so a tree lowered in the middle of another
//     never erases the outer one's entries.
// ES: Registra, para cada acceso a elemento de un arreglo con bits de vida en el arbol bajo
//     `root`, que hace con el elemento la expresion que lo rodea. Iterativo (pila explicita);
//     O(nodos). Las entradas se acumulan entre arboles (los nodos son unicos), asi un arbol bajado
//     a mitad de otro nunca borra las entradas del exterior.
void OopLlvmImplementation::classifyArrayElementAccesses(const frontend::ProjectOopMirNode* root)
{
        if (root == nullptr || memberModel_ == nullptr) return;
        std::unordered_map<const frontend::ProjectOopMirNode*, const frontend::ProjectOopMirNode*> parents;
        std::vector<const frontend::ProjectOopMirNode*> elements;
        std::vector<const frontend::ProjectOopMirNode*> work{root};
        while (!work.empty())
        {
            const frontend::ProjectOopMirNode* node = work.back();
            work.pop_back();
            const bool access = node->kind == frontend::ProjectOopMirNodeKind::ArrayElementAddress ||
                node->kind == frontend::ProjectOopMirNodeKind::ArrayElementLoad ||
                node->kind == frontend::ProjectOopMirNodeKind::RangeArrayElementAddress ||
                node->kind == frontend::ProjectOopMirNodeKind::RangeArrayElementLoad;
            if (access && arrayElementLivenessTracked(arrayAccessElementType(node))) elements.push_back(node);
            for (const auto& child : node->children)
            {
                if (child == nullptr) continue;
                parents.emplace(child.get(), node);
                work.push_back(child.get());
            }
        }
        for (const frontend::ProjectOopMirNode* element : elements)
            arrayElementAccess_[element] = arrayElementAccessFor(element, parents);
}

// EN: The access kind of one element node, from the nodes above it: a store target, the base of a
//     chain of field addresses under a store target, a move operand, a delete operand, or a read.
// ES: El tipo de acceso de un nodo de elemento segun los nodos de arriba: destino de un store, base
//     de una cadena de direcciones de campo bajo un destino de store, operando de move, operando de
//     delete, o una lectura.
OopLlvmArrayElementAccess OopLlvmImplementation::arrayElementAccessFor(
        const frontend::ProjectOopMirNode* element,
        const std::unordered_map<const frontend::ProjectOopMirNode*, const frontend::ProjectOopMirNode*>&
            parents) const
{
        if (element->kind == frontend::ProjectOopMirNodeKind::ArrayElementLoad ||
            element->kind == frontend::ProjectOopMirNodeKind::RangeArrayElementLoad)
            return OopLlvmArrayElementAccess::Read;
        const frontend::ProjectOopMirNode* child = element;
        bool throughField = false;
        auto parent = parents.find(child);
        while (parent != parents.end() && parent->second->kind == frontend::ProjectOopMirNodeKind::FieldAddress &&
               !parent->second->children.empty() && parent->second->children.front().get() == child)
        {
            child = parent->second;
            throughField = true;
            parent = parents.find(child);
        }
        if (parent == parents.end()) return OopLlvmArrayElementAccess::Read;
        const frontend::ProjectOopMirNode* user = parent->second;
        const bool firstChild = !user->children.empty() && user->children.front().get() == child;
        if (user->kind == frontend::ProjectOopMirNodeKind::Store && firstChild)
            return throughField ? OopLlvmArrayElementAccess::FieldStore : OopLlvmArrayElementAccess::WholeStore;
        if (throughField) return OopLlvmArrayElementAccess::Read;
        if (user->kind == frontend::ProjectOopMirNodeKind::MoveArrayElement)
            return OopLlvmArrayElementAccess::Move;
        if (user->kind == frontend::ProjectOopMirNodeKind::Deallocate && user->valueTypeNoOpDeletion &&
            user->booleanValue)
            return OopLlvmArrayElementAccess::Delete;
        return OopLlvmArrayElementAccess::Read;
}

// EN: Bytes of live bits for `count` elements: ceil(count / 8).
// ES: Bytes de bits de vida para `count` elementos: techo(count / 8).
LLVMValueRef OopLlvmImplementation::arrayLivenessByteCount(LLVMValueRef count)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef rounded = LLVMBuildAdd(builder_, count, LLVMConstInt(i64, 7, 0), "array.live.round");
        return LLVMBuildLShr(builder_, rounded, LLVMConstInt(i64, 3, 0), "array.live.bytes");
}

// EN: The live bit of element `linearIndex`. The bitmap starts right after the data:
//     dataOffset + length * elementSize, with the total element count at offset 0.
// ES: El bit de vida del elemento `linearIndex`. El mapa empieza justo despues de los datos:
//     dataOffset + length * elementSize, con el total de elementos en el offset 0.
OopLlvmArrayElementLiveness OopLlvmImplementation::arrayElementLiveness(
        LLVMValueRef array, LLVMValueRef linearIndex, std::uint64_t dataOffset, std::uint64_t elementSize)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef length = LLVMBuildLoad2(builder_, i64, array, "array.live.length");
        LLVMValueRef start = LLVMBuildAdd(
            builder_, LLVMBuildMul(builder_, length, LLVMConstInt(i64, elementSize, 0), "array.live.data"),
            LLVMConstInt(i64, dataOffset, 0), "array.live.start");
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, start, LLVMBuildLShr(builder_, linearIndex, LLVMConstInt(i64, 3, 0), "array.live.word"),
            "array.live.offset");
        LLVMValueRef indices[] = {offset};
        OopLlvmArrayElementLiveness liveness;
        liveness.byte = LLVMBuildGEP2(builder_, i8, array, indices, 1, "array.live.byte");
        LLVMValueRef bit = LLVMBuildTrunc(
            builder_, LLVMBuildAnd(builder_, linearIndex, LLVMConstInt(i64, 7, 0), "array.live.bit"), i8,
            "array.live.bit8");
        liveness.mask = LLVMBuildShl(builder_, LLVMConstInt(i8, 1, 0), bit, "array.live.mask");
        return liveness;
}

// EN: i1: whether the element of `liveness` is alive.
// ES: i1: si el elemento de `liveness` esta vivo.
LLVMValueRef OopLlvmImplementation::arrayElementIsAlive(const OopLlvmArrayElementLiveness& liveness)
{
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef bits = LLVMBuildLoad2(builder_, i8, liveness.byte, "array.live.load");
        LLVMValueRef selected = LLVMBuildAnd(builder_, bits, liveness.mask, "array.live.selected");
        return LLVMBuildICmp(builder_, LLVMIntNE, selected, LLVMConstInt(i8, 0, 0), "array.live.is");
}

// EN: Sets or clears the live bit of the element of `liveness`.
// ES: Enciende o apaga el bit de vida del elemento de `liveness`.
void OopLlvmImplementation::setArrayElementAlive(const OopLlvmArrayElementLiveness& liveness, bool alive)
{
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef bits = LLVMBuildLoad2(builder_, i8, liveness.byte, "array.live.old");
        LLVMValueRef updated = alive
            ? LLVMBuildOr(builder_, bits, liveness.mask, "array.live.set")
            : LLVMBuildAnd(builder_, bits, LLVMBuildNot(builder_, liveness.mask, "array.live.notmask"),
                "array.live.clear");
        LLVMBuildStore(builder_, updated, liveness.byte);
}

// EN: Liveness work at the element address, by access kind. A read or a move of a dead element
//     traps (DeadElementRead); a field store revives a dead element after zeroing it, so its
//     other fields hold their defaults; a whole store, a move and a delete leave the bit to their
//     consumer. `address` is the element; its linear index is recovered from it.
// ES: Trabajo de vida en la direccion del elemento, segun el acceso. Leer o mover un elemento
//     muerto atrapa (DeadElementRead); escribir un campo revive un elemento muerto tras ponerlo en
//     cero, asi sus otros campos quedan por defecto; un store completo, un move y un delete dejan
//     el bit a su consumidor. `address` es el elemento; su indice lineal se recupera de ella.
bool OopLlvmImplementation::applyArrayElementAccess(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef array,
        LLVMValueRef address,
        std::uint64_t dataOffset,
        std::uint64_t elementSize,
        std::string& error)
{
        const auto access = arrayElementAccess_.find(node);
        if (access == arrayElementAccess_.end() || elementSize == 0 || terminated()) return true;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef distance = LLVMBuildSub(
            builder_, LLVMBuildPtrToInt(builder_, address, i64, "array.live.element"),
            LLVMBuildPtrToInt(builder_, array, i64, "array.live.base"), "array.live.distance");
        LLVMValueRef linear = LLVMBuildUDiv(
            builder_, LLVMBuildSub(builder_, distance, LLVMConstInt(i64, dataOffset, 0), "array.live.data.distance"),
            LLVMConstInt(i64, elementSize, 0), "array.live.index");
        const OopLlvmArrayElementLiveness liveness = arrayElementLiveness(array, linear, dataOffset, elementSize);
        switch (access->second)
        {
            case OopLlvmArrayElementAccess::Read:
                return emitGuard(arrayElementIsAlive(liveness), LlvmTrapReason::DeadElementRead, "array.dead", error);
            case OopLlvmArrayElementAccess::Move:
                if (!emitGuard(arrayElementIsAlive(liveness), LlvmTrapReason::DeadElementRead, "array.dead", error))
                    return false;
                arrayElementLiveness_[node] = liveness;
                return true;
            case OopLlvmArrayElementAccess::FieldStore:
                return reviveArrayElement(liveness, address, elementSize);
            case OopLlvmArrayElementAccess::WholeStore:
            case OopLlvmArrayElementAccess::Delete:
                arrayElementLiveness_[node] = liveness;
                return true;
        }
        return true;
}

// EN: A dead element written through one of its fields is first zeroed (its other fields take
//     their defaults) and marked alive; a live one is left as it is.
// ES: Un elemento muerto escrito por uno de sus campos primero se pone en cero (sus otros campos
//     toman su valor por defecto) y se marca vivo; uno vivo se deja como esta.
bool OopLlvmImplementation::reviveArrayElement(
        const OopLlvmArrayElementLiveness& liveness, LLVMValueRef address, std::uint64_t elementSize)
{
        LLVMBasicBlockRef reviveBlock = appendBlock("array.revive");
        LLVMBasicBlockRef readyBlock = appendBlock("array.revive.ready");
        LLVMBuildCondBr(builder_, arrayElementIsAlive(liveness), readyBlock, reviveBlock);
        enterBlock(reviveBlock);
        LLVMValueRef arguments[] = {
            address,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), elementSize, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, arguments, 4, "");
        setArrayElementAlive(liveness, true);
        LLVMBuildBr(builder_, readyBlock);
        enterBlock(readyBlock);
        return true;
}

// EN: The liveness bit the element address `node` left for its consumer, removed from the table so
//     a later lowering of the same node never reuses it. Empty for an untracked element.
// ES: El bit de vida que la direccion de elemento `node` dejo a su consumidor, quitado de la tabla
//     para que otra bajada del mismo nodo nunca lo reuse. Vacio para un elemento sin bits.
std::optional<OopLlvmArrayElementLiveness> OopLlvmImplementation::takeArrayElementLiveness(
        const frontend::ProjectOopMirNode* node)
{
        const auto found = arrayElementLiveness_.find(node);
        if (found == arrayElementLiveness_.end()) return std::nullopt;
        const OopLlvmArrayElementLiveness liveness = found->second;
        arrayElementLiveness_.erase(found);
        return liveness;
}

// EN: `a[i] = v` over a tracked element: the old value is dropped only when it is alive.
// ES: `a[i] = v` sobre un elemento con bits de vida: el valor anterior se destruye solo si vive.
bool OopLlvmImplementation::dropReplacedArrayElement(
        LLVMValueRef address,
        std::string_view typeKey,
        const OopLlvmArrayElementLiveness& liveness,
        std::string& error)
{
        LLVMBasicBlockRef dropBlock = appendBlock("value.replace.alive");
        LLVMBasicBlockRef readyBlock = appendBlock("value.replace.ready");
        LLVMBuildCondBr(builder_, arrayElementIsAlive(liveness), dropBlock, readyBlock);
        enterBlock(dropBlock);
        if (!emitValueDropAddress(address, typeKey, "value.replace", error)) return false;
        if (!terminated()) LLVMBuildBr(builder_, readyBlock);
        enterBlock(readyBlock);
        return true;
}

}
