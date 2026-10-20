#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::lowerMatchExpression(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() < 2 || node->children.front() == nullptr)
            return failValue(error, "T03-F03 match expression MIR is incomplete");

        LLVMValueRef governingAddress = nullptr;
        LLVMValueRef governing = lowerMatchGoverningValue(node, governingAddress, error);
        if (governing == nullptr) return nullptr;
        if (terminated()) return governing;
        matchValueStack_.emplace_back(governing, governingAddress);
        MatchValuePop pop{matchValueStack_};

        const std::vector<std::size_t> outerCleanups = activeCleanupRegions_;
        LLVMBasicBlockRef endBlock = appendBlock("match.end");
        std::vector<LLVMValueRef> incomingValues;
        std::vector<LLVMBasicBlockRef> incomingBlocks;
        if (node->booleanValue)
            return lowerSwitchMatchExpression(
                node, governingAddress, outerCleanups, endBlock,
                incomingValues, incomingBlocks, error);
        return lowerPatternMatchExpression(
            node, outerCleanups, endBlock, incomingValues, incomingBlocks, error);
    }

LLVMValueRef OopLlvmImplementation::lowerMatchGoverningValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef& governingAddress,
        std::string& error)
{
        LLVMTypeRef governingType = llvmType(node->children.front()->typeKey, error);
        if (governingType == nullptr) return nullptr;
        if (node->governingReference)
        {
            governingAddress = lowerExpression(node->children.front().get(), error);
            if (governingAddress == nullptr) return nullptr;
            return LLVMBuildLoad2(builder_, governingType, governingAddress, "match.value");
        }
        LLVMValueRef governing = lowerExpression(node->children.front().get(), error);
        if (governing == nullptr) return nullptr;
        governingAddress = LLVMBuildAlloca(builder_, governingType, "match.value.addr");
        LLVMBuildStore(builder_, governing, governingAddress);
        return governing;
}

bool OopLlvmImplementation::lowerMatchArmDeclarations(
        const frontend::ProjectOopMirNode* arm, std::string& error)
{
        for (std::size_t childIndex = 2; childIndex + 1 < arm->children.size(); ++childIndex)
        {
            const frontend::ProjectOopMirNode* declaration = arm->children[childIndex].get();
            if (!lowerStatement(declaration, error)) return false;
            if (declaration != nullptr && declaration->cleanupRegionOrdinal.has_value())
                activeCleanupRegions_.push_back(*declaration->cleanupRegionOrdinal);
            if (terminated()) break;
        }
        return true;
}

bool OopLlvmImplementation::emitMatchArmCleanups(
        const std::size_t cleanupDepth, std::string& error)
{
        while (activeCleanupRegions_.size() > cleanupDepth)
        {
            const std::size_t ordinal = activeCleanupRegions_.back();
            activeCleanupRegions_.pop_back();
            if (!emitCleanupAction(ordinal, error)) return false;
            if (terminated()) break;
        }
        return true;
}

bool OopLlvmImplementation::lowerSelectedMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error)
{
        activeCleanupRegions_ = outerCleanups;
        const std::size_t cleanupDepth = activeCleanupRegions_.size();
        if (!lowerMatchArmDeclarations(arm, error))
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (terminated())
        {
            activeCleanupRegions_ = outerCleanups;
            return true;
        }
        LLVMValueRef value = lowerExpression(arm->children.back().get(), error);
        if (value == nullptr)
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (!terminated() && !emitMatchArmCleanups(cleanupDepth, error))
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (!terminated())
        {
            LLVMBasicBlockRef exit = LLVMGetInsertBlock(builder_);
            LLVMBuildBr(builder_, endBlock);
            incomingValues.push_back(value);
            incomingBlocks.push_back(exit);
        }
        activeCleanupRegions_ = outerCleanups;
        return true;
}

LLVMValueRef OopLlvmImplementation::finishMatchResult(
        const frontend::ProjectOopMirNode* node,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error)
{
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (incomingValues.empty())
        {
            LLVMBuildUnreachable(builder_);
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }
        if (node->typeKey == "builtin:void")
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        if (incomingValues.size() == 1) return incomingValues.front();
        LLVMTypeRef resultType = node->referenceResult
            ? LLVMPointerTypeInContext(context_, 0)
            : llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "match.result");
        LLVMAddIncoming(
            phi, incomingValues.data(), incomingBlocks.data(),
            static_cast<unsigned>(incomingValues.size()));
        return phi;
}

