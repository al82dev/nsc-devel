#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMTypeRef OopLlvmImplementation::bitFieldStorageType(
        const frontend::ProjectOopMirNode* node,
        std::string& error) const
{
        if (node == nullptr || !node->bitOffset.has_value() || !node->bitWidth.has_value() ||
            node->bitStorageSize == 0 || node->bitStorageSize > 8)
        {
            error = "item 329 bit-field access has incomplete allocation-unit metadata";
            return nullptr;
        }
        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        if (*node->bitWidth == 0 || *node->bitOffset >= storageBits ||
            *node->bitWidth > storageBits - *node->bitOffset)
        {
            error = "item 329 bit-field coordinates exceed their allocation unit";
            return nullptr;
        }
        switch (storageBits)
        {
            case 8U: return LLVMInt8TypeInContext(context_);
            case 16U: return LLVMInt16TypeInContext(context_);
            case 32U: return LLVMInt32TypeInContext(context_);
            case 64U: return LLVMInt64TypeInContext(context_);
            default:
                error = "item 329 bit-field allocation unit has an unsupported integer width";
                return nullptr;
        }
    }

LLVMValueRef OopLlvmImplementation::extractBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
{
        LLVMTypeRef storageType = bitFieldStorageType(node, error);
        if (storageType == nullptr || storage == nullptr) return nullptr;

        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        const std::uint64_t bitOffset = *node->bitOffset;
        const std::uint64_t bitWidth = *node->bitWidth;
        const std::uint64_t lowMask = bitWidth == 64U
            ? ~std::uint64_t{0}
            : (std::uint64_t{1} << bitWidth) - 1U;
        LLVMValueRef extracted = storage;
        if (bitOffset != 0)
        {
            extracted = LLVMBuildLShr(
                builder_, extracted, LLVMConstInt(storageType, bitOffset, 0), "bitfield.shift");
        }
        if (bitWidth != storageBits)
        {
            extracted = LLVMBuildAnd(
                builder_, extracted, LLVMConstInt(storageType, lowMask, 0), "bitfield.mask");
        }
        if (node->typeKey == "builtin:bool")
        {
            return LLVMBuildICmp(
                builder_, LLVMIntNE, extracted, LLVMConstNull(storageType), "bitfield.bool");
        }

        const frontend::builtins::IntegerTypeInfo* enumInfo = findEnumUnderlyingInfo(nameLookup_, node->typeKey);
        const unsigned logicalWidth = enumInfo != nullptr
            ? enumInfo->width : integerWidth(node->typeKey);
        if (logicalWidth != storageBits)
        {
            return failValue(error, "item 329 bit-field logical type does not match its allocation unit");
        }
        const bool signedField = enumInfo != nullptr ? enumInfo->isSigned : isSignedInteger(node->typeKey);
        if (signedField && bitWidth < storageBits)
        {
            const std::uint64_t extensionShift = storageBits - bitWidth;
            LLVMValueRef shift = LLVMConstInt(storageType, extensionShift, 0);
            extracted = LLVMBuildShl(builder_, extracted, shift, "bitfield.sign.left");
            extracted = LLVMBuildAShr(builder_, extracted, shift, "bitfield.sign.extend");
        }
        return extracted;
    }

LLVMValueRef OopLlvmImplementation::insertBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef previousStorage,
        LLVMValueRef value,
        std::string& error)
{
        LLVMTypeRef storageType = bitFieldStorageType(node, error);
        if (storageType == nullptr || previousStorage == nullptr || value == nullptr) return nullptr;

        const std::uint64_t storageBits = node->bitStorageSize * 8U;
        const std::uint64_t bitOffset = *node->bitOffset;
        const std::uint64_t bitWidth = *node->bitWidth;
        const std::uint64_t lowMask = bitWidth == 64U
            ? ~std::uint64_t{0}
            : (std::uint64_t{1} << bitWidth) - 1U;
        const std::uint64_t fieldMask = lowMask << bitOffset;

        LLVMValueRef widened = value;
        if (node->typeKey == "builtin:bool")
        {
            widened = LLVMBuildZExt(builder_, value, storageType, "bitfield.bool.widen");
        }
        else
        {
            const frontend::builtins::IntegerTypeInfo* enumInfo = findEnumUnderlyingInfo(nameLookup_, node->typeKey);
            const unsigned logicalWidth = enumInfo != nullptr
                ? enumInfo->width : integerWidth(node->typeKey);
            if (logicalWidth != storageBits)
            {
                return failValue(error, "item 329 bit-field store type does not match its allocation unit");
            }
        }
        LLVMValueRef bits = LLVMBuildAnd(
            builder_, widened, LLVMConstInt(storageType, lowMask, 0), "bitfield.value.mask");
        if (bitOffset != 0)
        {
            bits = LLVMBuildShl(
                builder_, bits, LLVMConstInt(storageType, bitOffset, 0), "bitfield.value.shift");
        }
        LLVMValueRef preserved = LLVMBuildAnd(
            builder_, previousStorage, LLVMConstInt(storageType, ~fieldMask, 0),
            "bitfield.storage.preserve");
        return LLVMBuildOr(builder_, preserved, bits, "bitfield.storage.merge");
    }

