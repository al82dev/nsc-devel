#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "../SignedDivisionCorrection.h"
#include "OopLlvmSupport.h"

#include "nativesharp/Language/AbiSemantics.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::emitRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        std::string& error)
{
        if (rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr ||
            rttiInterfaceTestFunction_ == nullptr)
        {
            return failValue(error, "Stage 39 RTTI helper functions are unavailable");
        }
        LLVMValueRef targetId = LLVMConstInt(
            LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef arguments[] = {object, targetId};
        LLVMValueRef function = node->targetIsInterface
            ? rttiInterfaceTestFunction_
            : rttiClassTestFunction_;
        return buildNoUnwindCall(
            rttiTestFunctionType_, function, arguments, 2,
            node->targetIsInterface ? "is.interface" : "is.class");
    }

LLVMValueRef OopLlvmImplementation::lowerRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 39 runtime-type-test MIR requires one operand");
        }
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        return object != nullptr ? emitRuntimeTypeTest(node, object, error) : nullptr;
    }

LLVMValueRef OopLlvmImplementation::lowerSafeReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 39 safe-reference-cast MIR requires one operand");
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;
        LLVMValueRef compatible = emitRuntimeTypeTest(node, object, error);
        if (compatible == nullptr) return nullptr;

        LLVMBasicBlockRef successBlock = appendBlock("cast.safe.success");
        LLVMBasicBlockRef failureBlock = appendBlock("cast.safe.failure");
        LLVMBasicBlockRef doneBlock = appendBlock("cast.safe.done");
        LLVMBuildCondBr(builder_, compatible, successBlock, failureBlock);

        enterBlock(successBlock);
        LLVMBuildBr(builder_, doneBlock);
        enterBlock(failureBlock);
        LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, ptr, "cast.safe.result");
        LLVMValueRef incomingValues[] = {object, LLVMConstNull(ptr)};
        LLVMBasicBlockRef incomingBlocks[] = {successBlock, failureBlock};
        LLVMAddIncoming(result, incomingValues, incomingBlocks, 2);
        return result;
    }

LLVMValueRef OopLlvmImplementation::lowerCheckedReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1 || trapFunction_ == nullptr || trapFunctionType_ == nullptr)
        {
            return failValue(error, "Stage 39 checked-reference-cast MIR is incomplete");
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;

        LLVMBasicBlockRef testBlock = appendBlock("cast.checked.test");
        LLVMBasicBlockRef okBlock = appendBlock("cast.checked.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("cast.checked.invalid");
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "cast.checked.null");
        LLVMBuildCondBr(builder_, isNull, okBlock, testBlock);

        enterBlock(testBlock);
        LLVMValueRef compatible = emitRuntimeTypeTest(node, object, error);
        if (compatible == nullptr) return nullptr;
        LLVMBuildCondBr(builder_, compatible, okBlock, trapBlock);

        enterBlock(trapBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InvalidCast),
            0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        enterBlock(okBlock);
        return object;
    }

LLVMValueRef OopLlvmImplementation::normalizeShiftCount(
        LLVMValueRef count,
        const std::string& countTypeKey,
        const std::string& valueTypeKey,
        std::string& error)
{
        const unsigned valueWidth = integerWidth(valueTypeKey);
        LLVMTypeRef countType = llvmType(countTypeKey, error);
        if (valueWidth == 0 || countType == nullptr)
        {
            return failValue(error, "shift operands must have concrete integer widths");
        }

        LLVMValueRef belowWidth = LLVMBuildICmp(
            builder_,
            LLVMIntULT,
            count,
            LLVMConstInt(countType, valueWidth, 0),
            "shift.below_width");
        LLVMValueRef validCount = belowWidth;
        if (isSignedInteger(countTypeKey))
        {
            LLVMValueRef nonnegative = LLVMBuildICmp(
                builder_,
                LLVMIntSGE,
                count,
                LLVMConstInt(countType, 0, 0),
                "shift.nonnegative");
            validCount = LLVMBuildAnd(builder_, nonnegative, belowWidth, "shift.valid");
        }
        if (!emitGuard(validCount, LlvmTrapReason::InvalidShiftCount, "shift.range", error))
        {
            return nullptr;
        }
        return convert(count, countTypeKey, valueTypeKey, error);
    }