LLVMValueRef OopLlvmImplementation::lowerSwitchMatchExpression(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef governingAddress,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error)
{
        LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef indices[] = {offset};
        LLVMValueRef tagAddress = LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), governingAddress,
            indices, 1, "match.tag.addr");
        LLVMTypeRef tagType = LLVMInt32TypeInContext(context_);
        LLVMValueRef tag = LLVMBuildLoad2(builder_, tagType, tagAddress, "match.tag");
        LLVMBasicBlockRef defaultBlock = appendBlock("match.invalid.tag");
        std::vector<LLVMBasicBlockRef> armBlocks;
        armBlocks.reserve(node->children.size() - 1U);
        unsigned caseCount = 0;
        for (std::size_t armIndex = 1; armIndex < node->children.size(); ++armIndex)
        {
            armBlocks.push_back(appendBlock("match.case"));
            const frontend::ProjectOopMirNode* arm = node->children[armIndex].get();
            if (arm != nullptr) caseCount += static_cast<unsigned>(arm->switchLabels.size());
        }
        LLVMValueRef dispatch = LLVMBuildSwitch(builder_, tag, defaultBlock, caseCount);
        for (std::size_t armIndex = 1; armIndex < node->children.size(); ++armIndex)
        {
            const frontend::ProjectOopMirNode* arm = node->children[armIndex].get();
            if (arm == nullptr || arm->kind != frontend::ProjectOopMirNodeKind::MatchArm ||
                arm->children.size() < 3 || arm->children.back() == nullptr ||
                arm->switchLabels.empty())
            {
                activeCleanupRegions_ = outerCleanups;
                return failValue(error, "T03-F03 switch match arm has no complete tag/value contract");
            }
            for (const std::int64_t caseTag : arm->switchLabels)
            {
                LLVMAddCase(dispatch,
                    LLVMConstInt(tagType, static_cast<std::uint64_t>(caseTag), 1),
                    armBlocks[armIndex - 1U]);
            }
        }
        for (std::size_t armIndex = 1; armIndex < node->children.size(); ++armIndex)
        {
            LLVMPositionBuilderAtEnd(builder_, armBlocks[armIndex - 1U]);
            if (!lowerSelectedMatchArm(
                    node->children[armIndex].get(), outerCleanups, endBlock,
                    incomingValues, incomingBlocks, error)) return nullptr;
        }
        LLVMPositionBuilderAtEnd(builder_, defaultBlock);
        LLVMBuildUnreachable(builder_);
        return finishMatchResult(node, endBlock, incomingValues, incomingBlocks, error);
}

bool OopLlvmImplementation::lowerGuardedMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef nextBlock,
        std::size_t& cleanupDepth,
        std::string& error)
{
        activeCleanupRegions_ = outerCleanups;
        cleanupDepth = activeCleanupRegions_.size();
        if (!lowerMatchArmDeclarations(arm, error))
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (terminated())
        {
            activeCleanupRegions_ = outerCleanups;
            LLVMPositionBuilderAtEnd(builder_, nextBlock);
            return true;
        }
        const std::vector<std::size_t> armCleanups(
            activeCleanupRegions_.begin() + static_cast<std::ptrdiff_t>(cleanupDepth),
            activeCleanupRegions_.end());
        LLVMValueRef guard = lowerExpression(arm->children[1].get(), error);
        if (guard == nullptr)
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (terminated())
        {
            activeCleanupRegions_ = outerCleanups;
            LLVMPositionBuilderAtEnd(builder_, nextBlock);
            return true;
        }
        LLVMBasicBlockRef valueBlock = appendBlock("match.value");
        LLVMBasicBlockRef guardFailedBlock = appendBlock("match.guard.failed");
        LLVMBuildCondBr(builder_, guard, valueBlock, guardFailedBlock);
        LLVMPositionBuilderAtEnd(builder_, guardFailedBlock);
        activeCleanupRegions_ = outerCleanups;
        activeCleanupRegions_.insert(
            activeCleanupRegions_.end(), armCleanups.begin(), armCleanups.end());
        if (!emitMatchArmCleanups(cleanupDepth, error))
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, nextBlock);
        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        activeCleanupRegions_ = outerCleanups;
        activeCleanupRegions_.insert(
            activeCleanupRegions_.end(), armCleanups.begin(), armCleanups.end());
        return true;
}

