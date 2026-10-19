#pragma once

#include "nativesharp/Parser/Parser.h"

namespace nativesharp::parser
{
class Parser::PatternBindingEmitter final
{
public:
    PatternBindingEmitter(
        Parser& parser,
        const Pattern& root,
        PatternExpressionFactory source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);

    void run();

private:
    enum class SourceKind
    {
        Root,
        Identifier,
        MatchValue,
        Member,
        FixedIndex,
        FromEndIndex,
        RestSlice
    };

    struct SourcePlan final
    {
        std::size_t parent = 0;
        SourceKind kind = SourceKind::Root;
        SourceRange range;
        SourceRange containerRange;
        std::string text;
        std::size_t value = 0;
        std::size_t extent = 0;
    };

    enum class SourceWorkKind
    {
        Build,
        FinishMember,
        FinishFixedIndex,
        FinishFromEndFirst,
        FinishFromEndSecond,
        FinishRestFirst,
        FinishRestSecond
    };

    struct SourceWork final
    {
        SourceWorkKind kind = SourceWorkKind::Build;
        std::size_t sourcePlan = 0;
        ast::AstNode* first = nullptr;
    };

    enum class WorkKind
    {
        Emit,
        ContinuePositional,
        FinishPositionalChild,
        ContinueOrBinding,
        ContinueOrAlternative,
        FinishOrAlternative,
        FinishOrBinding
    };

    struct Work final
    {
        WorkKind kind = WorkKind::Emit;
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        ast::AstNode* destination = nullptr;
        std::size_t state = 0;
    };

    struct PositionalState final
    {
        const Pattern* pattern = nullptr;
        ast::AstNode* destination = nullptr;
        std::vector<std::string> names;
        std::size_t nextChild = 0;
        ast::AstNode* condition = nullptr;
    };

    struct OrState final
    {
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        ast::AstNode* destination = nullptr;
        std::vector<Pattern> alternatives;
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::size_t nextBinding = 0;
        std::size_t nextAlternative = 0;
        ast::AstNode* selector = nullptr;
        ast::AstNode* arm = nullptr;
    };

    void schedule(Work work);
    void scheduleEmit(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void dispatch(Work work);
    std::size_t addSource(
        std::size_t parent,
        SourceKind kind,
        SourceRange range,
        SourceRange containerRange = {},
        std::string text = {},
        std::size_t value = 0,
        std::size_t extent = 0);
    ast::AstNode* evaluateSource(std::size_t sourcePlan);
    void dispatchSourceWork(
        SourceWork work,
        std::vector<SourceWork>& pending,
        std::vector<ast::AstNode*>& values);
    void finishFromEndSource(
        const SourceWork& work,
        std::vector<ast::AstNode*>& values);
    void finishRestSource(
        const SourceWork& work,
        std::vector<ast::AstNode*>& values);
    PatternExpressionFactory makeSourceFactory(std::size_t sourcePlan);
    void emit(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* destination);
    void emitSingle(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void emitTuple(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void startPositional(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void continuePositional(std::size_t state);
    void finishPositionalChild(std::size_t state, std::size_t sourcePlan);
    void emitStruct(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void emitList(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void emitVariant(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void startOr(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* destination);
    void continueOrBinding(std::size_t state);
    void continueOrAlternative(std::size_t state);
    void finishOrAlternative(std::size_t state);
    void finishOrBinding(std::size_t state);

    Parser& parser_;
    const Pattern& root_;
    PatternExpressionFactory rootSource_;
    ast::AstNode* destination_ = nullptr;
    std::string temporaryPrefix_;
    std::vector<SourcePlan> sources_;
    std::vector<Work> work_;
    std::deque<PositionalState> positionalStates_;
    std::deque<OrState> orStates_;
};
}
