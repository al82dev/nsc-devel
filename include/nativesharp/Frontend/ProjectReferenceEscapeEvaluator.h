#pragma once

#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/ReferenceEscapeEvaluationFrame.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
class ProjectMethodBodyModel;
struct ClassMemberSymbol;
struct MethodCallBinding;

class ProjectReferenceEscapeEvaluator final
{
public:
    ProjectReferenceEscapeEvaluator(
        const ProjectMethodBodyModel& model,
        std::size_t scopeOrdinal) noexcept;

    [[nodiscard]] MethodEscapeOrigin evaluate(const ast::AstNode* expression) const;

private:
    void startNode(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        MethodEscapeOrigin& result,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    void startIdentifier(const ast::AstNode* expression, MethodEscapeOrigin& result) const;
    void startMatch(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        MethodEscapeOrigin& result,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    void startMember(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        MethodEscapeOrigin& result) const;
    void startIndex(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        MethodEscapeOrigin& result) const;
    void startCall(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    void startDereference(
        const ast::AstNode* expression,
        const ast::AstNode*& next,
        MethodEscapeOrigin& result,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    void resume(
        const ast::AstNode*& next,
        MethodEscapeOrigin& result,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    bool scheduleNextMatchArm(
        ReferenceEscapeEvaluationFrame& frame,
        const ast::AstNode*& next) const;
    bool scheduleNextCallArgument(
        ReferenceEscapeEvaluationFrame& frame,
        const ast::AstNode*& next) const;
    void pushContinuation(
        ReferenceEscapeEvaluationFrame frame,
        std::vector<ReferenceEscapeEvaluationFrame>& continuations) const;
    [[nodiscard]] std::string_view typeKeyOf(const ast::AstNode* syntax) const;
    [[nodiscard]] const MethodCallBinding* resolvedCall(const ast::AstNode* syntax) const;
    [[nodiscard]] const ClassMemberSymbol* resolvedCallee(const ast::AstNode* syntax) const;
    [[nodiscard]] std::size_t rootScopeOrdinal() const noexcept;

    const ProjectMethodBodyModel& model_;
    std::size_t scopeOrdinal_ = 0;
    std::size_t callableOrdinal_ = 0;
};
}