LLVMValueRef OopLlvmImplementation::fieldAddressFromReceiver(
        const frontend::ProjectOopMirNode* node, LLVMValueRef receiver, std::string& error)
{
        const bool structReceiver = node->children.front() != nullptr &&
            isStructValue(node->children.front()->typeKey);
        if (!structReceiver)
        {
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr) return nullptr;
        }
        LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1, "field.addr");
    }

LLVMValueRef OopLlvmImplementation::checkedReceiver(LLVMValueRef receiver, std::string& error)
{
        if (receiver == nullptr || trapFunction_ == nullptr || trapFunctionType_ == nullptr ||
            currentFunction_ == nullptr)
        {
            return failValue(error, "Macro A cannot emit null-reference receiver guard");
        }

        LLVMValueRef nullValue = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        LLVMValueRef valid = LLVMBuildICmp(builder_, LLVMIntNE, receiver, nullValue, "receiver.nonnull");
        LLVMBasicBlockRef okBlock = appendBlock("receiver.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("receiver.null");
        LLVMBuildCondBr(builder_, valid, okBlock, trapBlock);

        enterBlock(trapBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::NullReference),
            0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        enterBlock(okBlock);
        return receiver;
    }

LLVMValueRef OopLlvmImplementation::convert(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        std::string& error)
{
        if (source == target || source == "builtin:null") return value;
        LLVMTypeRef destination = llvmType(target, error);
        if (destination == nullptr) return nullptr;
        if (const std::optional<language::AnonymousLambdaType> lambdaType =
                language::parseAnonymousLambdaTypeIdentity(source))
        {
            const std::optional<language::FunctionType> targetFunction =
                language::parseFunctionTypeIdentity(target);
            if (!targetFunction.has_value() ||
                targetFunction->carrier != language::FunctionCarrier::Pointer ||
                !language::functionTypesAssignable(lambdaType->signature, *targetFunction))
                return failValue(error, "invalid T08-F02 anonymous lambda function-pointer conversion");
            return callbackGuardThunk(
                language::anonymousLambdaGeneratedName(*lambdaType), error,
                targetFunction->callingConvention == language::CallingConvention::VectorCall);
        }
        const std::optional<std::vector<std::string>> sourceTuple =
            frontend::ProjectTypeConversion::tupleElementTypeKeys(source);
        const std::optional<std::vector<std::string>> targetTuple =
            frontend::ProjectTypeConversion::tupleElementTypeKeys(target);
        if (sourceTuple.has_value() || targetTuple.has_value())
            return convertStructuralTuple(value, source, target, destination, error);
        // NS304-T11-F01: Span<T> -> ReadOnlySpan<T> is a structural two-field view. The pointer
        // field converts by opaque-pointer identity (const is erased in LLVM), the length field is
        // identity; both are copied into the target struct exactly like the tuple conversion above.
        bool spanViewHandled = false;
        LLVMValueRef spanView = tryConvertSpanView(
            value, source, target, destination, spanViewHandled, error);
        if (spanViewHandled) return spanView;
        return convertScalar(value, source, target, destination, error);
}

LLVMValueRef OopLlvmImplementation::convertStructuralTuple(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        std::string& error)
{
        const std::optional<std::vector<std::string>> sourceTuple =
            frontend::ProjectTypeConversion::tupleElementTypeKeys(source);
        const std::optional<std::vector<std::string>> targetTuple =
            frontend::ProjectTypeConversion::tupleElementTypeKeys(target);
        const frontend::ProjectOopMirStructLayout* sourceLayout = findStructLayout(mirModel_, source);
        const frontend::ProjectOopMirStructLayout* targetLayout = findStructLayout(mirModel_, target);
        if (!sourceTuple.has_value() || !targetTuple.has_value() ||
            sourceTuple->size() != targetTuple->size() || sourceLayout == nullptr ||
            targetLayout == nullptr || sourceLayout->fields.size() != sourceTuple->size() ||
            targetLayout->fields.size() != targetTuple->size())
            return failValue(error, "malformed structural tuple conversion");
        std::string ignored;
        LLVMTypeRef sourceCarrier = llvmType(source, ignored);
        if (sourceCarrier == nullptr) return failValue(error, "tuple source has no LLVM carrier");
        LLVMValueRef sourceSlot = entryBlockAlloca(
            sourceCarrier, static_cast<unsigned>(sourceLayout->alignment), "tuple.convert.source");
        LLVMValueRef sourceStore = LLVMBuildStore(builder_, value, sourceSlot);
        if (sourceLayout->alignment > 1U)
            LLVMSetAlignment(sourceStore, static_cast<unsigned>(sourceLayout->alignment));
        LLVMValueRef targetSlot = entryBlockAlloca(
            destination, static_cast<unsigned>(targetLayout->alignment), "tuple.convert.target");
        LLVMValueRef targetZero = LLVMBuildStore(builder_, LLVMConstNull(destination), targetSlot);
        if (targetLayout->alignment > 1U)
            LLVMSetAlignment(targetZero, static_cast<unsigned>(targetLayout->alignment));
        for (std::size_t index = 0; index < sourceTuple->size(); ++index)
        {
            LLVMTypeRef sourceType = llvmType((*sourceTuple)[index], error);
            if (sourceType == nullptr) return nullptr;
            LLVMValueRef sourceAddress = byteOffset(
                sourceSlot, sourceLayout->fields[index].offset, "tuple.convert.source.element");
            LLVMValueRef element = LLVMBuildLoad2(
                builder_, sourceType, sourceAddress, "tuple.convert.element");
            if (sourceLayout->fields[index].alignment > 1U)
                LLVMSetAlignment(element, static_cast<unsigned>(sourceLayout->fields[index].alignment));
            element = convert(element, (*sourceTuple)[index], (*targetTuple)[index], error);
            if (element == nullptr) return nullptr;
            LLVMValueRef targetAddress = byteOffset(
                targetSlot, targetLayout->fields[index].offset, "tuple.convert.target.element");
            LLVMValueRef store = LLVMBuildStore(builder_, element, targetAddress);
            if (targetLayout->fields[index].alignment > 1U)
                LLVMSetAlignment(store, static_cast<unsigned>(targetLayout->fields[index].alignment));
        }
        LLVMValueRef converted = LLVMBuildLoad2(
            builder_, destination, targetSlot, "tuple.convert.value");
        if (targetLayout->alignment > 1U)
            LLVMSetAlignment(converted, static_cast<unsigned>(targetLayout->alignment));
        return converted;
}

std::string_view OopLlvmImplementation::terminalTypeKey(std::string_view key) const
{
        const std::size_t separator = key.find_last_of(".:+");
        return separator == std::string_view::npos ? key : key.substr(separator + 1U);
}

LLVMValueRef OopLlvmImplementation::tryConvertSpanView(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        bool& handled,
        std::string& error)
{
        handled = false;
        constexpr std::string_view spanPrefix = "Span__g1__";
        constexpr std::string_view readOnlySpanPrefix = "ReadOnlySpan__g1__";
        if (!terminalTypeKey(source).starts_with(spanPrefix) ||
            !terminalTypeKey(target).starts_with(readOnlySpanPrefix)) return nullptr;
        const frontend::ProjectOopMirStructLayout* sourceLayout = findStructLayout(mirModel_, source);
        const frontend::ProjectOopMirStructLayout* targetLayout = findStructLayout(mirModel_, target);
        // EN: Freeze 3.0 P4: Span and ReadOnlySpan have the same fields (four since P4); all are
        //     carried, so the converted view keeps its array and first index.
        // ES: Congelamiento 3.0 P4: Span y ReadOnlySpan tienen los mismos campos (cuatro desde P4); se
        //     copian todos, asi la vista convertida conserva su arreglo y su primer indice.
        if (sourceLayout == nullptr || targetLayout == nullptr || sourceLayout->fields.size() < 2U ||
            sourceLayout->fields.size() != targetLayout->fields.size()) return nullptr;
        handled = true;
        std::string ignored;
        LLVMTypeRef sourceCarrier = llvmType(source, ignored);
        if (sourceCarrier == nullptr) return failValue(error, "span view source has no LLVM carrier");
        LLVMValueRef sourceSlot = entryBlockAlloca(
            sourceCarrier, static_cast<unsigned>(sourceLayout->alignment), "span.view.source");
        LLVMValueRef sourceStore = LLVMBuildStore(builder_, value, sourceSlot);
        if (sourceLayout->alignment > 1U)
            LLVMSetAlignment(sourceStore, static_cast<unsigned>(sourceLayout->alignment));
        LLVMValueRef targetSlot = entryBlockAlloca(
            destination, static_cast<unsigned>(targetLayout->alignment), "span.view.target");
        LLVMValueRef targetZero = LLVMBuildStore(builder_, LLVMConstNull(destination), targetSlot);
        if (targetLayout->alignment > 1U)
            LLVMSetAlignment(targetZero, static_cast<unsigned>(targetLayout->alignment));
        for (std::size_t index = 0; index < sourceLayout->fields.size(); ++index)
        {
            LLVMTypeRef sourceType = llvmType(sourceLayout->fields[index].typeKey, error);
            if (sourceType == nullptr) return nullptr;
            LLVMValueRef sourceAddress = byteOffset(
                sourceSlot, sourceLayout->fields[index].offset, "span.view.source.element");
            LLVMValueRef element = LLVMBuildLoad2(
                builder_, sourceType, sourceAddress, "span.view.element");
            if (sourceLayout->fields[index].alignment > 1U)
                LLVMSetAlignment(element, static_cast<unsigned>(sourceLayout->fields[index].alignment));
            element = convert(element, sourceLayout->fields[index].typeKey,
                targetLayout->fields[index].typeKey, error);
            if (element == nullptr) return nullptr;
            LLVMValueRef targetAddress = byteOffset(
                targetSlot, targetLayout->fields[index].offset, "span.view.target.element");
            LLVMValueRef store = LLVMBuildStore(builder_, element, targetAddress);
            if (targetLayout->fields[index].alignment > 1U)
                LLVMSetAlignment(store, static_cast<unsigned>(targetLayout->fields[index].alignment));
        }
        LLVMValueRef converted = LLVMBuildLoad2(builder_, destination, targetSlot, "span.view.value");
        if (targetLayout->alignment > 1U)
            LLVMSetAlignment(converted, static_cast<unsigned>(targetLayout->alignment));
        return converted;
}

LLVMValueRef OopLlvmImplementation::convertDecimalToInteger(
        LLVMValueRef value, LLVMTypeRef destination)
{
        // NS304-T15-F06: decimal -> integer = coefficient (low 96 bits) / 10^scale, truncated toward
        // zero. 10^scale is built with a runtime loop because the scale is dynamic (0..28). The
        // division is widened through a 64-bit lane (the 128-bit divide would need __udivti3).
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef coefficientMask = LLVMConstInt(wide, 0xFFFFFFFFFFFFFFFFull, 0);
        LLVMValueRef coefficient = LLVMBuildAnd(builder_, value, coefficientMask, "decimal.coeff");
        LLVMValueRef scaleShift = LLVMConstInt(wide, 96, 0);
        LLVMValueRef scaleRaw = LLVMBuildLShr(builder_, value, scaleShift, "decimal.scale.raw");
        // The scale occupies bits 96..101; the sign sits at bit 127. Mask the scale so a negative
        // decimal's sign bit never leaks into the pow10 loop as a bogus exponent.
        LLVMValueRef scale = LLVMBuildAnd(
            builder_, scaleRaw, LLVMConstInt(wide, 0x3F, 0), "decimal.scale");
        LLVMValueRef signBit = LLVMBuildLShr(
            builder_, value, LLVMConstInt(wide, 127, 0), "decimal.sign.bit");
        LLVMValueRef isNegative = LLVMBuildTrunc(
            builder_, signBit, LLVMInt1TypeInContext(context_), "decimal.negative");
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMValueRef coefficientLane = LLVMBuildTrunc(builder_, coefficient, lane, "decimal.coeff64");
        LLVMValueRef scaleLane = LLVMBuildTrunc(builder_, scale, lane, "decimal.scale64");
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef header = LLVMAppendBasicBlockInContext(
            context_, currentFunction_, "decimal.pow10.header");
        LLVMBasicBlockRef body = LLVMAppendBasicBlockInContext(
            context_, currentFunction_, "decimal.pow10.body");
        LLVMBasicBlockRef exit = LLVMAppendBasicBlockInContext(
            context_, currentFunction_, "decimal.pow10.exit");
        LLVMValueRef one = LLVMConstInt(lane, 1, 0);
        LLVMValueRef ten = LLVMConstInt(lane, 10, 0);
        LLVMBuildBr(builder_, header);
        enterBlock(header);
        LLVMValueRef divisor = LLVMBuildPhi(builder_, lane, "decimal.pow10.div");
        LLVMValueRef remaining = LLVMBuildPhi(builder_, lane, "decimal.pow10.scale");
        LLVMValueRef done = LLVMBuildICmp(
            builder_, LLVMIntEQ, remaining, LLVMConstInt(lane, 0, 0), "decimal.pow10.done");
        LLVMBuildCondBr(builder_, done, exit, body);
        enterBlock(body);
        LLVMValueRef nextDivisor = LLVMBuildMul(builder_, divisor, ten, "decimal.pow10.mul");
        LLVMValueRef nextRemaining = LLVMBuildSub(builder_, remaining, one, "decimal.pow10.dec");
        LLVMBuildBr(builder_, header);
        LLVMAddIncoming(divisor, &one, &preheader, 1);
        LLVMAddIncoming(divisor, &nextDivisor, &body, 1);
        LLVMAddIncoming(remaining, &scaleLane, &preheader, 1);
        LLVMAddIncoming(remaining, &nextRemaining, &body, 1);
        enterBlock(exit);
        LLVMValueRef quotient = LLVMBuildUDiv(builder_, coefficientLane, divisor, "decimal.quotient");
        LLVMValueRef truncated = LLVMBuildTrunc(builder_, quotient, destination, "decimal.trunc");
        LLVMValueRef negated = LLVMBuildSub(
            builder_, LLVMConstNull(destination), truncated, "decimal.neg");
        return LLVMBuildSelect(builder_, isNegative, negated, truncated, "decimal.toint");
}

LLVMValueRef OopLlvmImplementation::convertIntegerToDecimal(
        LLVMValueRef value, const bool sourceSigned)
{
        // NS304-T15-F06: integer -> decimal = |value| coefficient, scale 0, sign bit 127 for
        // negatives. Implicit and exact (no rounding, no overflow for the closed integral set).
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef extended = sourceSigned
            ? LLVMBuildSExt(builder_, value, wide, "decimal.from.sext")
            : LLVMBuildZExt(builder_, value, wide, "decimal.from.zext");
        LLVMValueRef isNegative = LLVMBuildICmp(
            builder_, LLVMIntSLT, extended, LLVMConstNull(wide), "decimal.from.neg");
        LLVMValueRef negated = LLVMBuildNeg(builder_, extended, "decimal.from.negated");
        LLVMValueRef coefficient = LLVMBuildSelect(
            builder_, isNegative, negated, extended, "decimal.from.coeff");
        LLVMValueRef sign = LLVMBuildShl(
            builder_, LLVMBuildZExt(builder_, isNegative, wide, "decimal.from.signext"),
            LLVMConstInt(wide, 127, 0), "decimal.from.sign");
        return LLVMBuildOr(builder_, coefficient, sign, "decimal.from.value");
}

LLVMValueRef OopLlvmImplementation::convertDecimalToFloating(
        LLVMValueRef value, LLVMTypeRef destination)
{
        // NS304-T15-F06: decimal -> float/double = |coefficient| / 10^scale, sign applied. Precision
        // loss is accepted (the destination FP format decides it).
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef coefficient = LLVMBuildAnd(
            builder_, value, LLVMConstInt(wide, 0xFFFFFFFFFFFFFFFFull, 0), "decimal.coeff");
        LLVMValueRef scaleRaw = LLVMBuildLShr(
            builder_, value, LLVMConstInt(wide, 96, 0), "decimal.scale.raw");
        LLVMValueRef scale = LLVMBuildAnd(
            builder_, scaleRaw, LLVMConstInt(wide, 0x3F, 0), "decimal.scale");
        LLVMValueRef signBit = LLVMBuildLShr(
            builder_, value, LLVMConstInt(wide, 127, 0), "decimal.sign.bit");
        LLVMValueRef isNegative = LLVMBuildTrunc(
            builder_, signBit, LLVMInt1TypeInContext(context_), "decimal.negative");
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMValueRef coeff64 = LLVMBuildTrunc(builder_, coefficient, lane, "decimal.coeff64");
        LLVMValueRef scale64 = LLVMBuildTrunc(builder_, scale, lane, "decimal.scale64");
        LLVMValueRef divisorInt = buildPow10(scale64, "decimal.tofp.pow10");
        LLVMValueRef coeffFp = LLVMBuildUIToFP(builder_, coeff64, destination, "decimal.coeff.fp");
        LLVMValueRef divisorFp = LLVMBuildUIToFP(builder_, divisorInt, destination, "decimal.pow10.fp");
        LLVMValueRef result = LLVMBuildFDiv(builder_, coeffFp, divisorFp, "decimal.tofp");
        LLVMValueRef negated = LLVMBuildFNeg(builder_, result, "decimal.tofp.neg");
        return LLVMBuildSelect(builder_, isNegative, negated, result, "decimal.tofp.signed");
}

LLVMValueRef OopLlvmImplementation::convertFloatingToDecimal(
        LLVMValueRef value, std::string_view source, std::string& error)
{
        // NS304-T15-F06: float/double -> decimal. Finite values are rounded to 6 fractional digits
        // (coefficient = round(|v| * 10^6), scale 6, sign bit).
        LLVMTypeRef sourceFp = llvmType(source, error);
        if (sourceFp == nullptr) return nullptr;
        LLVMValueRef isNegative = LLVMBuildFCmp(
            builder_, LLVMRealOLT, value, LLVMConstReal(sourceFp, 0.0), "decimal.from.neg");
        LLVMValueRef negValue = LLVMBuildFNeg(builder_, value, "decimal.from.negval");
        LLVMValueRef abs = LLVMBuildSelect(builder_, isNegative, negValue, value, "decimal.from.abs");
        // NaN and infinities are rejected before any decimal bits are produced.
        LLVMValueRef isNan = LLVMBuildFCmp(builder_, LLVMRealUNO, value, value, "decimal.from.nan");
        LLVMValueRef inf = LLVMConstReal(sourceFp, std::numeric_limits<double>::infinity());
        LLVMValueRef isInf = LLVMBuildFCmp(builder_, LLVMRealOEQ, abs, inf, "decimal.from.inf");
        LLVMValueRef invalid = LLVMBuildOr(builder_, isNan, isInf, "decimal.from.invalid");
        LLVMBasicBlockRef okBlock = appendBlock("decimal.from.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("decimal.from.trap");
        LLVMBuildCondBr(builder_, invalid, trapBlock, okBlock);
        enterBlock(trapBlock);
        if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
            return failValue(error, "decimal float conversion rejects non-finite input but no trap is configured");
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::DecimalOverflow), 0);
        LLVMValueRef trapArgs[] = { reason };
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArgs, 1, "");
        LLVMBuildUnreachable(builder_);
        enterBlock(okBlock);
        LLVMValueRef scaled = LLVMBuildFMul(
            builder_, abs, LLVMConstReal(sourceFp, 1e6), "decimal.from.scaled");
        LLVMValueRef rounded = LLVMBuildFPToSI(
            builder_, scaled, LLVMInt64TypeInContext(context_), "decimal.from.coeff64");
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef coeffWide = LLVMBuildZExt(builder_, rounded, wide, "decimal.from.coeff");
        LLVMValueRef scaleShifted = LLVMBuildShl(
            builder_, LLVMConstInt(wide, 6, 0),
            LLVMConstInt(wide, 96, 0), "decimal.from.scale");
        LLVMValueRef combined = LLVMBuildOr(builder_, coeffWide, scaleShifted, "decimal.from.value");
        LLVMValueRef sign = LLVMBuildShl(
            builder_, LLVMBuildZExt(builder_, isNegative, wide, "decimal.from.signext"),
            LLVMConstInt(wide, 127, 0), "decimal.from.sign");
        return LLVMBuildOr(builder_, combined, sign, "decimal.from.signed");
}