bool OopLlvmImplementation::lowerPatternMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error)
{
        if (arm == nullptr || arm->kind != frontend::ProjectOopMirNodeKind::MatchArm ||
            arm->children.size() < 3 || arm->children[0] == nullptr ||
            arm->children[1] == nullptr || arm->children.back() == nullptr)
        {
            activeCleanupRegions_ = outerCleanups;
            return fail(error, "T03-F03 match arm MIR is incomplete");
        }
        LLVMValueRef condition = lowerExpression(arm->children[0].get(), error);
        if (condition == nullptr)
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (terminated())
        {
            activeCleanupRegions_ = outerCleanups;
            return true;
        }
        LLVMBasicBlockRef bindingBlock = appendBlock("match.bind");
        LLVMBasicBlockRef nextBlock = appendBlock("match.next");
        LLVMBuildCondBr(builder_, condition, bindingBlock, nextBlock);
        LLVMPositionBuilderAtEnd(builder_, bindingBlock);
        std::size_t cleanupDepth = 0;
        if (!lowerGuardedMatchArm(arm, outerCleanups, nextBlock, cleanupDepth, error)) return false;
        if (LLVMGetInsertBlock(builder_) == nextBlock) return true;
        LLVMValueRef value = lowerExpression(arm->children.back().get(), error);
        if (value == nullptr)
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (!terminated() && !emitMatchArmCleanups(cleanupDepth, error))
        {
            activeCleanupRegions_ = outerCleanups;
            return false;
        }
        if (!terminated())
        {
            LLVMBasicBlockRef exit = LLVMGetInsertBlock(builder_);
            LLVMBuildBr(builder_, endBlock);
            incomingValues.push_back(value);
            incomingBlocks.push_back(exit);
        }
        activeCleanupRegions_ = outerCleanups;
        LLVMPositionBuilderAtEnd(builder_, nextBlock);
        return true;
}

LLVMValueRef OopLlvmImplementation::lowerPatternMatchExpression(
        const frontend::ProjectOopMirNode* node,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error)
{
        for (std::size_t armIndex = 1; armIndex < node->children.size(); ++armIndex)
        {
            if (!lowerPatternMatchArm(
                    node->children[armIndex].get(), outerCleanups, endBlock,
                    incomingValues, incomingBlocks, error)) return nullptr;
        }
        // Semantic exhaustiveness makes this edge impossible. Keeping it explicit makes malformed
        // or stale MIR fail closed without adding a runtime object or fallback value.
        if (!terminated()) LLVMBuildUnreachable(builder_);
        activeCleanupRegions_ = outerCleanups;
        return finishMatchResult(node, endBlock, incomingValues, incomingBlocks, error);
}