LLVMValueRef OopLlvmImplementation::applyBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero)
{
        // NSC-LIMIT-004. DEFENSA, no arreglo. Un agregado no puede llegar aqui: `icmp eq [24 x i8]`
        // no es una instruccion legal y el verificador rechaza el modulo entero, con un mensaje
        // que habla de vectores y no menciona structs por ningun lado. La correccion esta en el
        // frontend --`==` sobre structs baja a AggregateEquals-- y esto es la red por si alguna
        // vez vuelve a colarse: dice el nombre del tipo y de donde viene el problema.
        if (isEnumTypeKey(nameLookup_, operandType) == false && operandType.starts_with("type:") &&
            findStructLayout(mirModel_, operandType) != nullptr)
        {
            return failValue(
                error,
                "aggregate comparison reached scalar binary lowering for '" + operandType +
                    "': struct equality must lower through AggregateEquals");
        }
        const auto portableSimd = language::parsePortableSimdType(operandType);
        const bool floating = portableSimd.has_value()
            ? portableSimd->laneKind == language::SimdLaneKind::FloatingPoint
            : isFloating(operandType);
        if (isComparison(operation))
            return applyComparisonOperation(
                operation, left, right, operandType, portableSimd, floating);
        if (floating) return applyFloatingBinaryOperation(operation, left, right, error);
        if (operandType == "builtin:decimal")
            return applyDecimalBinaryOperation(operation, left, right, error);
        return applyIntegerBinaryOperation(
            operation, left, right, operandType, error, divisorProvenNonZero);
}

LLVMValueRef OopLlvmImplementation::applyComparisonOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        const std::optional<language::PortableSimdType>& portableSimd,
        const bool floating)
{
        if (floating)
        {
            return applyFloatingPointMode(LLVMBuildFCmp(
                builder_, floatingPredicate(operation), left, right, "fcmp"));
        }
        const bool signedInteger = portableSimd.has_value()
            ? portableSimd->laneKind == language::SimdLaneKind::SignedInteger
            : (isSignedInteger(operandType) || operandType == "builtin:decimal");
        return LLVMBuildICmp(
            builder_, integerPredicate(operation, signedInteger), left, right, "icmp");
}

LLVMValueRef OopLlvmImplementation::tryApplyConstrainedFloatingOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const FloatingPointRoundingMode rounding)
{
        const char* intrinsicName = nullptr;
        switch (operation)
        {
            case lexer::TokenKind::Plus: intrinsicName = "llvm.experimental.constrained.fadd"; break;
            case lexer::TokenKind::Minus: intrinsicName = "llvm.experimental.constrained.fsub"; break;
            case lexer::TokenKind::Star: intrinsicName = "llvm.experimental.constrained.fmul"; break;
            case lexer::TokenKind::Slash: intrinsicName = "llvm.experimental.constrained.fdiv"; break;
            default: break;
        }
        if (intrinsicName == nullptr) return nullptr;
        const unsigned intrinsicId = LLVMLookupIntrinsicID(
            intrinsicName, std::char_traits<char>::length(intrinsicName));
        LLVMTypeRef overloadType = LLVMTypeOf(left);
        LLVMValueRef intrinsic = LLVMGetIntrinsicDeclaration(module_, intrinsicId, &overloadType, 1U);
        LLVMTypeRef intrinsicType = LLVMGlobalGetValueType(intrinsic);
        const std::string_view roundingText = floatingPointRoundingMetadata(rounding);
        LLVMMetadataRef roundingMetadata = LLVMMDStringInContext2(
            context_, roundingText.data(), roundingText.size());
        static constexpr std::string_view exceptionText = "fpexcept.strict";
        LLVMMetadataRef exceptionMetadata = LLVMMDStringInContext2(
            context_, exceptionText.data(), exceptionText.size());
        LLVMValueRef arguments[] = {
            left,
            right,
            LLVMMetadataAsValue(context_, roundingMetadata),
            LLVMMetadataAsValue(context_, exceptionMetadata)};
        return LLVMBuildCall2(builder_, intrinsicType, intrinsic, arguments, 4U, "fop.rounded");
}

