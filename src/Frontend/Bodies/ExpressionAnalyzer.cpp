#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <ranges>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using lexer::TokenKind;

namespace
{
bool isLogical(TokenKind kind) noexcept
{
    return kind == TokenKind::AmpAmp || kind == TokenKind::PipePipe;
}
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    // The O0 Windows host gives this dispatcher a large native stack frame. Deep but valid value
    // expressions used to multiply that frame through recursive call/binary/construction descent.
    // The worklist below evaluates those children in the same left-to-right postorder on heap
    // storage, then lets the existing semantic core consume the cached results exactly once.
    const ExpressionAnalysisRequest request{
        expression,
        allowMethodGroup,
        allowEvent,
        allowMove,
        allowConstructorPropertyWrite};
    if (iterativeExpressionAnalysis_ != nullptr)
    {
        if (const ExpressionResult* cached = cachedExpressionResult(request); cached != nullptr)
        {
            return *cached;
        }
        return analyzeExpressionCore(
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            diagnostics,
            allowMethodGroup,
            allowEvent,
            allowMove,
            allowConstructorPropertyWrite);
    }
    if (shouldAnalyzeExpressionIteratively(request))
    {
        return analyzeExpressionIteratively(
            request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    return analyzeExpressionCore(
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        diagnostics,
        allowMethodGroup,
        allowEvent,
        allowMove,
        allowConstructorPropertyWrite);
}

const ProjectMethodBodyModel::ExpressionResult* ProjectMethodBodyModel::cachedExpressionResult(
    const ExpressionAnalysisRequest& request) const noexcept
{
    if (iterativeExpressionAnalysis_ == nullptr)
    {
        return nullptr;
    }
    const auto found = iterativeExpressionAnalysis_->resultIndex.find(
        ExpressionAnalysisCacheKey{request.syntax, request.optionMask()});
    return found == iterativeExpressionAnalysis_->resultIndex.end() ||
            found->second >= iterativeExpressionAnalysis_->results.size()
        ? nullptr
        : &iterativeExpressionAnalysis_->results[found->second].result;
}

void ProjectMethodBodyModel::appendIterativeExpressionChildren(
    const ExpressionAnalysisRequest& request,
    std::vector<ExpressionAnalysisRequest>& children) const
{
    const ast::AstNode* expression = request.syntax;
    if (expression == nullptr)
    {
        return;
    }
    if (expression->kind() == ast::AstNodeKind::BinaryExpression &&
        !isLogical(expression->tokenKind()))
    {
        children.push_back(ExpressionAnalysisRequest{expression->first()});
        children.push_back(ExpressionAnalysisRequest{expression->second()});
        return;
    }
    if (expression->kind() == ast::AstNodeKind::ObjectCreationExpression)
    {
        for (const ast::AstNode* argument : expression->children())
        {
            children.push_back(ExpressionAnalysisRequest{
                argument,
                false,
                false,
                true,
                false});
        }
        return;
    }
    if (expression->kind() != ast::AstNodeKind::CallExpression ||
        !expression->auxiliaryText().empty() ||
        expression->first() == nullptr ||
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression ||
        (namespaceFunctionModel_ != nullptr &&
         namespaceFunctionModel_->referenceBinding(expression) != nullptr) ||
        std::ranges::any_of(
            expression->children(),
            [](const ast::AstNode* argument)
            {
                return argument != nullptr &&
                    argument->kind() == ast::AstNodeKind::LambdaExpression;
            }))
    {
        return;
    }

    children.push_back(ExpressionAnalysisRequest{
        expression->first(),
        true,
        true,
        false,
        false});
    for (const ast::AstNode* argument : expression->children())
    {
        children.push_back(ExpressionAnalysisRequest{
            argument,
            true,
            false,
            true,
            false});
    }
}

bool ProjectMethodBodyModel::shouldAnalyzeExpressionIteratively(
    const ExpressionAnalysisRequest& request) const
{
    constexpr std::size_t minimumWorklistDepth = 6;
    struct Pending final
    {
        ExpressionAnalysisRequest request;
        std::size_t depth = 0;
    };

    std::vector<Pending> pending;
    pending.push_back(Pending{request, 1});
    while (!pending.empty())
    {
        Pending current = pending.back();
        pending.pop_back();
        if (current.depth >= minimumWorklistDepth)
        {
            return true;
        }

        std::vector<ExpressionAnalysisRequest> children;
        appendIterativeExpressionChildren(current.request, children);
        for (auto child = children.rbegin(); child != children.rend(); ++child)
        {
            pending.push_back(Pending{*child, current.depth + 1});
        }
    }
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionIteratively(
    const ExpressionAnalysisRequest& request,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    IterativeExpressionAnalysis analysis;
    analysis.results.reserve(64);
    analysis.resultIndex.reserve(64);
    std::vector<ExpressionAnalysisFrame> work;
    work.push_back(ExpressionAnalysisFrame{request, false});
    iterativeExpressionAnalysis_ = &analysis;

    while (!work.empty())
    {
        ExpressionAnalysisFrame frame = work.back();
        work.pop_back();
        if (cachedExpressionResult(frame.request) != nullptr)
        {
            continue;
        }
        if (!frame.expanded)
        {
            frame.expanded = true;
            work.push_back(frame);

            std::vector<ExpressionAnalysisRequest> children;
            appendIterativeExpressionChildren(frame.request, children);
            for (auto child = children.rbegin(); child != children.rend(); ++child)
            {
                if (cachedExpressionResult(*child) == nullptr)
                {
                    work.push_back(ExpressionAnalysisFrame{*child, false});
                }
            }
            continue;
        }

        ExpressionResult result = analyzeExpressionCore(
            frame.request.syntax,
            methodMemberOrdinal,
            scopeOrdinal,
            diagnostics,
            frame.request.allowMethodGroup,
            frame.request.allowEvent,
            frame.request.allowMove,
            frame.request.allowConstructorPropertyWrite);
        const std::size_t resultIndex = analysis.results.size();
        analysis.results.push_back(ExpressionAnalysisCacheEntry{frame.request, std::move(result)});
        analysis.resultIndex.emplace(
            ExpressionAnalysisCacheKey{
                frame.request.syntax,
                frame.request.optionMask()},
            resultIndex);
    }

    const ExpressionResult* result = cachedExpressionResult(request);
    ExpressionResult completed = result != nullptr ? *result : ExpressionResult{};
    iterativeExpressionAnalysis_ = nullptr;
    return completed;
}
}