LLVMValueRef OopLlvmImplementation::lowerConditional(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 3)
            return failValue(error, "conditional expression MIR is incomplete");
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return nullptr;
        if (terminated()) return condition;
        LLVMBasicBlockRef trueBlock = appendBlock("cond.true");
        LLVMBasicBlockRef falseBlock = appendBlock("cond.false");
        LLVMBasicBlockRef endBlock = appendBlock("cond.end");
        LLVMBuildCondBr(builder_, condition, trueBlock, falseBlock);

        LLVMPositionBuilderAtEnd(builder_, trueBlock);
        LLVMValueRef whenTrue = lowerExpression(node->children[1].get(), error);
        if (whenTrue == nullptr) return nullptr;
        LLVMBasicBlockRef trueExit = LLVMGetInsertBlock(builder_);
        const bool trueReachesEnd = !terminated();
        if (trueReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, falseBlock);
        LLVMValueRef whenFalse = lowerExpression(node->children[2].get(), error);
        if (whenFalse == nullptr) return nullptr;
        LLVMBasicBlockRef falseExit = LLVMGetInsertBlock(builder_);
        const bool falseReachesEnd = !terminated();
        if (falseReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!trueReachesEnd && !falseReachesEnd)
        {
            LLVMBuildUnreachable(builder_);
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }
        if (trueReachesEnd && !falseReachesEnd) return whenTrue;
        if (!trueReachesEnd && falseReachesEnd) return whenFalse;
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "cond");
        LLVMValueRef values[] = {whenTrue, whenFalse};
        LLVMBasicBlockRef blocks[] = {trueExit, falseExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerNullCoalescing(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "null-coalescing MIR requires left and fallback values");
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        if (left == nullptr) return nullptr;
        if (terminated()) return left;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef hasValue = LLVMBuildICmp(
            builder_, LLVMIntNE, left, LLVMConstNull(pointerType), "coalesce.hasvalue");
        LLVMBasicBlockRef leftBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef fallbackBlock = appendBlock("coalesce.fallback");
        LLVMBasicBlockRef endBlock = appendBlock("coalesce.end");
        LLVMBuildCondBr(builder_, hasValue, endBlock, fallbackBlock);

        LLVMPositionBuilderAtEnd(builder_, fallbackBlock);
        LLVMValueRef fallback = lowerExpression(node->children[1].get(), error);
        if (fallback == nullptr) return nullptr;
        LLVMBasicBlockRef fallbackExit = LLVMGetInsertBlock(builder_);
        const bool fallbackReachesEnd = !terminated();
        if (fallbackReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!fallbackReachesEnd) return left;
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "coalesce");
        LLVMValueRef values[] = {left, fallback};
        LLVMBasicBlockRef blocks[] = {leftBlock, fallbackExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerOptionalCoalescing(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2 || node->elementTypeKey.empty() ||
            node->calleeName.empty() || node->secondaryCalleeName.empty())
        {
            return failValue(error, "Optional coalescing MIR requires carrier, fallback and protocol calls");
        }
        const auto hasValueTarget = functions_.find(node->calleeName);
        const auto uncheckedValueTarget = functions_.find(node->secondaryCalleeName);
        if (hasValueTarget == functions_.end() || uncheckedValueTarget == functions_.end())
        {
            return failValue(error, "Optional coalescing protocol target was not declared");
        }

        LLVMValueRef carrier = lowerExpression(node->children[0].get(), error);
        if (carrier == nullptr) return nullptr;
        if (terminated()) return carrier;
        LLVMTypeRef carrierType = llvmType(node->elementTypeKey, error);
        if (carrierType == nullptr) return nullptr;
        LLVMValueRef carrierSlot = entryBlockAlloca(
            carrierType, structAlignmentFor(node->elementTypeKey), "optional.carrier");
        LLVMBuildStore(builder_, carrier, carrierSlot);

        LLVMValueRef hasValueArguments[] = {carrierSlot};
        LLVMValueRef hasValue = buildPotentiallyThrowingCall(
            hasValueTarget->second.functionType, hasValueTarget->second.function,
            hasValueArguments, 1, "optional.hasvalue", error);
        if (hasValue == nullptr) return nullptr;

        LLVMBasicBlockRef valueBlock = appendBlock("optional.value");
        LLVMBasicBlockRef fallbackBlock = appendBlock("optional.fallback");
        LLVMBasicBlockRef endBlock = appendBlock("optional.end");
        LLVMBuildCondBr(builder_, hasValue, valueBlock, fallbackBlock);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef presentValue = lowerOptionalPresentValue(
            node, carrierSlot, uncheckedValueTarget->second, error);
        if (presentValue == nullptr) return nullptr;
        LLVMBasicBlockRef valueExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, fallbackBlock);
        LLVMValueRef fallback = lowerExpression(node->children[1].get(), error);
        if (fallback == nullptr) return nullptr;
        LLVMBasicBlockRef fallbackExit = LLVMGetInsertBlock(builder_);
        const bool fallbackReachesEnd = !terminated();
        if (fallbackReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!fallbackReachesEnd) return presentValue;
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "optional.coalesce");
        LLVMValueRef values[] = {presentValue, fallback};
        LLVMBasicBlockRef blocks[] = {valueExit, fallbackExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerOptionalPresentValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef carrierSlot,
        const FunctionInfo& uncheckedValueTarget,
        std::string& error)
{
        if (isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef valueType = llvmType(node->typeKey, error);
            if (valueType == nullptr) return nullptr;
            LLVMValueRef valueSlot = entryBlockAlloca(
                valueType, structAlignmentFor(node->typeKey), "optional.value.sret");
            LLVMValueRef arguments[] = {carrierSlot, valueSlot};
            LLVMValueRef call = buildPotentiallyThrowingCall(
                uncheckedValueTarget.functionType,
                uncheckedValueTarget.function,
                arguments, 2, "", error);
            if (call == nullptr) return nullptr;
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            return LLVMBuildLoad2(builder_, valueType, valueSlot, "optional.value.result");
        }

        LLVMValueRef arguments[] = {carrierSlot};
        return buildPotentiallyThrowingCall(
            uncheckedValueTarget.functionType,
            uncheckedValueTarget.function,
            arguments, 1, "optional.value.result", error);
    }

