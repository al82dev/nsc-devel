#pragma once

#include "nativesharp/Parser/Parser.h"

namespace nativesharp::parser
{
class Parser::PatternLoweringMachine final
{
public:
    PatternLoweringMachine(
        Parser& parser,
        const Pattern& root,
        PatternExpressionFactory source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);

    ast::AstNode* run();

private:
    enum class SourceKind
    {
        Root,
        Identifier,
        Member
    };

    struct SourcePlan final
    {
        std::size_t parent = 0;
        SourceKind kind = SourceKind::Root;
        SourceRange range;
        std::string text;
    };

    enum class WorkKind
    {
        Build,
        ContinueOr,
        FinishOrChild,
        ContinueSequential,
        FinishSequential,
        FinishVariant
    };

    struct Work final
    {
        WorkKind kind = WorkKind::Build;
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        std::size_t nextChild = 0;
        ast::AstNode* continuation = nullptr;
        ast::AstNode* result = nullptr;
        ast::AstNode* block = nullptr;
        ast::AstNode* decision = nullptr;
        std::string selectedName;
        std::vector<std::string> names;
    };

    struct ClassificationFrame final
    {
        const Pattern* pattern = nullptr;
        std::size_t nextChild = 0;
        bool conditionOnly = true;
    };

    void schedule(Work work);
    void scheduleBuild(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void dispatch(Work work);
    ast::AstNode* takeValue();
    std::size_t addSourcePlan(
        std::size_t parent,
        SourceKind kind,
        SourceRange range,
        std::string text);
    ast::AstNode* evaluateSource(std::size_t sourcePlan);
    PatternExpressionFactory makeSourceFactory(std::size_t sourcePlan);
    bool isConditionOnly(const Pattern& pattern);
    void finishClassification(
        std::vector<ClassificationFrame>& frames,
        bool value);
    void build(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void buildBinding(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void startOr(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void continueOr(Work work);
    void finishOrChild(Work work);
    void startSequential(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void initializeDeconstruction(Work& work);
    void continueSequential(Work work);
    void finishSequential(Work work);
    void buildList(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void startVariant(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void finishVariant(Work work);
    void buildConditional(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);

    Parser& parser_;
    const Pattern& root_;
    PatternExpressionFactory rootSource_;
    ast::AstNode* continuation_ = nullptr;
    std::string temporaryPrefix_;
    bool emitBindings_ = true;
    std::vector<SourcePlan> sources_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
    std::unordered_map<const Pattern*, bool> conditionOnly_;
};
}