LLVMValueRef OopLlvmImplementation::applyFloatingBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error)
{
        const FloatingPointRoundingMode rounding = activeFloatingPointRoundingMode();
        if (rounding != FloatingPointRoundingMode::Dynamic &&
            operation != lexer::TokenKind::Percent)
        {
            LLVMValueRef constrained = tryApplyConstrainedFloatingOperation(
                operation, left, right, rounding);
            if (constrained != nullptr) return constrained;
        }
        LLVMValueRef result = nullptr;
        switch (operation)
        {
            case lexer::TokenKind::Plus: result = LLVMBuildFAdd(builder_, left, right, "fadd"); break;
            case lexer::TokenKind::Minus: result = LLVMBuildFSub(builder_, left, right, "fsub"); break;
            case lexer::TokenKind::Star: result = LLVMBuildFMul(builder_, left, right, "fmul"); break;
            case lexer::TokenKind::Slash: result = LLVMBuildFDiv(builder_, left, right, "fdiv"); break;
            case lexer::TokenKind::Percent: result = LLVMBuildFRem(builder_, left, right, "frem"); break;
            default: return failValue(error, "unsupported floating binary operation in Stage 34");
        }
        const bool contractable = operation == lexer::TokenKind::Plus ||
            operation == lexer::TokenKind::Minus || operation == lexer::TokenKind::Star;
        return applyFloatingPointMode(result, contractable);
}

LLVMValueRef OopLlvmImplementation::applyDecimalAddOrSubtract(
        lexer::TokenKind operation,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef right,
        LLVMValueRef scaleShift,
        LLVMValueRef scaleMask,
        LLVMValueRef& effectiveScale)
{
        // NS304-T15-F04 mixed scales: align both coefficients to the max scale by multiplying
        // each by 10^(max - its scale), then add/sub the aligned coefficients.
        LLVMValueRef rightScale = LLVMBuildAnd(
            builder_, LLVMBuildLShr(builder_, right, scaleShift, "decimal.right.scale.raw"),
            scaleMask, "decimal.right.scale");
        LLVMValueRef scaleGe = LLVMBuildICmp(
            builder_, LLVMIntUGE, leftScale, rightScale, "decimal.scale.ge");
        LLVMValueRef maxScale = LLVMBuildSelect(
            builder_, scaleGe, leftScale, rightScale, "decimal.scale.max");
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMValueRef leftCoeff64 = LLVMBuildTrunc(
            builder_, leftCoefficient, lane, "decimal.align.left64");
        LLVMValueRef rightCoeff64 = LLVMBuildTrunc(
            builder_, rightCoefficient, lane, "decimal.align.right64");
        LLVMValueRef leftDiff = LLVMBuildTrunc(
            builder_, LLVMBuildSub(builder_, maxScale, leftScale, "decimal.align.leftdiff"),
            lane, "decimal.align.leftdiff64");
        LLVMValueRef rightDiff = LLVMBuildTrunc(
            builder_, LLVMBuildSub(builder_, maxScale, rightScale, "decimal.align.rightdiff"),
            lane, "decimal.align.rightdiff64");
        LLVMValueRef leftMult = buildPow10(leftDiff, "decimal.align.left.pow10");
        LLVMValueRef rightMult = buildPow10(rightDiff, "decimal.align.right.pow10");
        LLVMValueRef leftAligned = LLVMBuildMul(
            builder_, leftCoeff64, leftMult, "decimal.align.left");
        LLVMValueRef rightAligned = LLVMBuildMul(
            builder_, rightCoeff64, rightMult, "decimal.align.right");
        LLVMValueRef aligned = operation == lexer::TokenKind::Plus
            ? LLVMBuildAdd(builder_, leftAligned, rightAligned, "decimal.coeff.add")
            : LLVMBuildSub(builder_, leftAligned, rightAligned, "decimal.coeff.sub");
        effectiveScale = maxScale;
        return LLVMBuildZExt(
            builder_, aligned, LLVMIntTypeInContext(context_, 128), "decimal.coeff.wide");
}

