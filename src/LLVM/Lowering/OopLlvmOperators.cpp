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
        if (operandType == "builtin:decimal" &&
            (operation == lexer::TokenKind::EqualEqual || operation == lexer::TokenKind::BangEqual))
            return applyDecimalEquality(operation, left, right);
        if (operandType == "builtin:decimal" &&
            (operation == lexer::TokenKind::Less || operation == lexer::TokenKind::LessEqual ||
             operation == lexer::TokenKind::Greater || operation == lexer::TokenKind::GreaterEqual))
        {
            LLVMTypeRef encoded = LLVMIntTypeInContext(context_, 128);
            LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
            std::uint64_t coefficientWords[2] = {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFull};
            LLVMValueRef mask = LLVMConstIntOfArbitraryPrecision(encoded, 2, coefficientWords);
            LLVMValueRef leftCoeff = LLVMBuildAnd(builder_, left, mask, "decimal.order.left.coeff");
            LLVMValueRef rightCoeff = LLVMBuildAnd(builder_, right, mask, "decimal.order.right.coeff");
            LLVMValueRef shift = LLVMConstInt(encoded, 96, 0);
            LLVMValueRef scaleMask = LLVMConstInt(encoded, 63, 0);
            LLVMValueRef leftScale = LLVMBuildTrunc(builder_, LLVMBuildAnd(builder_,
                LLVMBuildLShr(builder_, left, shift, "decimal.order.left.scale.raw"), scaleMask,
                "decimal.order.left.scale.mask"), countType, "decimal.order.left.scale");
            LLVMValueRef rightScale = LLVMBuildTrunc(builder_, LLVMBuildAnd(builder_,
                LLVMBuildLShr(builder_, right, shift, "decimal.order.right.scale.raw"), scaleMask,
                "decimal.order.right.scale.mask"), countType, "decimal.order.right.scale");
            LLVMValueRef scaleLess = LLVMBuildICmp(builder_, LLVMIntULT, leftScale, rightScale, "decimal.order.scale.lt");
            LLVMValueRef maximum = LLVMBuildSelect(builder_, scaleLess, rightScale, leftScale, "decimal.order.scale.max");
            LLVMValueRef alignedLeft = alignDecimalCoefficient(leftCoeff,
                LLVMBuildSub(builder_, maximum, leftScale, "decimal.order.left.diff"));
            LLVMValueRef alignedRight = alignDecimalCoefficient(rightCoeff,
                LLVMBuildSub(builder_, maximum, rightScale, "decimal.order.right.diff"));
            LLVMValueRef magnitudeLess = LLVMBuildICmp(builder_, LLVMIntULT, alignedLeft, alignedRight, "decimal.order.mag.lt");
            LLVMValueRef magnitudeGreater = LLVMBuildICmp(builder_, LLVMIntUGT, alignedLeft, alignedRight, "decimal.order.mag.gt");
            LLVMValueRef magnitudeEqual = LLVMBuildICmp(builder_, LLVMIntEQ, alignedLeft, alignedRight, "decimal.order.mag.eq");
            LLVMValueRef zero = LLVMConstInt(encoded, 0, 0);
            LLVMValueRef signShift = LLVMConstInt(encoded, 127, 0);
            LLVMValueRef leftNegative = LLVMBuildAnd(builder_,
                LLVMBuildICmp(builder_, LLVMIntNE, LLVMBuildLShr(builder_, left, signShift, "decimal.order.left.sign"), zero, "decimal.order.left.signed"),
                LLVMBuildICmp(builder_, LLVMIntNE, leftCoeff, zero, "decimal.order.left.nonzero"), "decimal.order.left.negative");
            LLVMValueRef rightNegative = LLVMBuildAnd(builder_,
                LLVMBuildICmp(builder_, LLVMIntNE, LLVMBuildLShr(builder_, right, signShift, "decimal.order.right.sign"), zero, "decimal.order.right.signed"),
                LLVMBuildICmp(builder_, LLVMIntNE, rightCoeff, zero, "decimal.order.right.nonzero"), "decimal.order.right.negative");
            LLVMValueRef differentSigns = LLVMBuildXor(builder_, leftNegative, rightNegative, "decimal.order.signs.differ");
            LLVMValueRef sameSignLess = LLVMBuildSelect(builder_, leftNegative, magnitudeGreater, magnitudeLess, "decimal.order.same.sign.lt");
            LLVMValueRef less = LLVMBuildSelect(builder_, differentSigns, leftNegative, sameSignLess, "decimal.order.lt");
            LLVMValueRef equal = LLVMBuildAnd(builder_, magnitudeEqual,
                LLVMBuildNot(builder_, differentSigns, "decimal.order.signs.same"), "decimal.order.eq");
            if (operation == lexer::TokenKind::Less) return less;
            if (operation == lexer::TokenKind::LessEqual) return LLVMBuildOr(builder_, less, equal, "decimal.order.le");
            if (operation == lexer::TokenKind::GreaterEqual) return LLVMBuildNot(builder_, less, "decimal.order.ge");
            return LLVMBuildNot(builder_, LLVMBuildOr(builder_, less, equal, "decimal.order.le.for.gt"), "decimal.order.gt");
        }
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