LLVMValueRef OopLlvmImplementation::lowerBlockExpression(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.empty())
        {
            return failValue(error, "block expression has no result");
        }
        for (std::size_t index = 0; index + 1U < node->children.size(); ++index)
        {
            if (node->children[index] != nullptr &&
                !lowerStatement(node->children[index].get(), error))
            {
                return nullptr;
            }
        }
        if (node->children.back() == nullptr)
        {
            return failValue(error, "block expression result is missing");
        }
        return lowerExpression(node->children.back().get(), error);
    }

LLVMValueRef OopLlvmImplementation::lowerResultPropagation(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef carrierOverride )
{
        if (node->children.size() != 1U || node->children.front() == nullptr ||
            node->resultSourceCarrierTypeKey.empty() ||
            node->resultSourceErrorTypeKey.empty() ||
            node->resultReturnCarrierTypeKey.empty() ||
            node->resultReturnErrorTypeKey.empty() ||
            node->resultSuccessTestCalleeName.empty() ||
            node->resultSuccessExtractorCalleeName.empty() ||
            node->resultErrorExtractorCalleeName.empty() ||
            node->resultErrorFactoryCalleeName.empty())
        {
            return failValue(error, "T07-F01 Result propagation MIR is incomplete");
        }

        LLVMValueRef carrier = carrierOverride != nullptr
            ? carrierOverride
            : lowerExpression(node->children.front().get(), error);
        if (carrier == nullptr) return nullptr;
        LLVMTypeRef carrierType = llvmType(node->resultSourceCarrierTypeKey, error);
        if (carrierType == nullptr) return nullptr;
        LLVMValueRef carrierSlot = entryBlockAlloca(
            carrierType,
            structAlignmentFor(node->resultSourceCarrierTypeKey),
            "result.carrier");
        LLVMBuildStore(builder_, carrier, carrierSlot);

        LLVMValueRef isSuccess = lowerResultPropagationInstanceCall(
            node, carrierSlot, node->resultSuccessTestCalleeName,
            "builtin:bool", "result.is_ok", error);
        if (isSuccess == nullptr) return nullptr;
        LLVMBasicBlockRef successBlock = appendBlock("result.ok");
        LLVMBasicBlockRef errorBlock = appendBlock("result.error");
        LLVMBuildCondBr(builder_, isSuccess, successBlock, errorBlock);

        LLVMPositionBuilderAtEnd(builder_, errorBlock);
        LLVMValueRef errorValue = lowerResultPropagationInstanceCall(
            node, carrierSlot, node->resultErrorExtractorCalleeName,
            node->resultSourceErrorTypeKey, "result.error.value", error);
        if (errorValue == nullptr) return nullptr;
        if (!node->resultErrorConversionCalleeName.empty())
        {
            errorValue = lowerResultPropagationStaticCall(
                node->resultErrorConversionCalleeName,
                node->resultSourceErrorTypeKey,
                errorValue,
                node->resultReturnErrorTypeKey,
                "result.error.converted", error);
            if (errorValue == nullptr) return nullptr;
        }
        LLVMValueRef returnCarrier = lowerResultPropagationStaticCall(
            node->resultErrorFactoryCalleeName,
            node->resultReturnErrorTypeKey,
            errorValue,
            node->resultReturnCarrierTypeKey,
            "result.error.carrier", error);
        if (returnCarrier == nullptr) return nullptr;

        frontend::ProjectOopMirNode returnNode;
        returnNode.kind = frontend::ProjectOopMirNodeKind::Return;
        returnNode.typeKey = "builtin:void";
        returnNode.cleanupRegionOrdinals = node->cleanupRegionOrdinals;
        returnNode.cleanupContinuationKind = node->cleanupContinuationKind;
        returnNode.targetScopeOrdinal = node->targetScopeOrdinal;
        if (!lowerStructuredExit(&returnNode, error, returnCarrier)) return nullptr;

        LLVMPositionBuilderAtEnd(builder_, successBlock);
        return lowerResultPropagationInstanceCall(
            node, carrierSlot, node->resultSuccessExtractorCalleeName,
            node->typeKey, "result.ok.value", error);
    }