LLVMValueRef OopLlvmImplementation::buildSaturatedDecimalMultiply(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef mask96,
        LLVMTypeRef wide)
{
        LLVMValueRef leftSign = LLVMBuildLShr(
            builder_, left, LLVMConstInt(wide, 127, 0), "decimal.mul.leftsign");
        LLVMValueRef rightSign = LLVMBuildLShr(
            builder_, right, LLVMConstInt(wide, 127, 0), "decimal.mul.rightsign");
        LLVMValueRef sign = LLVMBuildAnd(
            builder_, LLVMBuildXor(builder_, leftSign, rightSign, "decimal.mul.signxor"),
            LLVMConstInt(wide, 1, 0), "decimal.mul.sign");
        return LLVMBuildOr(
            builder_, mask96,
            LLVMBuildShl(builder_, sign, LLVMConstInt(wide, 127, 0), "decimal.mul.signshift"),
            "decimal.mul.saturated");
}

void OopLlvmImplementation::mergeSaturatedDecimalMultiply(
        LLVMValueRef saturated,
        LLVMBasicBlockRef preheader,
        LLVMBasicBlockRef trapBlock,
        LLVMValueRef& combined,
        LLVMValueRef& effectiveScale,
        LLVMTypeRef wide)
{
        LLVMValueRef phi = LLVMBuildPhi(builder_, wide, "decimal.mul.result");
        LLVMValueRef scalePhi = LLVMBuildPhi(builder_, wide, "decimal.mul.scale");
        LLVMValueRef normalCombined[] = { combined };
        LLVMValueRef satCombined[] = { saturated };
        LLVMValueRef normalScale[] = { effectiveScale };
        LLVMValueRef satScale[] = { LLVMConstInt(wide, 0, 0) };
        LLVMAddIncoming(phi, normalCombined, &preheader, 1);
        LLVMAddIncoming(phi, satCombined, &trapBlock, 1);
        LLVMAddIncoming(scalePhi, normalScale, &preheader, 1);
        LLVMAddIncoming(scalePhi, satScale, &trapBlock, 1);
        combined = phi;
        effectiveScale = scalePhi;
}

bool OopLlvmImplementation::resolveDecimalMultiplyOverflow(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef& combined,
        LLVMValueRef& effectiveScale,
        std::string& error)
{
        // NS304-T15-F05: the coefficient must stay within 96 bits. The default (checked)
        // policy traps DecimalOverflow; `saturating(expr)` yields MaxValue/MinValue.
        std::uint64_t maskWords[2] = { 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFull };
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef mask96 = LLVMConstIntOfArbitraryPrecision(wide, 2, maskWords);
        LLVMValueRef overflow = LLVMBuildICmp(
            builder_, LLVMIntUGT, combined, mask96, "decimal.mul.overflow");
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef okBlock = appendBlock("decimal.mul.ok");
        LLVMBasicBlockRef trapBlock = appendBlock("decimal.mul.trap");
        LLVMBuildCondBr(builder_, overflow, trapBlock, okBlock);
        enterBlock(trapBlock);
        if (currentOverflowPolicy_ == "saturating")
        {
            LLVMValueRef saturated = buildSaturatedDecimalMultiply(left, right, mask96, wide);
            LLVMBuildBr(builder_, okBlock);
            enterBlock(okBlock);
            mergeSaturatedDecimalMultiply(
                saturated, preheader, trapBlock, combined, effectiveScale, wide);
            return true;
        }
        if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
            return fail(error, "decimal multiply overflow but no trap is configured");
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::DecimalOverflow), 0);
        LLVMValueRef trapArguments[] = { reason };
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
        enterBlock(okBlock);
        return true;
}

