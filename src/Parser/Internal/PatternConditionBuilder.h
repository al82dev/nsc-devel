#pragma once

#include "nativesharp/Parser/Parser.h"

namespace nativesharp::parser
{
class Parser::PatternConditionBuilder final
{
public:
    PatternConditionBuilder(
        Parser& parser,
        const Pattern& root,
        PatternExpressionFactory source);

    ast::AstNode* run();

private:
    enum class SourceStepKind
    {
        Root,
        Member,
        VariantPayload,
        Index
    };

    struct SourcePlan final
    {
        std::size_t parent = 0;
        SourceStepKind kind = SourceStepKind::Root;
        std::string text;
        ast::AstNode* index = nullptr;
    };

    enum class WorkKind
    {
        Build,
        FinishNot,
        FinishVariant,
        FinishLogical,
        ContinueStruct,
        FinishStruct,
        ContinueList,
        FinishList
    };

    struct Work final
    {
        WorkKind kind = WorkKind::Build;
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        std::size_t nextChild = 0;
        ast::AstNode* result = nullptr;
        TokenKind operation = TokenKind::AmpAmp;
    };

    void schedule(Work work);
    void scheduleBuild(const Pattern& pattern, std::size_t sourcePlan);
    void dispatch(Work work);
    ast::AstNode* takeValue();
    std::size_t addSourcePlan(
        std::size_t parent,
        SourceStepKind kind,
        std::string text = {},
        ast::AstNode* index = nullptr);
    ast::AstNode* evaluateSource(std::size_t sourcePlan);
    ast::AstNode* buildLeaf(const Pattern& pattern, std::size_t sourcePlan);
    void build(const Pattern& pattern, std::size_t sourcePlan);
    void startNot(const Pattern& pattern, std::size_t sourcePlan);
    void finishNot(Work work);
    void startVariant(const Pattern& pattern, std::size_t sourcePlan);
    void finishVariant(Work work);
    void startLogical(
        const Pattern& pattern,
        std::size_t sourcePlan,
        TokenKind operation);
    void finishLogical(Work work);
    void startStruct(const Pattern& pattern, std::size_t sourcePlan);
    void continueStruct(Work work);
    void finishStruct(Work work);
    void startList(const Pattern& pattern, std::size_t sourcePlan);
    void continueList(Work work);
    void finishList(Work work);

    Parser& parser_;
    const Pattern& root_;
    PatternExpressionFactory rootSource_;
    std::vector<SourcePlan> sources_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
};
}