LLVMValueRef OopLlvmImplementation::lowerResultPropagationInstanceCall(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef carrierSlot,
        std::string_view callee,
        std::string_view resultType,
        std::string_view stem,
        std::string& error)
{
        frontend::ProjectOopMirNode call;
        call.kind = frontend::ProjectOopMirNodeKind::DirectInstanceCall;
        call.typeKey = std::string(resultType);
        call.calleeName = std::string(callee);
        auto receiver = std::make_unique<frontend::ProjectOopMirNode>();
        receiver->typeKey = node->resultSourceCarrierTypeKey;
        call.children.push_back(std::move(receiver));
        LLVMValueRef value = lowerCall(&call, error, carrierSlot);
        if (value != nullptr && !stem.empty()) LLVMSetValueName2(
            value, stem.data(), stem.size());
        return value;
    }

LLVMValueRef OopLlvmImplementation::lowerResultPropagationStaticCall(
        std::string_view callee,
        std::string_view argumentType,
        LLVMValueRef argument,
        std::string_view resultType,
        std::string_view stem,
        std::string& error)
{
        frontend::ProjectOopMirNode call;
        call.kind = frontend::ProjectOopMirNodeKind::DirectStaticCall;
        call.typeKey = std::string(resultType);
        call.calleeName = std::string(callee);
        auto input = std::make_unique<frontend::ProjectOopMirNode>();
        input->typeKey = std::string(argumentType);
        call.children.push_back(std::move(input));
        const std::vector<LLVMValueRef> arguments{argument};
        LLVMValueRef value = lowerStaticCall(&call, error, &arguments);
        if (value != nullptr && !stem.empty()) LLVMSetValueName2(
            value, stem.data(), stem.size());
        return value;
    }

