#pragma once

#include "LoweringFrame.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Sema/SemanticModel.h"
#include "nativesharp/Sema/TypeContext.h"

#include <string>
#include <vector>

namespace nativesharp::hir::internal
{
/// Executes the base AST-to-HIR lowering with heap-backed continuation frames.
class IterativeHirLowering final
{
public:
    IterativeHirLowering(
        HirArena& arena,
        sema::TypeContext& types,
        const sema::SemanticModel& semanticModel,
        std::string& error);

    HirNode* lowerGlobal(ast::AstNode* node);

    HirNode* lowerStruct(ast::AstNode* node);

    HirNode* lowerEnum(ast::AstNode* node);

    HirNode* lowerFunction(ast::AstNode* node);

    HirNode* lowerGlobalVariable(ast::AstNode* node);

    HirNode* lowerBlock(ast::AstNode* node);

    HirNode* lowerStatement(ast::AstNode* node);

    HirNode* lowerLocalVariable(ast::AstNode* node);

    HirNode* lowerExpression(ast::AstNode* node, bool loadLValue);

    HirNode* lowerAddress(ast::AstNode* node);

    HirNode* lowerConvertedOperand(
        const ast::AstNode* owner,
        ast::AstNode* operand,
        int operandIndex);

private:
    HirNode* run(LoweringFrame initial);

    void finishFrame(
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted,
        HirNode* result);

    HirNode* takeCompletedFrame(
        HirNode*& completed,
        bool& hasCompleted);

    void descendFrame(
        std::vector<LoweringFrame>& frames,
        bool& hasCompleted,
        LoweringFrame child);

    bool dispatchGlobalFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchStructFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchEnumFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchFunctionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchGlobalVariableFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchBlockFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchLocalVariableFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool dispatchConvertedOperandFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);


    bool lowerStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool continueConditionalStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool continueLoopStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool continueForStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishValueStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool lowerExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginBasicExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginCallOrMemberExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginAllocationExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishLoadOrUnaryFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishBinaryFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishConditionalFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishAssignmentFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishCallMemberOrAllocationFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool lowerAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool beginAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishDereferenceAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishIndexAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    bool finishMemberAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted);

    const sema::ExpressionInfo* expressionInfo(const ast::AstNode* node);

    sema::Symbol* boundSymbol(const ast::AstNode* node);

    const sema::ResolvedOperatorInfo* operatorInfo(const ast::AstNode* node);

    const sema::ConversionInfo* operandConversion(
        const ast::AstNode* owner,
        const ast::AstNode* operand) const noexcept;

    const sema::ConversionInfo* resultConversion(const ast::AstNode* owner) const noexcept;

    HirNode* wrapConversion(
        SourceRange range,
        HirNode* value,
        const sema::ConversionInfo& conversion);

    HirNode* fail(std::string message);

    HirArena& arena_;
    sema::TypeContext& types_;
    const sema::SemanticModel& semanticModel_;
    std::string& error_;
    std::vector<LoweringFrame> frames_;
};
}