LLVMValueRef OopLlvmImplementation::alignDecimalCoefficient(
        LLVMValueRef coefficient, LLVMValueRef scaleDifference)
{
        LLVMTypeRef alignedType = LLVMIntTypeInContext(context_, 320);
        LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
        LLVMValueRef initial = LLVMBuildZExt(builder_, coefficient, alignedType, "decimal.eq.coeff.wide");
        LLVMBasicBlockRef initialBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loopBlock = appendBlock("decimal.eq.align");
        LLVMBasicBlockRef doneBlock = appendBlock("decimal.eq.done");
        LLVMValueRef nonzero = LLVMBuildICmp(builder_, LLVMIntNE, scaleDifference,
                                            LLVMConstInt(countType, 0, 0), "decimal.eq.needs.scale");
        LLVMBuildCondBr(builder_, nonzero, loopBlock, doneBlock);
        enterBlock(loopBlock);
        LLVMValueRef current = LLVMBuildPhi(builder_, alignedType, "decimal.eq.coeff");
        LLVMValueRef currentIndex = LLVMBuildPhi(builder_, countType, "decimal.eq.index");
        LLVMValueRef next = LLVMBuildAdd(builder_,
            LLVMBuildShl(builder_, current, LLVMConstInt(alignedType, 3, 0), "decimal.eq.x8"),
            LLVMBuildShl(builder_, current, LLVMConstInt(alignedType, 1, 0), "decimal.eq.x2"), "decimal.eq.x10");
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, currentIndex, LLVMConstInt(countType, 1, 0),
                                             "decimal.eq.index.next");
        LLVMValueRef more = LLVMBuildICmp(builder_, LLVMIntULT, nextIndex, scaleDifference, "decimal.eq.more");
        LLVMBuildCondBr(builder_, more, loopBlock, doneBlock);
        LLVMValueRef values[] = {initial, next};
        LLVMValueRef indexValues[] = {LLVMConstInt(countType, 0, 0), nextIndex};
        LLVMBasicBlockRef blocks[] = {initialBlock, loopBlock};
        LLVMAddIncoming(current, values, blocks, 2);
        LLVMAddIncoming(currentIndex, indexValues, blocks, 2);
        enterBlock(doneBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, alignedType, "decimal.eq.aligned");
        LLVMAddIncoming(result, values, blocks, 2);
        return result;
}