LLVMValueRef OopLlvmImplementation::lowerNullCoalescingStore(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "null-coalescing store MIR requires address and fallback");
        LLVMValueRef address = lowerExpression(node->children[0].get(), error);
        if (address == nullptr) return nullptr;
        if (terminated()) return address;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef current = LLVMBuildLoad2(builder_, pointerType, address, "coalesce.current");
        if (node->children[0]->volatileAccess) LLVMSetVolatile(current, 1);
        LLVMValueRef hasValue = LLVMBuildICmp(
            builder_, LLVMIntNE, current, LLVMConstNull(pointerType), "coalesce.store.hasvalue");
        LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef fallbackBlock = appendBlock("coalesce.store.fallback");
        LLVMBasicBlockRef endBlock = appendBlock("coalesce.store.end");
        LLVMBuildCondBr(builder_, hasValue, endBlock, fallbackBlock);

        LLVMPositionBuilderAtEnd(builder_, fallbackBlock);
        LLVMValueRef fallback = lowerExpression(node->children[1].get(), error);
        if (fallback == nullptr) return nullptr;
        const bool fallbackReachesEnd = !terminated();
        if (!fallbackReachesEnd)
        {
            LLVMPositionBuilderAtEnd(builder_, endBlock);
            return current;
        }
        LLVMValueRef store = LLVMBuildStore(builder_, fallback, address);
        if (node->children[0]->volatileAccess) LLVMSetVolatile(store, 1);
        LLVMBasicBlockRef fallbackExit = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, pointerType, "coalesce.store");
        LLVMValueRef values[] = {current, fallback};
        LLVMBasicBlockRef blocks[] = {currentBlock, fallbackExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerNullConditionalField(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride )
{
        if (node->children.size() != 1)
            return failValue(error, "null-conditional field MIR requires one receiver");
        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride
            : lowerExpression(node->children[0].get(), error);
        if (receiver == nullptr) return nullptr;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcond.hasreceiver");
        LLVMBasicBlockRef nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef valueBlock = appendBlock("nullcond.value");
        LLVMBasicBlockRef endBlock = appendBlock("nullcond.end");
        LLVMBuildCondBr(builder_, hasReceiver, valueBlock, endBlock);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef offset = LLVMConstInt(
            LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef indices[] = {offset};
        LLVMValueRef address = LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1,
            "nullcond.field.addr");
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return nullptr;
        LLVMValueRef value = LLVMBuildLoad2(builder_, resultType, address, "nullcond.field");
        if (node->volatileAccess) LLVMSetVolatile(value, 1);
        LLVMBasicBlockRef valueExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcond");
        LLVMValueRef values[] = {LLVMConstNull(resultType), value};
        LLVMBasicBlockRef blocks[] = {nullBlock, valueExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

bool OopLlvmImplementation::prepareNullConditionalFieldStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMValueRef& address,
        LLVMValueRef& current,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock)
{
        if (node->children.size() != 2)
        {
            error = "null-conditional field store MIR requires receiver and value";
            return false;
        }
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return false;
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcond.hasreceiver");
        nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef valueBlock = appendBlock("nullcond.store");
        endBlock = appendBlock("nullcond.store.end");
        LLVMBuildCondBr(builder_, hasReceiver, valueBlock, endBlock);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef offset = LLVMConstInt(
            LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef indices[] = {offset};
        address = LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1,
            "nullcond.store.addr");
        current = nullptr;
        if (node->operation != lexer::TokenKind::Equal)
        {
            current = LLVMBuildLoad2(
                builder_, resultType, address, "nullcond.compound.load");
            if (node->volatileAccess) LLVMSetVolatile(current, 1);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::concatenateNullConditionalFieldValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef current,
        LLVMValueRef value,
        std::string& error)
{
        LLVMValueRef leftLength = nullableStringLength(current, "nullcond.concat.left");
        LLVMValueRef rightLength = nullableStringLength(value, "nullcond.concat.right");
        LLVMValueRef total = LLVMBuildAdd(
            builder_, leftLength, rightLength, "nullcond.concat.length");
        LLVMValueRef result = allocateNativeString(total, "nullcond.concat", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef resultData = byteOffset(result, 8, "nullcond.concat.result.data");
        LLVMValueRef leftData = byteOffset(current, 8, "nullcond.concat.left.data");
        if (!emitMemcpyBytes(resultData, leftData, leftLength, "nullcond.concat.copy.left", error))
            return nullptr;
        LLVMValueRef rightDestination = dynamicByteOffset(
            resultData, leftLength, "nullcond.concat.right.dest");
        LLVMValueRef rightData = byteOffset(value, 8, "nullcond.concat.right.data");
        if (!emitMemcpyBytes(rightDestination, rightData, rightLength, "nullcond.concat.copy.right", error))
            return nullptr;
        if (node->booleanValue &&
            !emitOwnedPointerDestroy(current, node->typeKey, "nullcond.concat.previous", error))
            return nullptr;
        return result;
    }

LLVMValueRef OopLlvmImplementation::computeNullConditionalFieldValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef current,
        LLVMValueRef value,
        std::string& error)
{
        if (node->operation == lexer::TokenKind::Equal) return value;
        if (node->typeKey == "builtin:string")
            return concatenateNullConditionalFieldValue(node, current, value, error);
        const lexer::TokenKind base = lexer::compoundAssignmentBaseOperator(node->operation);
        return applyBinaryOperation(base, current, value, node->typeKey, error, false);
    }

LLVMValueRef OopLlvmImplementation::finishNullConditionalFieldStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef current,
        LLVMValueRef value,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error)
{
        if (terminated())
        {
            LLVMPositionBuilderAtEnd(builder_, endBlock);
            return LLVMConstNull(resultType);
        }
        // NS304-T10-F06: compute and ownership remain inside the non-null branch.
        LLVMValueRef storedValue = computeNullConditionalFieldValue(node, current, value, error);
        if (storedValue == nullptr) return nullptr;
        LLVMValueRef store = LLVMBuildStore(builder_, storedValue, address);
        if (node->volatileAccess) LLVMSetVolatile(store, 1);
        LLVMBasicBlockRef valueExit = LLVMGetInsertBlock(builder_);
        const bool valueReachesEnd = !terminated();
        if (valueReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!valueReachesEnd) return LLVMConstNull(resultType);
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcond.store.result");
        LLVMValueRef values[] = {LLVMConstNull(resultType), storedValue};
        LLVMBasicBlockRef blocks[] = {nullBlock, valueExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

bool OopLlvmImplementation::prepareNullConditionalArrayLoad(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock)
{
        if (node->children.size() != 2)
        {
            error = "null-conditional array load MIR requires receiver and index (got " +
                std::to_string(node->children.size()) + ")";
            return false;
        }
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return false;
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcond.hasreceiver");
        nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef valueBlock = appendBlock("nullcond.index");
        endBlock = appendBlock("nullcond.index.end");
        LLVMBuildCondBr(builder_, hasReceiver, valueBlock, endBlock);
        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::nullConditionalArrayLoadAddress(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        std::string& error)
{
        LLVMValueRef length = LLVMBuildLoad2(
            builder_, LLVMInt64TypeInContext(context_), receiver, "nullcond.index.length");
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, length, "nullcond.index.inrange");
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(
                builder_, LLVMIntSGE, index, zero, "nullcond.index.nonnegative");
            inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "nullcond.index.valid");
        }
        if (!emitGuard(inRange, LlvmTrapReason::BoundsViolation, "nullcond.index", error))
            return nullptr;
        const std::uint64_t dataOffset =
            (8U + node->alignment - 1U) / node->alignment * node->alignment;
        LLVMValueRef scaled = LLVMBuildMul(
            builder_, index, LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            "nullcond.index.scaled");
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, scaled, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0),
            "nullcond.index.offset");
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1,
            "nullcond.index.addr");
    }