LLVMValueRef OopLlvmImplementation::applyDecimalMultiply(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef scaleMask,
        LLVMValueRef& effectiveScale,
        std::string& error)
{
        LLVMValueRef combined = LLVMBuildMul(
            builder_, leftCoefficient, rightCoefficient, "decimal.coeff.mul");
        LLVMValueRef rightScale = LLVMBuildAnd(
            builder_, LLVMBuildLShr(builder_, right, scaleShift, "decimal.right.scale.raw"),
            scaleMask, "decimal.right.scale");
        effectiveScale = LLVMBuildAdd(builder_, leftScale, rightScale, "decimal.scale.add");
        if (!emitDecimalScaleOverflow(effectiveScale, "decimal.mul.scale", error)) return nullptr;
        if (!resolveDecimalMultiplyOverflow(
                left, right, combined, effectiveScale, error)) return nullptr;
        return combined;
}

LLVMValueRef OopLlvmImplementation::applyDecimalDivide(
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef& effectiveScale,
        std::string& error)
{
        // div: result scale = max(leftScale, rightScale) + (leftScale > rightScale ? 1 : 0),
        // numerator = leftCoeff * 10^(rightScale - leftScale + target) / rightCoeff.
        // This preserves the exact quotient when the scales differ (1.5m / 2m -> 75e-2)
        // and keeps the equal-scale result at its natural scale (1.5m / 2.5m -> 6e-1).
        // NS304-T15-F04: the quotient is rounded half-up, so 1/3 rounds to the target
        // precision instead of truncating. 64-bit lane: __udivti3 is not linked.
        LLVMValueRef rightScale = LLVMBuildLShr(
            builder_, right, scaleShift, "decimal.div.right.scale");
        LLVMValueRef scaleGe = LLVMBuildICmp(
            builder_, LLVMIntUGE, leftScale, rightScale, "decimal.div.scale.ge");
        LLVMValueRef scaleGt = LLVMBuildICmp(
            builder_, LLVMIntUGT, leftScale, rightScale, "decimal.div.scale.gt");
        LLVMValueRef maxScale = LLVMBuildSelect(
            builder_, scaleGe, leftScale, rightScale, "decimal.div.scale.max");
        LLVMValueRef guard = LLVMBuildZExt(
            builder_, scaleGt, LLVMIntTypeInContext(context_, 128), "decimal.div.guard");
        LLVMValueRef target = LLVMBuildAdd(builder_, maxScale, guard, "decimal.div.scale.target");
        if (!emitDecimalScaleOverflow(target, "decimal.div.scale", error)) return nullptr;
        LLVMValueRef exponent = LLVMBuildSub(
            builder_, LLVMBuildAdd(builder_, target, rightScale, "decimal.div.exp.a"),
            leftScale, "decimal.div.exponent");
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMValueRef leftCoeff64 = LLVMBuildTrunc(
            builder_, leftCoefficient, lane, "decimal.div.left64");
        LLVMValueRef rightCoeff64 = LLVMBuildTrunc(
            builder_, rightCoefficient, lane, "decimal.div.right64");
        LLVMValueRef exponent64 = LLVMBuildTrunc(
            builder_, exponent, lane, "decimal.div.exponent64");
        LLVMValueRef numerator = LLVMBuildMul(
            builder_, leftCoeff64, buildPow10(exponent64, "decimal.div.pow10"),
            "decimal.div.num");
        LLVMValueRef half = LLVMBuildLShr(
            builder_, rightCoeff64, LLVMConstInt(lane, 1, 0), "decimal.div.half");
        LLVMValueRef rounded = LLVMBuildAdd(builder_, numerator, half, "decimal.div.roundnum");
        LLVMValueRef quotient = LLVMBuildUDiv(builder_, rounded, rightCoeff64, "decimal.div.quotient");
        effectiveScale = target;
        return LLVMBuildZExt(
            builder_, quotient, LLVMIntTypeInContext(context_, 128), "decimal.div.wide");
}