LLVMValueRef OopLlvmImplementation::applyDecimalEquality(
        lexer::TokenKind operation, LLVMValueRef left, LLVMValueRef right)
{
        LLVMTypeRef encoded = LLVMIntTypeInContext(context_, 128);
        LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
        std::uint64_t coefficientWords[2] = {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFull};
        LLVMValueRef coefficientMask = LLVMConstIntOfArbitraryPrecision(encoded, 2, coefficientWords);
        LLVMValueRef leftCoeff = LLVMBuildAnd(builder_, left, coefficientMask, "decimal.eq.left.coeff");
        LLVMValueRef rightCoeff = LLVMBuildAnd(builder_, right, coefficientMask, "decimal.eq.right.coeff");
        LLVMValueRef scaleShift = LLVMConstInt(encoded, 96, 0);
        LLVMValueRef scaleMask = LLVMConstInt(encoded, 63, 0);
        LLVMValueRef leftScale = LLVMBuildTrunc(builder_, LLVMBuildAnd(builder_,
            LLVMBuildLShr(builder_, left, scaleShift, "decimal.eq.left.scale.raw"), scaleMask,
            "decimal.eq.left.scale.masked"), countType, "decimal.eq.left.scale");
        LLVMValueRef rightScale = LLVMBuildTrunc(builder_, LLVMBuildAnd(builder_,
            LLVMBuildLShr(builder_, right, scaleShift, "decimal.eq.right.scale.raw"), scaleMask,
            "decimal.eq.right.scale.masked"), countType, "decimal.eq.right.scale");
        LLVMValueRef leftSmaller = LLVMBuildICmp(builder_, LLVMIntULT, leftScale, rightScale, "decimal.eq.scale.lt");
        LLVMValueRef maximum = LLVMBuildSelect(builder_, leftSmaller, rightScale, leftScale, "decimal.eq.scale.max");
        LLVMValueRef leftDiff = LLVMBuildSub(builder_, maximum, leftScale, "decimal.eq.left.diff");
        LLVMValueRef rightDiff = LLVMBuildSub(builder_, maximum, rightScale, "decimal.eq.right.diff");
        LLVMValueRef alignedLeft = alignDecimalCoefficient(leftCoeff, leftDiff);
        LLVMValueRef alignedRight = alignDecimalCoefficient(rightCoeff, rightDiff);
        LLVMValueRef equalMagnitude = LLVMBuildICmp(builder_, LLVMIntEQ, alignedLeft, alignedRight,
                                                    "decimal.eq.magnitude");
        LLVMValueRef signDifference = LLVMBuildLShr(builder_, LLVMBuildXor(builder_, left, right, "decimal.eq.xor"),
                                                   LLVMConstInt(encoded, 127, 0), "decimal.eq.sign.diff");
        LLVMValueRef equalSigns = LLVMBuildICmp(builder_, LLVMIntEQ, signDifference, LLVMConstInt(encoded, 0, 0),
                                               "decimal.eq.sign.same");
        LLVMValueRef bothZero = LLVMBuildICmp(builder_, LLVMIntEQ,
            LLVMBuildOr(builder_, leftCoeff, rightCoeff, "decimal.eq.coefficients.or"),
            LLVMConstInt(encoded, 0, 0), "decimal.eq.both.zero");
        LLVMValueRef equal = LLVMBuildAnd(builder_, equalMagnitude,
            LLVMBuildOr(builder_, equalSigns, bothZero, "decimal.eq.sign.ok"), "decimal.eq.value");
        return operation == lexer::TokenKind::BangEqual ? LLVMBuildNot(builder_, equal, "decimal.ne") : equal;
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
        LLVMValueRef left,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef right,
        LLVMValueRef scaleShift,
        LLVMValueRef scaleMask,
        LLVMValueRef& effectiveScale,
        std::string& error)
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
        LLVMTypeRef encoded = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef signShift = LLVMConstInt(encoded, 127, 0);
        LLVMValueRef leftNegative = LLVMBuildICmp(builder_, LLVMIntNE,
            LLVMBuildLShr(builder_, left, signShift, "decimal.add.left.sign"),
            LLVMConstInt(encoded, 0, 0), "decimal.add.left.negative");
        LLVMValueRef rightNegative = LLVMBuildICmp(builder_, LLVMIntNE,
            LLVMBuildLShr(builder_, right, signShift, "decimal.add.right.sign"),
            LLVMConstInt(encoded, 0, 0), "decimal.add.right.negative");
        if (operation == lexer::TokenKind::Minus)
            rightNegative = LLVMBuildNot(builder_, rightNegative, "decimal.sub.right.effective.sign");
        LLVMValueRef sameSigns = LLVMBuildICmp(builder_, LLVMIntEQ, leftNegative, rightNegative,
                                              "decimal.add.same.signs");
        LLVMValueRef leftLarger = LLVMBuildICmp(builder_, LLVMIntUGE, leftAligned, rightAligned,
                                               "decimal.add.left.larger");
        LLVMValueRef sum = LLVMBuildAdd(builder_, leftAligned, rightAligned, "decimal.coeff.add");
        LLVMValueRef difference = LLVMBuildSelect(builder_, leftLarger,
            LLVMBuildSub(builder_, leftAligned, rightAligned, "decimal.coeff.left.minus.right"),
            LLVMBuildSub(builder_, rightAligned, leftAligned, "decimal.coeff.right.minus.left"),
            "decimal.coeff.difference");
        LLVMValueRef aligned = LLVMBuildSelect(builder_, sameSigns, sum, difference, "decimal.coeff.result");
        LLVMValueRef differenceSign = LLVMBuildSelect(builder_, leftLarger, leftNegative, rightNegative,
                                                      "decimal.add.difference.sign");
        LLVMValueRef negative = LLVMBuildSelect(builder_, sameSigns, leftNegative, differenceSign,
                                               "decimal.add.result.sign");
        negative = LLVMBuildAnd(builder_, negative,
            LLVMBuildICmp(builder_, LLVMIntNE, aligned, LLVMConstInt(lane, 0, 0), "decimal.add.nonzero"),
            "decimal.add.sign.nonzero");
        effectiveScale = maxScale;
        LLVMValueRef coefficient = LLVMBuildZExt(builder_, aligned, encoded, "decimal.coeff.wide");
        LLVMValueRef sign = LLVMBuildShl(builder_, LLVMBuildZExt(builder_, negative, encoded,
            "decimal.add.sign.wide"), signShift, "decimal.add.sign.bit");
        LLVMValueRef previous = LLVMBuildOr(builder_, coefficient, sign, "decimal.add.signed");

        // Compute the exact aligned magnitudes using the full 96-bit carriers.
        // If the result cannot be represented at this scale, keep the previous
        // behavior: overflow/normalization policy is not determined here.
        std::uint64_t coefficientWords[2] = {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFull};
        LLVMValueRef fullMask = LLVMConstIntOfArbitraryPrecision(encoded, 2, coefficientWords);
        LLVMValueRef fullLeft = LLVMBuildAnd(builder_, left, fullMask, "decimal.add.full.left");
        LLVMValueRef fullRight = LLVMBuildAnd(builder_, right, fullMask, "decimal.add.full.right");
        LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
        LLVMValueRef wideLeft = alignDecimalCoefficient(fullLeft,
            LLVMBuildTrunc(builder_, leftDiff, countType, "decimal.add.wide.left.diff"));
        LLVMValueRef wideRight = alignDecimalCoefficient(fullRight,
            LLVMBuildTrunc(builder_, rightDiff, countType, "decimal.add.wide.right.diff"));
        LLVMTypeRef alignedType = LLVMTypeOf(wideLeft);
        LLVMValueRef wideLeftLarger = LLVMBuildICmp(builder_, LLVMIntUGE, wideLeft, wideRight,
                                                   "decimal.add.wide.left.larger");
        LLVMValueRef wideSum = LLVMBuildAdd(builder_, wideLeft, wideRight, "decimal.add.wide.sum");
        LLVMValueRef wideDifference = LLVMBuildSelect(builder_, wideLeftLarger,
            LLVMBuildSub(builder_, wideLeft, wideRight, "decimal.add.wide.left.minus.right"),
            LLVMBuildSub(builder_, wideRight, wideLeft, "decimal.add.wide.right.minus.left"),
            "decimal.add.wide.difference");
        LLVMValueRef wideResult = LLVMBuildSelect(builder_, sameSigns, wideSum, wideDifference,
                                                  "decimal.add.wide.result");
        LLVMValueRef wideDifferenceSign = LLVMBuildSelect(builder_, wideLeftLarger,
            leftNegative, rightNegative, "decimal.add.wide.difference.sign");
        LLVMValueRef wideNegative = LLVMBuildSelect(builder_, sameSigns, leftNegative,
            wideDifferenceSign, "decimal.add.wide.result.sign");
        wideNegative = LLVMBuildAnd(builder_, wideNegative,
            LLVMBuildICmp(builder_, LLVMIntNE, wideResult, LLVMConstInt(alignedType, 0, 0),
                          "decimal.add.wide.nonzero"), "decimal.add.wide.sign.nonzero");
        LLVMValueRef wideMask = LLVMBuildZExt(builder_, fullMask, alignedType, "decimal.add.wide.mask");
        LLVMValueRef needsReduction = LLVMBuildAnd(builder_,
            LLVMBuildICmp(builder_, LLVMIntUGT, wideResult, wideMask, "decimal.add.reduce.large"),
            LLVMBuildICmp(builder_, LLVMIntNE, maxScale, LLVMConstInt(encoded, 0, 0), "decimal.add.reduce.scale"),
            "decimal.add.reduce.needed");
        LLVMBasicBlockRef reductionEntry = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef reductionLoop = appendBlock("decimal.add.reduce.loop");
        LLVMBasicBlockRef reductionDone = appendBlock("decimal.add.reduce.done");
        LLVMBuildCondBr(builder_, needsReduction, reductionLoop, reductionDone);
        enterBlock(reductionLoop);
        LLVMValueRef current = LLVMBuildPhi(builder_, alignedType, "decimal.add.reduce.value");
        LLVMValueRef currentScale = LLVMBuildPhi(builder_, encoded, "decimal.add.reduce.current.scale");
        LLVMTypeRef wordType = LLVMInt64TypeInContext(context_);
        LLVMValueRef remainder = LLVMConstInt(wordType, 0, 0);
        LLVMValueRef quotient = LLVMConstInt(alignedType, 0, 0);
        // Long division of ten 32-bit limbs by ten. The carried remainder is
        // below ten, so each 64-bit chunk contains at most 36 significant bits.
        for (int limb = 9; limb >= 0; --limb)
        {
            LLVMValueRef shifted = LLVMBuildLShr(builder_, current,
                LLVMConstInt(alignedType, static_cast<unsigned>(limb) * 32U, 0), "decimal.add.reduce.shift");
            LLVMValueRef lowWord = LLVMBuildZExt(builder_, LLVMBuildTrunc(builder_, shifted, countType,
                "decimal.add.reduce.word32"), wordType, "decimal.add.reduce.word64");
            LLVMValueRef chunk = LLVMBuildOr(builder_, LLVMBuildShl(builder_, remainder,
                LLVMConstInt(wordType, 32, 0), "decimal.add.reduce.carry"), lowWord, "decimal.add.reduce.chunk");
            LLVMValueRef quotientWord = LLVMBuildUDiv(builder_, chunk, LLVMConstInt(wordType, 10, 0),
                                                      "decimal.add.reduce.quotient.word");
            remainder = LLVMBuildURem(builder_, chunk, LLVMConstInt(wordType, 10, 0), "decimal.add.reduce.remainder");
            LLVMValueRef wideWord = LLVMBuildZExt(builder_, quotientWord, alignedType, "decimal.add.reduce.word.wide");
            quotient = LLVMBuildOr(builder_, quotient, LLVMBuildShl(builder_, wideWord,
                LLVMConstInt(alignedType, static_cast<unsigned>(limb) * 32U, 0), "decimal.add.reduce.word.position"),
                "decimal.add.reduce.quotient");
        }
        LLVMValueRef divisible = LLVMBuildICmp(builder_, LLVMIntEQ, remainder, LLVMConstInt(wordType, 0, 0),
                                               "decimal.add.reduce.exact");
        LLVMValueRef nextValue = LLVMBuildSelect(builder_, divisible, quotient, current, "decimal.add.reduce.next");
        LLVMValueRef nextScale = LLVMBuildSelect(builder_, divisible,
            LLVMBuildSub(builder_, currentScale, LLVMConstInt(encoded, 1, 0), "decimal.add.reduce.scale.minus.one"),
            currentScale, "decimal.add.reduce.next.scale");
        LLVMValueRef more = LLVMBuildAnd(builder_, divisible, LLVMBuildAnd(builder_,
            LLVMBuildICmp(builder_, LLVMIntUGT, nextValue, wideMask, "decimal.add.reduce.still.large"),
            LLVMBuildICmp(builder_, LLVMIntNE, nextScale, LLVMConstInt(encoded, 0, 0), "decimal.add.reduce.has.scale"),
            "decimal.add.reduce.needs.more"), "decimal.add.reduce.repeat");
        LLVMBuildCondBr(builder_, more, reductionLoop, reductionDone);
        LLVMBasicBlockRef reductionBlocks[] = {reductionEntry, reductionLoop};
        LLVMValueRef reductionValues[] = {wideResult, nextValue};
        LLVMValueRef reductionScales[] = {maxScale, nextScale};
        LLVMAddIncoming(current, reductionValues, reductionBlocks, 2U);
        LLVMAddIncoming(currentScale, reductionScales, reductionBlocks, 2U);
        enterBlock(reductionDone);
        LLVMValueRef reduced = LLVMBuildPhi(builder_, alignedType, "decimal.add.reduce.result");
        LLVMValueRef reducedScale = LLVMBuildPhi(builder_, encoded, "decimal.add.reduce.result.scale");
        LLVMAddIncoming(reduced, reductionValues, reductionBlocks, 2U);
        LLVMAddIncoming(reducedScale, reductionScales, reductionBlocks, 2U);
        LLVMValueRef fits = LLVMBuildICmp(builder_, LLVMIntULE, reduced, wideMask, "decimal.add.wide.fits");
        if (currentOverflowPolicy_.empty() || currentOverflowPolicy_ == "checked")
        {
            // At scale zero after exact reductions an out-of-range integer
            // cannot be rescued by further reduction or a rounding decision.
            LLVMValueRef nonzeroScale = LLVMBuildICmp(builder_, LLVMIntNE, reducedScale,
                LLVMConstInt(encoded, 0, 0), "decimal.add.overflow.nonzero.scale");
            LLVMValueRef valid = LLVMBuildOr(builder_, fits, nonzeroScale, "decimal.add.overflow.valid");
            if (!emitGuard(valid, LlvmTrapReason::DecimalOverflow, "decimal.add.overflow", error))
                return nullptr;
        }
        effectiveScale = LLVMBuildSelect(builder_, fits, reducedScale, maxScale, "decimal.add.final.scale");
        LLVMValueRef exactCoefficient = LLVMBuildTrunc(builder_, reduced, encoded, "decimal.add.exact.coeff");
        LLVMValueRef exactSign = LLVMBuildShl(builder_, LLVMBuildZExt(builder_, wideNegative, encoded,
            "decimal.add.exact.sign.wide"), signShift, "decimal.add.exact.sign");
        LLVMValueRef exact = LLVMBuildOr(builder_, exactCoefficient, exactSign, "decimal.add.exact");
        return LLVMBuildSelect(builder_, fits, exact, previous, "decimal.add.result");
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
                operation, left, leftCoefficient, rightCoefficient, leftScale,
                right, scaleShift, scaleMask, effectiveScale, error);
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
        if (operation == lexer::TokenKind::Plus && operandType == "builtin:i128" &&
            currentOverflowPolicy_ == "checked")
        {
            if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
                return failValue(error, "checked integer overflow but no trap is configured");
            constexpr std::string_view name = "llvm.sadd.with.overflow";
            const unsigned id = LLVMLookupIntrinsicID(name.data(), name.size());
            LLVMTypeRef type = LLVMTypeOf(left);
            LLVMValueRef intrinsic = LLVMGetIntrinsicDeclaration(module_, id, &type, 1U);
            LLVMValueRef arguments[] = {left, right};
            LLVMValueRef pair = LLVMBuildCall2(
                builder_, LLVMGlobalGetValueType(intrinsic), intrinsic, arguments, 2U, "checked.add");
            LLVMValueRef value = LLVMBuildExtractValue(builder_, pair, 0, "checked.add.value");
            LLVMValueRef overflow = LLVMBuildExtractValue(builder_, pair, 1, "checked.add.overflow");
            LLVMBasicBlockRef trapBlock = appendBlock("checked.add.trap");
            LLVMBasicBlockRef okBlock = appendBlock("checked.add.ok");
            LLVMBuildCondBr(builder_, overflow, trapBlock, okBlock);
            enterBlock(trapBlock);
            LLVMValueRef reason = LLVMConstInt(
                LLVMInt32TypeInContext(context_),
                static_cast<std::uint64_t>(LlvmTrapReason::IntegerOverflow), 0);
            LLVMValueRef trapArguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
            LLVMBuildUnreachable(builder_);
            enterBlock(okBlock);
            return value;
        }
        if (operation == lexer::TokenKind::Plus && operandType == "builtin:int" &&
            currentOverflowPolicy_ == "saturating")
        {
            constexpr std::string_view name = "llvm.sadd.sat";
            const unsigned id = LLVMLookupIntrinsicID(name.data(), name.size());
            LLVMTypeRef type = LLVMTypeOf(left);
            LLVMValueRef intrinsic = LLVMGetIntrinsicDeclaration(module_, id, &type, 1U);
            LLVMValueRef arguments[] = {left, right};
            return LLVMBuildCall2(
                builder_, LLVMGlobalGetValueType(intrinsic), intrinsic, arguments, 2U, "saturating.add");
        }
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