LLVMValueRef OopLlvmImplementation::finishNullConditionalArrayLoad(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error)
{
        if (terminated())
        {
            LLVMPositionBuilderAtEnd(builder_, endBlock);
            return LLVMConstNull(resultType);
        }
        LLVMValueRef address = nullConditionalArrayLoadAddress(node, receiver, index, error);
        if (address == nullptr) return nullptr;
        LLVMValueRef element = LLVMBuildLoad2(
            builder_, resultType, address, "nullcond.index.element");
        LLVMBasicBlockRef valueExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcond.index.phi");
        LLVMValueRef values[] = {LLVMConstNull(resultType), element};
        LLVMBasicBlockRef blocks[] = {nullBlock, valueExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

bool OopLlvmImplementation::prepareNullConditionalArrayStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock)
{
        if (node->children.size() != 3)
        {
            error = "null-conditional array store MIR requires receiver, index, value";
            return false;
        }
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        resultType = llvmType(node->typeKey, error);
        if (resultType == nullptr) return false;
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcond.hasreceiver");
        nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef valueBlock = appendBlock("nullcond.index.store");
        endBlock = appendBlock("nullcond.index.store.end");
        LLVMBuildCondBr(builder_, hasReceiver, valueBlock, endBlock);
        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::nullConditionalArrayStoreAddress(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        std::string& error)
{
        LLVMValueRef length = LLVMBuildLoad2(
            builder_, LLVMInt64TypeInContext(context_), receiver, "nullcond.index.length");
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, length, "nullcond.index.inrange");
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef nonNegative = LLVMBuildICmp(
                builder_, LLVMIntSGE, index, zero, "nullcond.index.nonnegative");
            inRange = LLVMBuildAnd(builder_, nonNegative, inRange, "nullcond.index.valid");
        }
        if (!emitGuard(inRange, LlvmTrapReason::BoundsViolation, "nullcond.index", error))
            return nullptr;
        const std::uint64_t dataOffset =
            (8U + node->alignment - 1U) / node->alignment * node->alignment;
        LLVMValueRef scaled = LLVMBuildMul(
            builder_, index, LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            "nullcond.index.scaled");
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, scaled, LLVMConstInt(LLVMInt64TypeInContext(context_), dataOffset, 0),
            "nullcond.index.offset");
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), receiver, indices, 1,
            "nullcond.index.addr");
    }

LLVMValueRef OopLlvmImplementation::finishNullConditionalArrayStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        LLVMValueRef value,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error)
{
        if (terminated())
        {
            LLVMPositionBuilderAtEnd(builder_, endBlock);
            return LLVMConstNull(resultType);
        }
        LLVMValueRef address = nullConditionalArrayStoreAddress(node, receiver, index, error);
        if (address == nullptr) return nullptr;
        LLVMBuildStore(builder_, value, address);
        LLVMBasicBlockRef valueExit = LLVMGetInsertBlock(builder_);
        const bool valueReachesEnd = !terminated();
        if (valueReachesEnd) LLVMBuildBr(builder_, endBlock);

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!valueReachesEnd) return LLVMConstNull(resultType);
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcond.index.store.result");
        LLVMValueRef values[] = {LLVMConstNull(resultType), value};
        LLVMBasicBlockRef blocks[] = {nullBlock, valueExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

}