LLVMValueRef OopLlvmImplementation::applyDecimalRemainder(
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef& effectiveScale)
{
        // rem: align both coefficients to the max scale, then coefficient % coefficient.
        LLVMValueRef rightScale = LLVMBuildLShr(
            builder_, right, scaleShift, "decimal.rem.right.scale");
        LLVMValueRef scaleGe = LLVMBuildICmp(
            builder_, LLVMIntUGE, leftScale, rightScale, "decimal.rem.scale.ge");
        LLVMValueRef maxScale = LLVMBuildSelect(
            builder_, scaleGe, leftScale, rightScale, "decimal.rem.scale.max");
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMValueRef leftCoeff64 = LLVMBuildTrunc(
            builder_, leftCoefficient, lane, "decimal.rem.left64");
        LLVMValueRef rightCoeff64 = LLVMBuildTrunc(
            builder_, rightCoefficient, lane, "decimal.rem.right64");
        LLVMValueRef leftDiff = LLVMBuildTrunc(
            builder_, LLVMBuildSub(builder_, maxScale, leftScale, "decimal.rem.leftdiff"),
            lane, "decimal.rem.leftdiff64");
        LLVMValueRef rightDiff = LLVMBuildTrunc(
            builder_, LLVMBuildSub(builder_, maxScale, rightScale, "decimal.rem.rightdiff"),
            lane, "decimal.rem.rightdiff64");
        LLVMValueRef leftAligned = LLVMBuildMul(
            builder_, leftCoeff64, buildPow10(leftDiff, "decimal.rem.left.pow10"),
            "decimal.rem.left.align");
        LLVMValueRef rightAligned = LLVMBuildMul(
            builder_, rightCoeff64, buildPow10(rightDiff, "decimal.rem.right.pow10"),
            "decimal.rem.right.align");
        LLVMValueRef remainder = LLVMBuildURem(builder_, leftAligned, rightAligned, "decimal.rem");
        effectiveScale = maxScale;
        return LLVMBuildZExt(
            builder_, remainder, LLVMIntTypeInContext(context_, 128), "decimal.rem.wide");
}

LLVMValueRef OopLlvmImplementation::applyDecimalBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error)
{
        // NS304-T15-F04: decimal is coefficient (low 96 bits) | scale (bits 96..101) | sign
        // (bit 127). A plain i128 op would corrupt the scale bits, so extract the coefficients,
        // operate on them, and recombine. add/sub keep the (equal) left scale; mul adds the two
        // scales. div/rem + mixed-scale alignment and rounding arrive in a later increment.
        if (operation != lexer::TokenKind::Plus && operation != lexer::TokenKind::Minus &&
            operation != lexer::TokenKind::Star && operation != lexer::TokenKind::Slash &&
            operation != lexer::TokenKind::Percent)
            return failValue(error, "unsupported decimal binary operation in Stage 34");
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef coefficientMask = LLVMConstInt(wide, 0xFFFFFFFFFFFFFFFFull, 0);
        LLVMValueRef leftCoefficient = LLVMBuildAnd(
            builder_, left, coefficientMask, "decimal.left.coeff");
        LLVMValueRef rightCoefficient = LLVMBuildAnd(
            builder_, right, coefficientMask, "decimal.right.coeff");
        LLVMValueRef scaleShift = LLVMConstInt(wide, 96, 0);
        // NS304-T15-F05: the scale lives in bits 96..101; bits 102..126 are padding and bit 127
        // is the sign. A plain >>96 leaks the sign bit (and padding) into the scale, so a negative
        // operand's scale reads as 2^31+scale and trips the scale-overflow trap.
        LLVMValueRef scaleMask = LLVMConstInt(wide, 0x3F, 0);
        LLVMValueRef leftScale = LLVMBuildAnd(
            builder_, LLVMBuildLShr(builder_, left, scaleShift, "decimal.scale.raw"),
            scaleMask, "decimal.scale");
        LLVMValueRef effectiveScale = nullptr;
        LLVMValueRef combined = nullptr;
        if (operation == lexer::TokenKind::Plus || operation == lexer::TokenKind::Minus)
            combined = applyDecimalAddOrSubtract(
                operation, leftCoefficient, rightCoefficient, leftScale,
                right, scaleShift, scaleMask, effectiveScale);
        else if (operation == lexer::TokenKind::Star)
            combined = applyDecimalMultiply(
                left, right, leftCoefficient, rightCoefficient, leftScale,
                scaleShift, scaleMask, effectiveScale, error);
        else if (operation == lexer::TokenKind::Slash)
            combined = applyDecimalDivide(
                right, leftCoefficient, rightCoefficient, leftScale,
                scaleShift, effectiveScale, error);
        else
            combined = applyDecimalRemainder(
                right, leftCoefficient, rightCoefficient, leftScale,
                scaleShift, effectiveScale);
        if (combined == nullptr) return nullptr;
        LLVMValueRef scaleShifted = LLVMBuildShl(
            builder_, effectiveScale, scaleShift, "decimal.scale.sh");
        return LLVMBuildOr(builder_, combined, scaleShifted, "decimal.arith");
}