// Divide unsigned magnitudes one bit at a time, then restore the quotient sign.
// The carry preserves the 129th remainder bit; exactly 128 iterations are emitted.
LLVMValueRef OopLlvmImplementation::emitSignedI128Division(LLVMValueRef left, LLVMValueRef right)
{
        LLVMTypeRef wide = LLVMTypeOf(left);
        LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
        LLVMValueRef zero = LLVMConstInt(wide, 0, 0);
        LLVMValueRef one = LLVMConstInt(wide, 1, 0);
        LLVMValueRef signBit = LLVMConstInt(wide, 127, 0);
        LLVMValueRef leftSign = LLVMBuildAShr(builder_, left, signBit, "i128.div.left.sign");
        LLVMValueRef rightSign = LLVMBuildAShr(builder_, right, signBit, "i128.div.right.sign");
        LLVMValueRef numerator = LLVMBuildSub(
            builder_, LLVMBuildXor(builder_, left, leftSign, "i128.div.left.xor"), leftSign, "i128.div.left.abs");
        LLVMValueRef denominator = LLVMBuildSub(
            builder_, LLVMBuildXor(builder_, right, rightSign, "i128.div.right.xor"), rightSign, "i128.div.right.abs");
        LLVMBasicBlockRef entry = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loop = appendBlock("i128.div.loop");
        LLVMBasicBlockRef done = appendBlock("i128.div.done");
        LLVMBuildBr(builder_, loop);
        enterBlock(loop);
        LLVMValueRef n = LLVMBuildPhi(builder_, wide, "i128.div.n");
        LLVMValueRef r = LLVMBuildPhi(builder_, wide, "i128.div.r");
        LLVMValueRef q = LLVMBuildPhi(builder_, wide, "i128.div.q");
        LLVMValueRef i = LLVMBuildPhi(builder_, countType, "i128.div.i");
        LLVMValueRef carry = LLVMBuildICmp(builder_, LLVMIntNE,
            LLVMBuildLShr(builder_, r, signBit, "i128.div.carry.bit"), zero, "i128.div.carry");
        LLVMValueRef shifted = LLVMBuildOr(builder_, LLVMBuildShl(builder_, r, one, "i128.div.r.shift"),
            LLVMBuildLShr(builder_, n, signBit, "i128.div.next.bit"), "i128.div.r.shifted");
        LLVMValueRef subtract = LLVMBuildOr(builder_, carry,
            LLVMBuildICmp(builder_, LLVMIntUGE, shifted, denominator, "i128.div.ge"), "i128.div.subtract");
        LLVMValueRef nextR = LLVMBuildSelect(builder_, subtract,
            LLVMBuildSub(builder_, shifted, denominator, "i128.div.sub"), shifted, "i128.div.r.next");
        LLVMValueRef nextQ = LLVMBuildOr(builder_, LLVMBuildShl(builder_, q, one, "i128.div.q.shift"),
            LLVMBuildZExt(builder_, subtract, wide, "i128.div.q.bit"), "i128.div.q.next");
        LLVMValueRef nextN = LLVMBuildShl(builder_, n, one, "i128.div.n.next");
        LLVMValueRef nextI = LLVMBuildAdd(
            builder_, i, LLVMConstInt(countType, 1, 0), "i128.div.i.next");
        LLVMBuildCondBr(builder_, LLVMBuildICmp(builder_, LLVMIntEQ, nextI,
            LLVMConstInt(countType, 128, 0), "i128.div.finished"), done, loop);
        LLVMBasicBlockRef blocks[] = {entry, loop};
        LLVMValueRef nValues[] = {numerator, nextN};
        LLVMValueRef rValues[] = {zero, nextR};
        LLVMValueRef qValues[] = {zero, nextQ};
        LLVMValueRef iValues[] = {LLVMConstInt(countType, 0, 0), nextI};
        LLVMAddIncoming(n, nValues, blocks, 2U);
        LLVMAddIncoming(r, rValues, blocks, 2U);
        LLVMAddIncoming(q, qValues, blocks, 2U);
        LLVMAddIncoming(i, iValues, blocks, 2U);
        enterBlock(done);
        LLVMValueRef sign = LLVMBuildXor(builder_, leftSign, rightSign, "i128.div.sign");
        return LLVMBuildSub(builder_, LLVMBuildXor(builder_, nextQ, sign, "i128.div.signed.xor"),
            sign, "i128.div.signed");
}