LLVMValueRef OopLlvmImplementation::convertScalar(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        std::string& error)
{
        // UN ENUM SE CONVIERTE COMO SU TIPO SUBYACENTE. `integerWidth()` e `isSignedInteger()` solo
        // saben de claves "builtin:", asi que la clave de un enum tiene que traer su ancho y su
        // signo de la declaracion. Antes esto decia "32 bits y con signo" sin preguntar, y con un
        // `enum : byte` emitia `icmp ne i8 %v, i32 200` -- el verificador de LLVM lo rechazaba,
        // que es la forma AFORTUNADA del defecto; en `enum : sbyte` habria sido peor, porque
        // extender sin signo un -1 da 255 y eso compila y responde mal.
        const frontend::builtins::IntegerTypeInfo* sourceEnum = findEnumUnderlyingInfo(nameLookup_, source);
        const frontend::builtins::IntegerTypeInfo* targetEnum = findEnumUnderlyingInfo(nameLookup_, target);
        const unsigned sourceWidth = sourceEnum != nullptr ? sourceEnum->width : integerWidth(source);
        const unsigned targetWidth = targetEnum != nullptr ? targetEnum->width : integerWidth(target);
        const bool sourceSigned =
            sourceEnum != nullptr ? sourceEnum->isSigned : isSignedInteger(source);
        const bool targetSigned =
            targetEnum != nullptr ? targetEnum->isSigned : isSignedInteger(target);
        if (sourceWidth != 0 && targetWidth != 0)
        {
            if (sourceWidth == targetWidth) return value;
            if (sourceWidth > targetWidth) return LLVMBuildTrunc(builder_, value, destination, "trunc");
            return sourceSigned
                ? LLVMBuildSExt(builder_, value, destination, "sext")
                : LLVMBuildZExt(builder_, value, destination, "zext");
        }
        if (sourceWidth != 0 && isFloating(target))
            return sourceSigned
                ? LLVMBuildSIToFP(builder_, value, destination, "sitofp")
                : LLVMBuildUIToFP(builder_, value, destination, "uitofp");
        if (isFloating(source) && targetWidth != 0)
            return targetSigned
                ? LLVMBuildFPToSI(builder_, value, destination, "fptosi")
                : LLVMBuildFPToUI(builder_, value, destination, "fptoui");
        if (source == "builtin:float" && target == "builtin:double")
            return LLVMBuildFPExt(builder_, value, destination, "fpext");
        if (source == "builtin:double" && target == "builtin:float")
            return LLVMBuildFPTrunc(builder_, value, destination, "fptrunc");
        if ((isPointerLikeType(source) || isClassReferenceType(nameLookup_, source)) &&
            (isPointerLikeType(target) || isClassReferenceType(nameLookup_, target))) return value;

        // C1.1 "function-pointer round trip". Everything above is the numeric/reference world;
        // these three lines are the address world. An extern C delegate, a raw `T*`/`void*` and a
        // pointer-sized integral are the same 64 bits in three spellings, so the round trip is
        // exactly one LLVM instruction in each direction -- and NONE between two pointer-shaped
        // ends, since opaque pointers make that a no-op. The frontend has already refused every
        // combination that is not part of the trip (see ProjectMethodBodyModel's CastExpression
        // case), so reaching here means the conversion is one of the sanctioned three.
        const bool sourceIsAddress =
            isPointerLikeType(source) || isClassReferenceType(nameLookup_, source) || isExternCDelegateType(source);
        const bool targetIsAddress =
            isPointerLikeType(target) || isClassReferenceType(nameLookup_, target) || isExternCDelegateType(target);
        if (sourceIsAddress && targetIsAddress) return value;
        if (sourceIsAddress && targetWidth != 0)
            return LLVMBuildPtrToInt(builder_, value, destination, "ptrtoint");
        if (sourceWidth != 0 && targetIsAddress)
            return LLVMBuildIntToPtr(builder_, value, destination, "inttoptr");
        if (source == "builtin:decimal" && targetWidth != 0)
            return convertDecimalToInteger(value, destination);
        if (sourceWidth != 0 && target == "builtin:decimal")
            return convertIntegerToDecimal(value, sourceSigned);
        if (source == "builtin:decimal" && (target == "builtin:float" || target == "builtin:double"))
            return convertDecimalToFloating(value, destination);
        if ((source == "builtin:float" || source == "builtin:double") && target == "builtin:decimal")
            return convertFloatingToDecimal(value, source, error);
        return failValue(
            error, "unsupported Stage 34 LLVM conversion from '" + std::string(source) +
                "' to '" + std::string(target) + "'");
}

}