LLVMValueRef OopLlvmImplementation::applyIntegerBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero)
{
        switch (operation)
        {
            case lexer::TokenKind::Plus: return LLVMBuildAdd(builder_, left, right, "add");
            case lexer::TokenKind::Minus: return LLVMBuildSub(builder_, left, right, "sub");
            case lexer::TokenKind::Star: return LLVMBuildMul(builder_, left, right, "mul");
            case lexer::TokenKind::Slash:
            case lexer::TokenKind::Percent:
                return applyIntegerDivision(
                    operation, left, right, operandType, error, divisorProvenNonZero);
            case lexer::TokenKind::Amp: return LLVMBuildAnd(builder_, left, right, "and");
            case lexer::TokenKind::Pipe: return LLVMBuildOr(builder_, left, right, "or");
            case lexer::TokenKind::Caret: return LLVMBuildXor(builder_, left, right, "xor");
            case lexer::TokenKind::LessLess: return LLVMBuildShl(builder_, left, right, "shl");
            case lexer::TokenKind::GreaterGreater:
                return isSignedInteger(operandType)
                    ? LLVMBuildAShr(builder_, left, right, "ashr")
                    : LLVMBuildLShr(builder_, left, right, "lshr");
            default: return failValue(error, "unsupported integer binary operation in Stage 34");
        }
}

LLVMValueRef OopLlvmImplementation::applyIntegerDivision(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero)
{
        LLVMTypeRef integerType = llvmType(operandType, error);
        if (integerType == nullptr) return nullptr;

        const bool remainder = operation == lexer::TokenKind::Percent;
        LLVMValueRef zero = LLVMConstInt(integerType, 0, 0);
        if (!divisorProvenNonZero)
        {
            LLVMValueRef nonZero = LLVMBuildICmp(builder_, LLVMIntNE, right, zero, "div.nonzero");
            if (!emitGuard(nonZero, LlvmTrapReason::DivisionByZero, "div.zero", error))
                return nullptr;
        }

        if (!isSignedInteger(operandType))
        {
            return remainder
                ? LLVMBuildURem(builder_, left, right, "urem")
                : LLVMBuildUDiv(builder_, left, right, "udiv");
        }

        LLVMValueRef divisor = correctedSignedDivisor(
            builder_, integerType, left, right, integerWidth(operandType), "div");
        return remainder
            ? LLVMBuildSRem(builder_, left, divisor, "srem")
            : LLVMBuildSDiv(builder_, left, divisor, "sdiv");
    }

}