// Unsigned restoring division, including the 129th remainder carry.
LLVMValueRef OopLlvmImplementation::emitUnsignedI128Division(LLVMValueRef left, LLVMValueRef right)
{
        LLVMTypeRef wide = LLVMTypeOf(left);
        LLVMTypeRef countType = LLVMInt32TypeInContext(context_);
        LLVMValueRef zero = LLVMConstInt(wide, 0, 0);
        LLVMValueRef one = LLVMConstInt(wide, 1, 0);
        LLVMValueRef topBit = LLVMConstInt(wide, 127, 0);
        LLVMBasicBlockRef entry = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef loop = appendBlock("u128.div.loop");
        LLVMBasicBlockRef done = appendBlock("u128.div.done");
        LLVMBuildBr(builder_, loop);
        enterBlock(loop);
        LLVMValueRef n = LLVMBuildPhi(builder_, wide, "u128.div.n");
        LLVMValueRef r = LLVMBuildPhi(builder_, wide, "u128.div.r");
        LLVMValueRef q = LLVMBuildPhi(builder_, wide, "u128.div.q");
        LLVMValueRef i = LLVMBuildPhi(builder_, countType, "u128.div.i");
        LLVMValueRef carry = LLVMBuildICmp(builder_, LLVMIntNE,
            LLVMBuildLShr(builder_, r, topBit, "u128.div.carry.bit"), zero, "u128.div.carry");
        LLVMValueRef shifted = LLVMBuildOr(builder_, LLVMBuildShl(builder_, r, one, "u128.div.r.shift"),
            LLVMBuildLShr(builder_, n, topBit, "u128.div.next.bit"), "u128.div.r.shifted");
        LLVMValueRef subtract = LLVMBuildOr(builder_, carry,
            LLVMBuildICmp(builder_, LLVMIntUGE, shifted, right, "u128.div.ge"), "u128.div.subtract");
        LLVMValueRef nextR = LLVMBuildSelect(builder_, subtract,
            LLVMBuildSub(builder_, shifted, right, "u128.div.sub"), shifted, "u128.div.r.next");
        LLVMValueRef nextQ = LLVMBuildOr(builder_, LLVMBuildShl(builder_, q, one, "u128.div.q.shift"),
            LLVMBuildZExt(builder_, subtract, wide, "u128.div.q.bit"), "u128.div.q.next");
        LLVMValueRef nextN = LLVMBuildShl(builder_, n, one, "u128.div.n.next");
        LLVMValueRef nextI = LLVMBuildAdd(builder_, i, LLVMConstInt(countType, 1, 0), "u128.div.i.next");
        LLVMBuildCondBr(builder_, LLVMBuildICmp(builder_, LLVMIntEQ, nextI,
            LLVMConstInt(countType, 128, 0), "u128.div.finished"), done, loop);
        LLVMBasicBlockRef blocks[] = {entry, loop};
        LLVMValueRef nValues[] = {left, nextN};
        LLVMValueRef rValues[] = {zero, nextR};
        LLVMValueRef qValues[] = {zero, nextQ};
        LLVMValueRef iValues[] = {LLVMConstInt(countType, 0, 0), nextI};
        LLVMAddIncoming(n, nValues, blocks, 2U);
        LLVMAddIncoming(r, rValues, blocks, 2U);
        LLVMAddIncoming(q, qValues, blocks, 2U);
        LLVMAddIncoming(i, iValues, blocks, 2U);
        enterBlock(done);
        return nextQ;
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
            if (remainder && operandType == "builtin:u128")
            {
                LLVMValueRef quotient = emitUnsignedI128Division(left, right);
                return LLVMBuildSub(builder_, left,
                    LLVMBuildMul(builder_, quotient, right, "u128.rem.product"), "u128.rem");
            }
            if (!remainder && operandType == "builtin:u128")
                return emitUnsignedI128Division(left, right);
            return remainder
                ? LLVMBuildURem(builder_, left, right, "urem")
                : LLVMBuildUDiv(builder_, left, right, "udiv");
        }

        if (remainder && operandType == "builtin:i128")
        {
            LLVMValueRef quotient = emitSignedI128Division(left, right);
            // The quotient loop and these plain i128 operations use modulo 2^128;
            // MinValue % -1 is therefore zero without signed division poison.
            return LLVMBuildSub(builder_, left,
                LLVMBuildMul(builder_, quotient, right, "i128.rem.product"), "i128.rem");
        }
        if (!remainder && operandType == "builtin:i128")
            return emitSignedI128Division(left, right);
        LLVMValueRef divisor = correctedSignedDivisor(
            builder_, integerType, left, right, integerWidth(operandType), "div");
        return remainder
            ? LLVMBuildSRem(builder_, left, divisor, "srem")
            : LLVMBuildSDiv(builder_, left, divisor, "sdiv");
    }

}
