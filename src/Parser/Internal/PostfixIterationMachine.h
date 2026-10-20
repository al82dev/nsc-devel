#pragma once

#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::parser
{
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
namespace scheduler_test_hooks
{
extern thread_local std::size_t activeDepth;
extern thread_local std::size_t maximumDepth;
extern thread_local std::size_t unfinishedResults;
}
#endif

class Parser::PostfixIterationMachine final : public Parser::ExpressionContinuation
{
public:
    explicit PostfixIterationMachine(
        Parser& parser,
        bool resumable = false,
        ContinuationRequestKind rootKind = ContinuationRequestKind::Expression) noexcept;
    ast::AstNode* parse();
    ast::AstNode* parseUnary();
    ast::AstNode* parseBinary();
    ExpressionContinuationStatus advance(ast::AstNode* completedBlock) override;
    ast::AstNode* result() const noexcept override;

private:
    static constexpr std::size_t noSlot = std::numeric_limits<std::size_t>::max();
    enum class WorkKind : std::uint8_t {
        Expression, ExpressionTail, Unary, FinishUnary, FinishBinary,
        FinishAssignment, FinishConditionalTrue, FinishConditionalFalse,
        FinishTypeRelation, FinishRangeEnd, FinishRangeStep,
        Postfix, Primary, Loop, Comprehension, FinishComprehensionBuilder,
        Grouping, Collection, FinishNameOf,
        FinishOverflowPolicy, ContinueInterpolation, Call, Index, With,
        NewArguments, PlacementNew, ArrayNew, ArrayInitializer
    };
    ast::AstNode* parseRoot(WorkKind rootKind, std::uint8_t precedence);
    void initializeRoot(WorkKind rootKind, std::uint8_t precedence);
    struct Result final { ast::AstNode* value = nullptr; bool ready = false; };
    struct Work final {
        Work() = default;
        Work(WorkKind workKind, std::size_t result, std::size_t child = noSlot,
            ast::AstNode* current = nullptr, ast::AstNode* prior = nullptr) noexcept
            : kind(workKind), resultSlot(result), childSlot(child), node(current), first(prior)
        {
        }

        WorkKind kind = WorkKind::Postfix;
        std::size_t resultSlot = 0;
        std::size_t childSlot = noSlot;
        ast::AstNode* node = nullptr;
        ast::AstNode* first = nullptr;
        SourceRange range;
        std::string text;
        bool flag = false;
        std::uint8_t precedence = 0;
        lexer::TokenKind tokenKind = lexer::TokenKind::Invalid;
        std::string auxiliaryText;
        std::vector<ast::AstNode*> children;
        std::size_t frameIndex = noSlot;
        SourceRange outerRange;
        ast::TypeSyntax* typeSyntax = nullptr;
        std::vector<SourceRange> ranges;
        std::size_t ordinal = 0;
        std::size_t beginIndex = 0;
        std::string label;
    };
    void execute(Work work);
    struct InterpolationFrame final {
        std::optional<lexer::Token> token;
        std::vector<language::InterpolationPart> parts;
        std::size_t partIndex = 0;
        ast::AstNode* staticNode = nullptr;
        ast::AstNode* runtimeResult = nullptr;
        bool runtime = false;
        std::unique_ptr<source::SourceFile> fragmentSource;
        std::vector<lexer::Token> fragmentTokens;
        std::span<const lexer::Token> outerTokens;
        std::size_t outerIndex = 0;
        std::vector<std::uint8_t> outerQuestionIndex;
        std::vector<std::size_t> outerComprehensionDelimiterIndex;
        std::size_t outerPendingGenericGreater = 0;
    };

    std::size_t makeSlot();
    void complete(std::size_t slot, ast::AstNode* value);
    void scheduleChild(std::size_t slot);
    void startExpression(Work work);
    void continueExpression(Work work);
    void startUnary(Work work);
    void finishUnary(Work work);
    void finishBinary(Work work);
    void finishAssignment(Work work);
    void finishConditionalTrue(Work work);
    void finishConditionalFalse(Work work);
    void finishTypeRelation(Work work);
    void finishRangeEnd(Work work);
    void finishRangeStep(Work work);
    void validateConstantRange(ast::AstNode* range) const;
    void startPostfix(Work work);
    void continuePostfix(Work work, ast::AstNode* expression);
    void startPrimary(Work work);
    void startLoop(Work work);
    void finishLoopBody(Work work, ast::AstNode* body);
    void startComprehension(Work work);
    void finishComprehensionBuilder(Work work);
    void finishComprehensionBody(Work work, ast::AstNode* body);
    void continueGrouping(Work work);
    void continueCollection(Work work);
    void finishNameOf(Work work);
    void finishOverflowPolicy(Work work);
    void startInterpolationToken(Work work, const lexer::Token& token);
    void scheduleInterpolation(std::size_t frameIndex, std::size_t resultSlot);
    void finishInterpolationHole(Work work);
    void enterInterpolationFragment(InterpolationFrame& frame);
    void restoreInterpolationParser(InterpolationFrame& frame);
    void schedulePrimaryArgument(Work work, bool tupleElement);
    ast::AstNode* finishPrimaryArgument(Work& work);
    bool startReservedPrimary(Work& work);
    bool startKeywordPrimary(Work& work);
    bool startAggregatePrimary(Work& work);
    void continueCall(Work work);
    void continueIndex(Work work);
    void continueWith(Work work);
    ast::AstNode* finishArgument(Work& work);
    void finishCall(Work& work);
    void appendIndex(Work& work, ast::AstNode* value);
    void finishIndex(Work& work);
    void continueNewArguments(Work work);
    void continuePlacementNew(Work work);
    void continueArrayNew(Work work);
    void continueArrayInitializer(Work work);
    void finishArrayGroup(Work& work);
    void finishArrayNew(Work work);
    bool startSimpleUnary(Work& work);
    bool startDelimitedUnary(Work& work);
    bool startAtomicUnary(Work& work);
    void startNewExpression(Work work);
    void finishDelimitedUnary(Work work);
    ast::AstNode* buildUnaryNode(Work& work, ast::AstNode* operand);

    Parser& parser_;
    std::deque<Result> results_;
    std::deque<InterpolationFrame> interpolationFrames_;
    std::vector<Work> work_;
    Work pendingLoop_;
    Work pendingComprehension_;
    std::size_t rootSlot_ = noSlot;
    bool resumable_ = false;
    ContinuationRequestKind rootKind_ = ContinuationRequestKind::Expression;
    bool started_ = false;
    bool awaitingBlock_ = false;
    bool awaitingComprehensionBody_ = false;
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    std::size_t outstandingResults_ = 0;
#endif
};
}
