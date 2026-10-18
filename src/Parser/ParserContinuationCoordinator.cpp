#include "nativesharp/Parser/Parser.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;

class Parser::ParseContinuationCoordinator final
{
public:
    explicit ParseContinuationCoordinator(Parser& parser) noexcept;
    ~ParseContinuationCoordinator() noexcept;

    ast::AstNode* parse(ContinuationRequestKind kind);

private:
    static constexpr std::size_t noSlot = std::numeric_limits<std::size_t>::max();

    enum class WorkKind : std::uint8_t
    {
        Expression,
        Block,
        ContinueBlock,
        AppendBlockStatement,
        Statement,
        FinishLambdaBlock,
        FinishExpressionStatement,
        FinishReturn,
        FinishThrow,
        FinishJump,
        FinishYieldReturn,
        FinishStaticAssert,
        FinishDeconstruction,
        ContinueVariableDeclaration,
        FinishVariableInitializer,
        Aggregate,
        ContinueAggregate,
        FinishAggregateElement,
        FinishLocalFunction,
        Embedded,
        FinishEmbedded,
        FinishIfCondition,
        FinishIfThen,
        FinishIfElse,
        FinishWhileCondition,
        FinishWhileBody,
        FinishDoBody,
        FinishDoCondition,
        FinishForInitializer,
        FinishForCondition,
        FinishForIncrement,
        FinishForBody,
        ContinueForExpressionList,
        FinishForExpression
    };

    enum class StatementKind : std::uint8_t
    {
        Direct,
        Block,
        Expression,
        Return,
        MustTailReturn,
        Throw,
        Jump,
        Yield,
        StaticAssert,
        AwaitExpression,
        Deconstruction,
        VariableDeclaration,
        StaticLocal,
        LocalFunction,
        If,
        While,
        DoWhile,
        For
    };

    struct ResultSlot final
    {
        ast::AstNode* value = nullptr;
        bool ready = false;
    };

    struct BlockFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t beforeIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* block = nullptr;
        ast::AstNode* destination = nullptr;
    };

    struct VariableFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::TypeSyntax* type = nullptr;
        ast::AstNode* first = nullptr;
        ast::AstNode* declarationList = nullptr;
        bool isStaticLocal = false;
        bool isConst = false;
        bool isRaii = false;
        bool isVolatile = false;
        bool isReferenceLocal = false;
        bool isPinned = false;
        bool isScope = false;
        bool expectSemicolon = true;
    };

    struct AggregateFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* node = nullptr;
    };

    struct IfFrame final
    {
        IfParseState state;
        std::size_t resultSlot = 0;
    };

    struct WhileFrame final
    {
        WhileParseState state;
        std::size_t resultSlot = 0;
    };

    struct DoWhileFrame final
    {
        DoWhileParseState state;
        std::size_t resultSlot = 0;
        ast::AstNode* body = nullptr;
    };

    struct ForFrame final
    {
        ForParseState state;
        std::size_t resultSlot = 0;
    };

    struct ForExpressionFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* first = nullptr;
        ast::AstNode* sequence = nullptr;
    };

    struct Work final
    {
        Work(WorkKind requestedKind, std::size_t requestedResultSlot) noexcept
            : kind(requestedKind), resultSlot(requestedResultSlot)
        {
        }

        WorkKind kind = WorkKind::Expression;
        std::size_t resultSlot = 0;
        std::size_t frameIndex = noSlot;
        std::size_t childSlot = noSlot;
        std::size_t beginIndex = 0;
        ast::AstNode* node = nullptr;
        TokenKind tokenKind = TokenKind::Invalid;
        bool flag = false;
        bool secondaryFlag = false;
        std::string text;
    };

    std::size_t request(ContinuationRequestKind kind);
    std::size_t makeResultSlot();
    void drain(std::size_t resultSlot);
    void execute(Work work);
    void complete(std::size_t resultSlot, ast::AstNode* value);
    ast::AstNode* childValue(const Work& work) const;

    void startExpression(const Work& work);
    void finishLambdaBlock(const Work& work);
    void startBlock(const Work& work);
    void continueBlock(const Work& work);
    void appendBlockStatement(const Work& work);
    void finishBlock(BlockFrame& frame);

    StatementKind classifyStatement() const;
    bool scopedStatementStarts() const noexcept;
    bool controlStatementStarts() const noexcept;
    bool ordinaryIfStarts() const noexcept;
    bool ordinaryWhileStarts() const noexcept;
    bool declarationStatementStarts() const noexcept;
    void startStatement(const Work& work);
    void startMustTailReturn(const Work& work);
    void startExpressionStatement(const Work& work);
    void finishExpressionStatement(const Work& work);
    void startReturn(const Work& work, bool mustTail);
    void finishReturn(const Work& work);
    void startThrow(const Work& work);
    void finishThrow(const Work& work);
    void startJump(const Work& work);
    void finishJump(const Work& work);
    void startYield(const Work& work);
    void finishYieldReturn(const Work& work);
    void startStaticAssert(const Work& work);
    void finishStaticAssert(const Work& work);
    void startAwaitExpression(const Work& work);
    void startDeconstruction(const Work& work);
    void finishDeconstruction(const Work& work);
    void startVariableDeclaration(
        const Work& work,
        bool isStaticLocal,
        bool expectSemicolon = true);
    void parseVariablePrefix(VariableFrame& frame);
    void continueVariableDeclaration(const Work& work);
    ast::AstNode* makeVariableDeclarator(VariableFrame& frame, bool cloneType);
    void scheduleVariableInitializer(const Work& work);
    void finishVariableInitializer(const Work& work);
    void acceptVariableDeclarator(std::size_t frameIndex, ast::AstNode* variable);
    void finishVariableDeclaration(VariableFrame& frame);
    void startAggregate(const Work& work);
    void continueAggregate(const Work& work);
    void finishAggregateElement(const Work& work);
    void finishAggregate(AggregateFrame& frame);
    void startLocalFunction(const Work& work);
    void finishLocalFunction(const Work& work);
    void startEmbedded(const Work& work);
    void finishEmbedded(const Work& work);
    ast::AstNode* wrapEmbedded(ast::AstNode* statement);
    ast::AstNode* conditionFromDeclaration(ast::AstNode* declaration);
    void startIf(const Work& work);
    void finishIfCondition(const Work& work);
    void finishIfThen(const Work& work);
    void finishIfElse(const Work& work);
    void startWhile(const Work& work);
    void finishWhileCondition(const Work& work);
    void finishWhileBody(const Work& work);
    void startDoWhile(const Work& work);
    void finishDoBody(const Work& work);
    void finishDoCondition(const Work& work);
    void startFor(const Work& work);
    void finishForInitializer(const Work& work);
    void scheduleForCondition(std::size_t frameIndex);
    void finishForCondition(const Work& work);
    void scheduleForIncrement(std::size_t frameIndex);
    void finishForIncrement(const Work& work);
    void scheduleForBody(std::size_t frameIndex);
    void finishForBody(const Work& work);
    void startForExpressionList(std::size_t resultSlot, std::size_t beginIndex);
    void continueForExpressionList(const Work& work);
    void finishForExpression(const Work& work);

    Parser& parser_;
    ParseContinuationCoordinator* previous_ = nullptr;
    std::deque<ResultSlot> resultSlots_;
    std::deque<BlockFrame> blockFrames_;
    std::deque<VariableFrame> variableFrames_;
    std::deque<AggregateFrame> aggregateFrames_;
    std::deque<IfFrame> ifFrames_;
    std::deque<WhileFrame> whileFrames_;
    std::deque<DoWhileFrame> doWhileFrames_;
    std::deque<ForFrame> forFrames_;
    std::deque<ForExpressionFrame> forExpressionFrames_;
    std::vector<Work> work_;
};

// Installs one non-owning coordinator for the parser invocation; restoring the previous pointer
// permits bounded fallback reentry without changing Parser ownership or its public ABI.
Parser::ParseContinuationCoordinator::ParseContinuationCoordinator(Parser& parser) noexcept
    : parser_(parser), previous_(parser.activeContinuationCoordinator_)
{
    parser_.activeContinuationCoordinator_ = this;
}

// Restores the prior coordinator when the outer parse request finishes.
Parser::ParseContinuationCoordinator::~ParseContinuationCoordinator() noexcept
{
    parser_.activeContinuationCoordinator_ = previous_;
}

// Schedules one facade request and drains only until its stable result slot is ready.
ast::AstNode* Parser::ParseContinuationCoordinator::parse(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = request(kind);
    drain(resultSlot);
    return resultSlots_[resultSlot].value;
}

// Maps the public facade kind to an internal work item in constant time.
std::size_t Parser::ParseContinuationCoordinator::request(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = makeResultSlot();
    WorkKind workKind = WorkKind::Expression;
    if (kind == ContinuationRequestKind::Block) workKind = WorkKind::Block;
    else if (kind == ContinuationRequestKind::Statement) workKind = WorkKind::Statement;
    work_.push_back({workKind, resultSlot});
    return resultSlot;
}

// Appends a stable result cell whose index remains valid across deque growth.
std::size_t Parser::ParseContinuationCoordinator::makeResultSlot()
{
    const std::size_t resultSlot = resultSlots_.size();
    resultSlots_.push_back({});
    return resultSlot;
}

// Executes the explicit LIFO worklist until the requested result is available.
void Parser::ParseContinuationCoordinator::drain(std::size_t resultSlot)
{
    while (!resultSlots_[resultSlot].ready)
    {
        assert(!work_.empty());
        const Work work = work_.back();
        work_.pop_back();
        execute(work);
    }
}

// Dispatches one bounded continuation step without recursive product traversal.
void Parser::ParseContinuationCoordinator::execute(Work work)
{
    switch (work.kind)
    {
    case WorkKind::Expression: startExpression(work); return;
    case WorkKind::Block: startBlock(work); return;
    case WorkKind::ContinueBlock: continueBlock(work); return;
    case WorkKind::AppendBlockStatement: appendBlockStatement(work); return;
    case WorkKind::Statement: startStatement(work); return;
    case WorkKind::FinishLambdaBlock: finishLambdaBlock(work); return;
    case WorkKind::FinishExpressionStatement: finishExpressionStatement(work); return;
    case WorkKind::FinishReturn: finishReturn(work); return;
    case WorkKind::FinishThrow: finishThrow(work); return;
    case WorkKind::FinishJump: finishJump(work); return;
    case WorkKind::FinishYieldReturn: finishYieldReturn(work); return;
    case WorkKind::FinishStaticAssert: finishStaticAssert(work); return;
    case WorkKind::FinishDeconstruction: finishDeconstruction(work); return;
    case WorkKind::ContinueVariableDeclaration: continueVariableDeclaration(work); return;
    case WorkKind::FinishVariableInitializer: finishVariableInitializer(work); return;
    case WorkKind::Aggregate: startAggregate(work); return;
    case WorkKind::ContinueAggregate: continueAggregate(work); return;
    case WorkKind::FinishAggregateElement: finishAggregateElement(work); return;
    case WorkKind::FinishLocalFunction: finishLocalFunction(work); return;
    case WorkKind::Embedded: startEmbedded(work); return;
    case WorkKind::FinishEmbedded: finishEmbedded(work); return;
    case WorkKind::FinishIfCondition: finishIfCondition(work); return;
    case WorkKind::FinishIfThen: finishIfThen(work); return;
    case WorkKind::FinishIfElse: finishIfElse(work); return;
    case WorkKind::FinishWhileCondition: finishWhileCondition(work); return;
    case WorkKind::FinishWhileBody: finishWhileBody(work); return;
    case WorkKind::FinishDoBody: finishDoBody(work); return;
    case WorkKind::FinishDoCondition: finishDoCondition(work); return;
    case WorkKind::FinishForInitializer: finishForInitializer(work); return;
    case WorkKind::FinishForCondition: finishForCondition(work); return;
    case WorkKind::FinishForIncrement: finishForIncrement(work); return;
    case WorkKind::FinishForBody: finishForBody(work); return;
    case WorkKind::ContinueForExpressionList: continueForExpressionList(work); return;
    case WorkKind::FinishForExpression: finishForExpression(work); return;
    }
}

// Publishes a result exactly once for its waiting continuation.
void Parser::ParseContinuationCoordinator::complete(
    std::size_t resultSlot,
    ast::AstNode* value)
{
    resultSlots_[resultSlot].value = value;
    resultSlots_[resultSlot].ready = true;
}

// Reads an optional completed child; a missing child represents a statement without a value.
ast::AstNode* Parser::ParseContinuationCoordinator::childValue(const Work& work) const
{
    if (work.childSlot == noSlot) return nullptr;
    assert(resultSlots_[work.childSlot].ready);
    return resultSlots_[work.childSlot].value;
}

// Extracts only block-bodied lambdas; all other expressions retain the canonical parser path.
void Parser::ParseContinuationCoordinator::startExpression(const Work& work)
{
    if (!parser_.lambdaBlockBodyStartsHere())
    {
        complete(work.resultSlot, parser_.parseExpressionImpl());
        return;
    }
    ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
    const std::size_t bodySlot = makeResultSlot();
    Work finish{WorkKind::FinishLambdaBlock, work.resultSlot};
    finish.childSlot = bodySlot;
    finish.node = lambda;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, bodySlot});
}

// Attaches the completed block with the canonical lambda flags and range update.
void Parser::ParseContinuationCoordinator::finishLambdaBlock(const Work& work)
{
    parser_.finishLambdaExpression(work.node, childValue(work), true);
    complete(work.resultSlot, work.node);
}

// Creates one lexical block frame and schedules its first bounded continuation.
void Parser::ParseContinuationCoordinator::startBlock(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    parser_.expect(TokenKind::LeftBrace, "'{'");
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, parser_.tokenRangeFrom(beginIndex));
    const std::size_t frameIndex = blockFrames_.size();
    blockFrames_.push_back({beginIndex, parser_.index_, work.resultSlot, block, block});
    Work next{WorkKind::ContinueBlock, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Either closes the block or schedules exactly one statement plus its append continuation.
void Parser::ParseContinuationCoordinator::continueBlock(const Work& work)
{
    BlockFrame& frame = blockFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        finishBlock(frame);
        return;
    }
    frame.beforeIndex = parser_.index_;
    const std::size_t statementSlot = makeResultSlot();
    Work append{WorkKind::AppendBlockStatement, frame.resultSlot};
    append.frameIndex = work.frameIndex;
    append.childSlot = statementSlot;
    work_.push_back(std::move(append));
    work_.push_back({WorkKind::Statement, statementSlot});
}

// Preserves defer destination rewriting, inline expansions, and the historical progress guard.
void Parser::ParseContinuationCoordinator::appendBlockStatement(const Work& work)
{
    BlockFrame& frame = blockFrames_[work.frameIndex];
    parser_.appendParsedStatement(frame.destination, childValue(work));
    if (parser_.index_ == frame.beforeIndex) parser_.consume();
    Work next{WorkKind::ContinueBlock, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(std::move(next));
}

// Consumes the closing delimiter, fixes the full source range, and publishes the block.
void Parser::ParseContinuationCoordinator::finishBlock(BlockFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}'");
    frame.block->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.block);
}

// Mirrors the side-effect-free prefix test used before the scoped-statement fallback.
bool Parser::ParseContinuationCoordinator::scopedStatementStarts() const noexcept
{
    return (parser_.atContextualKeyword("fp") && parser_.peek().kind() == TokenKind::LeftParen) ||
        (parser_.atContextualKeyword("unsafe") &&
         (parser_.peek().kind() == TokenKind::LeftBrace ||
          parser_.peek().kind() == TokenKind::LeftParen)) ||
        (parser_.atContextualKeyword("asm") &&
         (parser_.peek().kind() == TokenKind::LeftBrace ||
          parser_.peek().kind() == TokenKind::LeftParen));
}

// Recognizes control-family prefixes without consuming tokens or changing their parser.
bool Parser::ParseContinuationCoordinator::controlStatementStarts() const noexcept
{
    if (parser_.atContextualKeyword("let")) return true;
    if ((parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile)) &&
        parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let") return true;
    return parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile) ||
        parser_.at(TokenKind::KwDo) || parser_.at(TokenKind::KwFor) ||
        parser_.at(TokenKind::KwForeach) || parser_.at(TokenKind::KwSwitch) ||
        parser_.startsParenthesizedBlock("match");
}

// Distinguishes ordinary if from the conditional-pattern form owned by R03C2C2E.
bool Parser::ParseContinuationCoordinator::ordinaryIfStarts() const noexcept
{
    const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let";
    return parser_.at(TokenKind::KwIf) && !conditional;
}

// Distinguishes ordinary while from the conditional-pattern form owned by R03C2C2E.
bool Parser::ParseContinuationCoordinator::ordinaryWhileStarts() const noexcept
{
    const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let";
    return parser_.at(TokenKind::KwWhile) && !conditional;
}

// Mirrors variable-declaration disambiguation after C-owned special forms are classified.
bool Parser::ParseContinuationCoordinator::declarationStatementStarts() const noexcept
{
    if (parser_.atContextualKeyword("pin") &&
        (parser_.peek().kind() == TokenKind::KwRaii ||
         (parser_.peek().kind() == TokenKind::Identifier && parser_.peek().lexeme() == "ref")))
        return true;
    if (parser_.at(TokenKind::KwRaii) || parser_.at(TokenKind::KwConst) ||
        parser_.at(TokenKind::KwVolatile) || parser_.at(TokenKind::KwVar)) return true;
    if (parser_.atContextualKeyword("ref") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) ||
         parser_.peek().kind() == TokenKind::KwVar || parser_.peek().kind() == TokenKind::KwConst ||
         parser_.peek().kind() == TokenKind::KwRaii)) return true;
    return parser_.looksLikeVariableDeclaration() && !parser_.looksLikeClosedGenericTypeReference();
}

// Reproduces the canonical dispatcher order while isolating the B and C worklist families.
Parser::ParseContinuationCoordinator::StatementKind
Parser::ParseContinuationCoordinator::classifyStatement() const
{
    if (parser_.at(TokenKind::LeftBrace)) return StatementKind::Block;
    if (ordinaryIfStarts()) return StatementKind::If;
    if (ordinaryWhileStarts()) return StatementKind::While;
    if (parser_.at(TokenKind::KwDo)) return StatementKind::DoWhile;
    if (parser_.at(TokenKind::KwFor)) return StatementKind::For;
    if (scopedStatementStarts() || controlStatementStarts()) return StatementKind::Direct;
    if (parser_.atContextualKeyword("yield"))
        return parser_.comprehensionContexts_.empty() ? StatementKind::Yield : StatementKind::Direct;
    if (parser_.at(TokenKind::KwStatic))
        return parser_.looksLikeLocalFunction()
            ? StatementKind::LocalFunction : StatementKind::StaticLocal;
    if (parser_.at(TokenKind::KwBreak) || parser_.at(TokenKind::KwContinue)) return StatementKind::Jump;
    if (parser_.atContextualKeyword("must_tail")) return StatementKind::MustTailReturn;
    if (parser_.at(TokenKind::KwReturn)) return StatementKind::Return;
    if (parser_.atContextualKeyword("defer") || parser_.at(TokenKind::KwTry) ||
        parser_.at(TokenKind::KwUsing)) return StatementKind::Direct;
    if (parser_.at(TokenKind::KwThrow)) return StatementKind::Throw;
    if (parser_.at(TokenKind::KwStaticAssert)) return StatementKind::StaticAssert;
    if (parser_.at(TokenKind::KwVar) && parser_.peek().kind() == TokenKind::LeftParen)
        return StatementKind::Deconstruction;
    if (parser_.looksLikeLocalFunction()) return StatementKind::LocalFunction;
    if (parser_.atContextualKeyword("await")) return StatementKind::AwaitExpression;
    if (declarationStatementStarts()) return StatementKind::VariableDeclaration;
    return StatementKind::Expression;
}

// Schedules one simple statement or delegates an untouched C-F family atomically.
void Parser::ParseContinuationCoordinator::startStatement(const Work& work)
{
    switch (classifyStatement())
    {
    case StatementKind::Direct: complete(work.resultSlot, parser_.parseStatementImpl()); return;
    case StatementKind::Block: work_.push_back({WorkKind::Block, work.resultSlot}); return;
    case StatementKind::Expression: startExpressionStatement(work); return;
    case StatementKind::Return: startReturn(work, false); return;
    case StatementKind::MustTailReturn: startMustTailReturn(work); return;
    case StatementKind::Throw: startThrow(work); return;
    case StatementKind::Jump: startJump(work); return;
    case StatementKind::Yield: startYield(work); return;
    case StatementKind::StaticAssert: startStaticAssert(work); return;
    case StatementKind::AwaitExpression: startAwaitExpression(work); return;
    case StatementKind::Deconstruction: startDeconstruction(work); return;
    case StatementKind::VariableDeclaration: startVariableDeclaration(work, false); return;
    case StatementKind::StaticLocal: startVariableDeclaration(work, true); return;
    case StatementKind::LocalFunction: startLocalFunction(work); return;
    case StatementKind::If: startIf(work); return;
    case StatementKind::While: startWhile(work); return;
    case StatementKind::DoWhile: startDoWhile(work); return;
    case StatementKind::For: startFor(work); return;
    }
}

// Preserves must_tail validation and then shares the ordinary return continuation.
void Parser::ParseContinuationCoordinator::startMustTailReturn(const Work& work)
{
    const SourceRange attributeRange = parser_.consume().range();
    if (!parser_.at(TokenKind::KwReturn))
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "must_tail must be followed by a return statement");
        parser_.synchronizeStatement();
        complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, attributeRange));
        return;
    }
    startReturn(work, true);
}

// Records the statement start and schedules its expression as a stable child result.
void Parser::ParseContinuationCoordinator::startExpressionStatement(const Work& work)
{
    const std::size_t childSlot = makeResultSlot();
    Work finish{WorkKind::FinishExpressionStatement, work.resultSlot};
    finish.childSlot = childSlot;
    finish.beginIndex = parser_.index_;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Expression, childSlot});
}

// Builds the statement before consuming ';' and then extends its range, matching legacy order.
void Parser::ParseContinuationCoordinator::finishExpressionStatement(const Work& work)
{
    ast::AstNode* expression = childValue(work);
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExpressionStatement,
        expression != nullptr ? expression->range() : parser_.current().range());
    statement->setFirst(expression);
    parser_.expect(TokenKind::Semicolon, work.text.empty() ? "';'" : work.text);
    statement->setRange(parser_.tokenRangeFrom(work.beginIndex));
    complete(work.resultSlot, statement);
}

// Consumes return/ref syntax and schedules only the optional value expression.
void Parser::ParseContinuationCoordinator::startReturn(const Work& work, bool mustTail)
{
    Work finish{WorkKind::FinishReturn, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    finish.flag = parser_.atContextualKeyword("ref") &&
        parser_.peek().kind() != TokenKind::Semicolon;
    if (finish.flag) parser_.consume();
    finish.secondaryFlag = mustTail;
    if (parser_.at(TokenKind::Semicolon))
    {
        finishReturn(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Completes the return after ';', retaining reference and must-tail flags.
void Parser::ParseContinuationCoordinator::finishReturn(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ReturnStatement, parser_.tokenRangeFrom(work.beginIndex));
    node->setFirst(childValue(work));
    node->setReference(work.flag);
    node->setFlag(work.secondaryFlag);
    complete(work.resultSlot, node);
}

// Consumes throw and schedules its optional value expression without native recursion.
void Parser::ParseContinuationCoordinator::startThrow(const Work& work)
{
    Work finish{WorkKind::FinishThrow, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    if (parser_.at(TokenKind::Semicolon))
    {
        finishThrow(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Completes the throw after its contractual semicolon and full range.
void Parser::ParseContinuationCoordinator::finishThrow(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ThrowStatement, parser_.tokenRangeFrom(work.beginIndex));
    node->setFirst(childValue(work));
    complete(work.resultSlot, node);
}

// Parses jump labels eagerly and schedules only a break value when one exists.
void Parser::ParseContinuationCoordinator::startJump(const Work& work)
{
    Work finish{WorkKind::FinishJump, work.resultSlot};
    finish.beginIndex = parser_.index_;
    finish.tokenKind = parser_.consume().kind();
    if (finish.tokenKind == TokenKind::KwBreak && parser_.at(TokenKind::Identifier) &&
        parser_.peek().kind() == TokenKind::Colon)
    {
        finish.text = std::string(parser_.consume().lexeme());
        parser_.consume();
    }
    else if (finish.tokenKind == TokenKind::KwContinue && parser_.at(TokenKind::Identifier))
        finish.text = std::string(parser_.consume().lexeme());
    if (finish.tokenKind != TokenKind::KwBreak || parser_.at(TokenKind::Semicolon))
    {
        finishJump(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Builds break or continue after ';', preserving label text and optional break value.
void Parser::ParseContinuationCoordinator::finishJump(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* exit = parser_.arena_.make<ast::AstNode>(
        work.tokenKind == TokenKind::KwBreak
            ? ast::AstNodeKind::BreakStatement : ast::AstNodeKind::ContinueStatement,
        parser_.tokenRangeFrom(work.beginIndex));
    exit->setFirst(childValue(work));
    exit->setText(work.text);
    complete(work.resultSlot, exit);
}

// Handles non-comprehension yield forms and schedules only yield-return's expression.
void Parser::ParseContinuationCoordinator::startYield(const Work& work)
{
    Work finish{WorkKind::FinishYieldReturn, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    if (parser_.at(TokenKind::KwReturn))
    {
        parser_.consume();
        finish.childSlot = makeResultSlot();
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, finish.childSlot});
        return;
    }
    ast::AstNodeKind kind = ast::AstNodeKind::SuspendStatement;
    std::string_view semicolonDescription = "';' after 'yield'";
    if (parser_.at(TokenKind::KwBreak))
    {
        parser_.consume();
        kind = ast::AstNodeKind::YieldBreakStatement;
        semicolonDescription = "';' after 'yield return'/'yield break'";
    }
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        kind, parser_.tokenRangeFrom(finish.beginIndex));
    parser_.expect(TokenKind::Semicolon, semicolonDescription);
    complete(work.resultSlot, statement);
}

// Builds yield-return before consuming ';', preserving the legacy range boundary.
void Parser::ParseContinuationCoordinator::finishYieldReturn(const Work& work)
{
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::YieldReturnStatement, parser_.tokenRangeFrom(work.beginIndex));
    statement->setFirst(childValue(work));
    parser_.expect(TokenKind::Semicolon, "';' after 'yield return'/'yield break'");
    complete(work.resultSlot, statement);
}

// Consumes the static_assert prefix and schedules its condition expression.
void Parser::ParseContinuationCoordinator::startStaticAssert(const Work& work)
{
    Work finish{WorkKind::FinishStaticAssert, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Parses the optional message and delimiters, then publishes the canonical statement node.
void Parser::ParseContinuationCoordinator::finishStaticAssert(const Work& work)
{
    std::string message;
    if (parser_.consumeIf(TokenKind::Comma))
    {
        if (parser_.at(TokenKind::StringLiteral))
            message = std::get<std::string>(parser_.consume().value());
        else
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "static_assert message must be a string literal");
    }
    parser_.expect(TokenKind::RightParen, "')'");
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::StaticAssertStatement, parser_.tokenRangeFrom(work.beginIndex));
    statement->setFirst(childValue(work));
    statement->setText(std::move(message));
    complete(work.resultSlot, statement);
}

// Routes contextual await through the expression worklist while preserving its diagnostic text.
void Parser::ParseContinuationCoordinator::startAwaitExpression(const Work& work)
{
    const std::size_t childSlot = makeResultSlot();
    Work finish{WorkKind::FinishExpressionStatement, work.resultSlot};
    finish.childSlot = childSlot;
    finish.beginIndex = parser_.index_;
    finish.text = "';' after await expression";
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Expression, childSlot});
}

// Parses only the binding pattern synchronously, then schedules the recursive RHS.
void Parser::ParseContinuationCoordinator::startDeconstruction(const Work& work)
{
    Work finish{WorkKind::FinishDeconstruction, work.resultSlot};
    finish.beginIndex = parser_.index_;
    const SourceRange featureRange = parser_.consume().range();
    ast::AstNode* pattern = parser_.parseDeconstructionPattern();
    finish.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, featureRange);
    if (pattern != nullptr && pattern->kind() == ast::AstNodeKind::TupleExpression)
        for (ast::AstNode* binding : pattern->children()) finish.node->addChild(binding);
    parser_.expect(TokenKind::Equal, "'=' after deconstruction bindings");
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Attaches the completed assignment expression and preserves delimiter/range ordering.
void Parser::ParseContinuationCoordinator::finishDeconstruction(const Work& work)
{
    work.node->setFirst(childValue(work));
    parser_.expect(TokenKind::Semicolon, "';'");
    work.node->setRange(parser_.tokenRangeFrom(work.beginIndex));
    complete(work.resultSlot, work.node);
}

// Validates static-local syntax before sharing the ordinary variable-declaration machine.
void Parser::ParseContinuationCoordinator::startVariableDeclaration(
    const Work& work,
    bool isStaticLocal,
    bool expectSemicolon)
{
    const std::size_t beginIndex = parser_.index_;
    if (isStaticLocal)
    {
        const SourceRange staticRange = parser_.consume().range();
        if (!declarationStatementStarts())
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                staticRange,
                "static statement must declare a local variable");
            parser_.synchronizeStatement();
            complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, staticRange));
            return;
        }
    }
    const std::size_t frameIndex = variableFrames_.size();
    variableFrames_.push_back({beginIndex, work.resultSlot});
    variableFrames_.back().isStaticLocal = isStaticLocal;
    variableFrames_.back().expectSemicolon = expectSemicolon;
    parseVariablePrefix(variableFrames_.back());
    Work next{WorkKind::ContinueVariableDeclaration, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Consumes declaration modifiers and type exactly once for every declarator in the frame.
void Parser::ParseContinuationCoordinator::parseVariablePrefix(VariableFrame& frame)
{
    const bool pinnedReference = parser_.atContextualKeyword("pin") &&
        parser_.peek().kind() == TokenKind::Identifier && parser_.peek().lexeme() == "ref";
    frame.isPinned = pinnedReference ||
        (parser_.atContextualKeyword("pin") && parser_.peek().kind() == TokenKind::KwRaii);
    if (frame.isPinned) parser_.consume();
    frame.isReferenceLocal = parser_.atContextualKeyword("ref") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) || parser_.peek().kind() == TokenKind::KwVar ||
         parser_.peek().kind() == TokenKind::KwConst || parser_.peek().kind() == TokenKind::KwRaii);
    if (frame.isReferenceLocal) parser_.consume();
    frame.isRaii = parser_.consumeIf(TokenKind::KwRaii);
    frame.isVolatile = parser_.consumeIf(TokenKind::KwVolatile);
    frame.isConst = parser_.consumeIf(TokenKind::KwConst);
    frame.isScope = parser_.atContextualKeyword("scope") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) || parser_.peek().kind() == TokenKind::KwVar);
    if (frame.isScope) parser_.consume();
    if (frame.isRaii && frame.isConst)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parser_.tokens_[frame.beginIndex].range(),
            "raii locals cannot also be const");
    if (!parser_.at(TokenKind::KwVar)) { frame.type = parser_.parseType(); return; }
    if (frame.isReferenceLocal)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "reference locals require an explicit element type");
    const lexer::Token& varToken = parser_.consume();
    frame.type = parser_.arena_.make<ast::TypeSyntax>(
        varToken.range(), TokenKind::KwVar, std::string("var"));
}

// Creates one declarator without consuming its optional initializer.
ast::AstNode* Parser::ParseContinuationCoordinator::makeVariableDeclarator(
    VariableFrame& frame,
    bool cloneType)
{
    if (!parser_.at(TokenKind::Identifier))
    {
        parser_.expect(TokenKind::Identifier, "variable name");
        return nullptr;
    }
    const lexer::Token& name = parser_.consume();
    ast::AstNode* variable = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement, name.range());
    variable->setTypeSyntax(cloneType ? parser_.cloneDeclarationType(frame.type) : frame.type);
    variable->setText(std::string(name.lexeme()));
    variable->setFlag(frame.isConst);
    if (frame.isRaii) variable->addModifier(TokenKind::KwRaii);
    if (frame.isVolatile) variable->addModifier(TokenKind::KwVolatile);
    if (frame.isReferenceLocal)
        variable->setAuxiliaryText(frame.isPinned ? "local:pinned-ref" : "local:ref");
    else if (frame.isPinned) variable->setAuxiliaryText("local:pinned");
    return variable;
}

// Advances one declarator and schedules only its recursive initializer when present.
void Parser::ParseContinuationCoordinator::continueVariableDeclaration(const Work& work)
{
    VariableFrame& frame = variableFrames_[work.frameIndex];
    const std::size_t beginIndex = parser_.index_;
    ast::AstNode* variable = makeVariableDeclarator(frame, frame.first != nullptr);
    if (variable == nullptr)
    {
        parser_.synchronizeStatement();
        if (frame.first == nullptr) complete(frame.resultSlot, nullptr);
        else finishVariableDeclaration(frame);
        return;
    }
    Work finish{WorkKind::FinishVariableInitializer, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.beginIndex = beginIndex;
    finish.node = variable;
    if (!parser_.consumeIf(TokenKind::Equal))
    {
        if (frame.isReferenceLocal)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "reference-local declaration requires '= ref <storage>'");
        else if (frame.isConst)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "const declaration requires an initializer");
        finishVariableInitializer(finish);
        return;
    }
    if (frame.isReferenceLocal)
        parser_.expectContextualKeyword("ref", "'ref' before the reference-local initializer");
    scheduleVariableInitializer(finish);
}

// Selects aggregate or ordinary expression parsing without adding a native frame.
void Parser::ParseContinuationCoordinator::scheduleVariableInitializer(
    const Work& finish)
{
    Work continuation = finish;
    continuation.childSlot = makeResultSlot();
    work_.push_back(continuation);
    work_.push_back({
        parser_.at(TokenKind::LeftBrace) ? WorkKind::Aggregate : WorkKind::Expression,
        continuation.childSlot});
}

// Attaches an optional initializer, fixes the declarator range, and advances the list.
void Parser::ParseContinuationCoordinator::finishVariableInitializer(const Work& work)
{
    work.node->setFirst(childValue(work));
    work.node->setRange(parser_.tokenRangeFrom(work.beginIndex));
    acceptVariableDeclarator(work.frameIndex, work.node);
}

// Preserves declaration-list construction, static/scope metadata, and source order.
void Parser::ParseContinuationCoordinator::acceptVariableDeclarator(
    std::size_t frameIndex,
    ast::AstNode* variable)
{
    VariableFrame& frame = variableFrames_[frameIndex];
    if (frame.isStaticLocal) variable->addModifier(TokenKind::KwStatic);
    if (frame.first == nullptr)
    {
        frame.first = variable;
        if (frame.isScope) variable->setAuxiliaryText("local:scope");
    }
    else
    {
        if (frame.declarationList == nullptr)
        {
            frame.declarationList = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, frame.first->range());
            frame.declarationList->setAuxiliaryText("declaration-list");
            frame.declarationList->addChild(frame.first);
        }
        frame.declarationList->addChild(variable);
    }
    if (!parser_.consumeIf(TokenKind::Comma)) { finishVariableDeclaration(frame); return; }
    Work next{WorkKind::ContinueVariableDeclaration, frame.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Consumes the statement delimiter and publishes either one declarator or its list wrapper.
void Parser::ParseContinuationCoordinator::finishVariableDeclaration(VariableFrame& frame)
{
    if (frame.expectSemicolon) parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* result = frame.declarationList != nullptr
        ? frame.declarationList : frame.first;
    if (result != nullptr) result->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, result);
}

// Opens one aggregate frame and advances it through bounded element continuations.
void Parser::ParseContinuationCoordinator::startAggregate(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    const lexer::Token& open = parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AggregateInitializerExpression, open.range());
    const std::size_t frameIndex = aggregateFrames_.size();
    aggregateFrames_.push_back({beginIndex, work.resultSlot, node});
    Work next{WorkKind::ContinueAggregate, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Schedules exactly one aggregate element while preserving spread/designator precedence.
void Parser::ParseContinuationCoordinator::continueAggregate(const Work& work)
{
    AggregateFrame& frame = aggregateFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        finishAggregate(frame);
        return;
    }
    Work finish{WorkKind::FinishAggregateElement, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    WorkKind childKind = parser_.at(TokenKind::LeftBrace)
        ? WorkKind::Aggregate : WorkKind::Expression;
    if (parser_.at(TokenKind::DotDot))
    {
        const SourceRange spread = parser_.consume().range();
        finish.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SpreadElementExpression, spread);
        finish.node->setTokenKind(TokenKind::DotDot);
        finish.tokenKind = TokenKind::DotDot;
        childKind = WorkKind::Expression;
    }
    else if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Equal)
    {
        finish.text = std::string(parser_.consume().lexeme());
        parser_.consume();
        childKind = parser_.at(TokenKind::LeftBrace)
            ? WorkKind::Aggregate : WorkKind::Expression;
    }
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({childKind, childSlot});
}

// Attaches one completed element and advances or closes the enclosing aggregate.
void Parser::ParseContinuationCoordinator::finishAggregateElement(const Work& work)
{
    AggregateFrame& frame = aggregateFrames_[work.frameIndex];
    ast::AstNode* element = childValue(work);
    if (work.tokenKind == TokenKind::DotDot)
    {
        work.node->setFirst(element);
        work.node->setRange(parser_.join(
            work.node->range(), element != nullptr ? element->range() : work.node->range()));
        element = work.node;
    }
    if (element != nullptr && !work.text.empty()) element->setArgumentName(work.text);
    if (element == nullptr) { finishAggregate(frame); return; }
    frame.node->addChild(element);
    if (!parser_.consumeIf(TokenKind::Comma)) { finishAggregate(frame); return; }
    Work next{WorkKind::ContinueAggregate, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(std::move(next));
}

// Closes one aggregate and publishes its exact token range.
void Parser::ParseContinuationCoordinator::finishAggregate(AggregateFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}'");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Parses a local-function header, deferring only its block body to the shared worklist.
void Parser::ParseContinuationCoordinator::startLocalFunction(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    const bool isStaticLocal = parser_.at(TokenKind::KwStatic);
    if (isStaticLocal) parser_.consume();
    ast::AstNode* function = parser_.parseFunctionOrGlobalVariable(true);
    if (function == nullptr || function->kind() != ast::AstNodeKind::FunctionDeclaration ||
        !parser_.at(TokenKind::LeftBrace))
    {
        complete(work.resultSlot, parser_.lowerLocalFunctionDeclaration(
            function, isStaticLocal, beginIndex));
        return;
    }
    Work finish{WorkKind::FinishLocalFunction, work.resultSlot};
    finish.beginIndex = beginIndex;
    finish.node = function;
    finish.flag = isStaticLocal;
    finish.childSlot = makeResultSlot();
    const std::size_t bodySlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, bodySlot});
}

// Restores canonical body metadata before lowering the local function to its lambda declaration.
void Parser::ParseContinuationCoordinator::finishLocalFunction(const Work& work)
{
    ast::AstNode* const body = childValue(work);
    work.node->setFirst(body);
    if (body != nullptr && work.node->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        parser_.applyMemberFloatingPointMode(body, work.node->floatingPointMode());
    if (body != nullptr) work.node->setRange(parser_.join(work.node->range(), body->range()));
    complete(work.resultSlot, parser_.lowerLocalFunctionDeclaration(
        work.node, work.flag, work.beginIndex));
}

// Wraps one non-block embedded statement in the canonical block shape and preserves diagnostics.
ast::AstNode* Parser::ParseContinuationCoordinator::wrapEmbedded(ast::AstNode* statement)
{
    if (statement != nullptr &&
        (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
         statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement ||
         (statement->kind() == ast::AstNodeKind::BlockStatement &&
          statement->auxiliaryText() == "conditional-pattern-inline")))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "a declaration used as an if or else body requires an explicit '{ ... }' block");
    const SourceRange range = statement != nullptr
        ? statement->range() : parser_.current().range();
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, range);
    if (statement != nullptr) block->addChild(statement);
    return block;
}

// Schedules one embedded body without invoking a nested facade or nested drain.
void Parser::ParseContinuationCoordinator::startEmbedded(const Work& work)
{
    if (parser_.at(TokenKind::LeftBrace))
    {
        work_.push_back({WorkKind::Block, work.resultSlot});
        return;
    }
    if (parser_.at(TokenKind::Semicolon))
    {
        complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, parser_.consume().range()));
        return;
    }
    Work finish{WorkKind::FinishEmbedded, work.resultSlot};
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Statement, childSlot});
}

// Publishes the canonical block wrapper around one completed embedded statement.
void Parser::ParseContinuationCoordinator::finishEmbedded(const Work& work)
{
    complete(work.resultSlot, wrapEmbedded(childValue(work)));
}

// Selects the single declaration binding and reproduces its exact invalid-list diagnostic.
ast::AstNode* Parser::ParseContinuationCoordinator::conditionFromDeclaration(
    ast::AstNode* declaration)
{
    if (declaration == nullptr) return nullptr;
    const ast::AstNode* binding = declaration;
    if (declaration->kind() == ast::AstNodeKind::BlockStatement &&
        declaration->auxiliaryText() == "declaration-list")
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a condition declaration must declare exactly one local");
        if (declaration->children().empty()) return nullptr;
        binding = declaration->children().front();
    }
    return parser_.makeIdentifierExpression(binding->range(), binding->text());
}

// Opens an ordinary if header and schedules its condition through one stable child slot.
void Parser::ParseContinuationCoordinator::startIf(const Work& work)
{
    const std::size_t frameIndex = ifFrames_.size();
    ifFrames_.push_back({{}, work.resultSlot});
    IfFrame& frame = ifFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishIfCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the if header and schedules exactly one embedded then body.
void Parser::ParseContinuationCoordinator::finishIfCondition(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    Work finish{WorkKind::FinishIfThen, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Embedded, childSlot});
}

// Attaches the then body and preserves nearest-if binding for a following else clause.
void Parser::ParseContinuationCoordinator::finishIfThen(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    frame.state.thenBlock = childValue(work);
    if (!parser_.consumeIf(TokenKind::KwElse))
    {
        complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishIfElse, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({parser_.at(TokenKind::KwIf)
        ? WorkKind::Statement : WorkKind::Embedded, childSlot});
}

// Publishes the completed ordinary if after its optional else subtree is ready.
void Parser::ParseContinuationCoordinator::finishIfElse(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, childValue(work)));
}

// Opens an ordinary while header and schedules its condition without native reentry.
void Parser::ParseContinuationCoordinator::startWhile(const Work& work)
{
    const std::size_t frameIndex = whileFrames_.size();
    whileFrames_.push_back({{}, work.resultSlot});
    WhileFrame& frame = whileFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishWhileCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the while header and requires the same explicit block body as the legacy path.
void Parser::ParseContinuationCoordinator::finishWhileCondition(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after while condition");
        complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishWhileBody, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed body to the canonical while lowering.
void Parser::ParseContinuationCoordinator::finishWhileBody(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, childValue(work)));
}

// Opens a do-while and schedules its required block body on the shared worklist.
void Parser::ParseContinuationCoordinator::startDoWhile(const Work& work)
{
    const std::size_t frameIndex = doWhileFrames_.size();
    doWhileFrames_.push_back({{parser_.index_}, work.resultSlot, nullptr});
    parser_.consume();
    Work finish{WorkKind::FinishDoBody, work.resultSlot};
    finish.frameIndex = frameIndex;
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after 'do'");
        work_.push_back(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Consumes the trailing do-while prefix and schedules its condition expression.
void Parser::ParseContinuationCoordinator::finishDoBody(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    frame.body = childValue(work);
    parser_.expect(TokenKind::KwWhile, "'while' after the do-loop body");
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishDoCondition, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Builds the do-while node after its condition and contractual trailing semicolon.
void Parser::ParseContinuationCoordinator::finishDoCondition(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    parser_.expect(TokenKind::RightParen, "')'");
    parser_.expect(TokenKind::Semicolon, "';' after the do-while condition");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DoWhileStatement,
        parser_.tokenRangeFrom(frame.state.beginIndex));
    node->setFirst(childValue(work));
    node->setSecond(frame.body);
    complete(frame.resultSlot, node);
}

// Opens a for header and schedules only the first present clause.
void Parser::ParseContinuationCoordinator::startFor(const Work& work)
{
    const std::size_t frameIndex = forFrames_.size();
    forFrames_.push_back({{}, work.resultSlot});
    ForFrame& frame = forFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForCondition(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForInitializer, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (parser_.at(TokenKind::KwRaii) || parser_.at(TokenKind::KwConst) ||
        parser_.at(TokenKind::KwVolatile) || parser_.at(TokenKind::KwVar) ||
        parser_.canStartTypeAt(parser_.index_))
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed initializer, consumes its delimiter, and advances to the condition.
void Parser::ParseContinuationCoordinator::finishForInitializer(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    frame.state.initializer = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForCondition(work.frameIndex);
}

// Schedules the optional for condition or advances directly across its delimiter.
void Parser::ParseContinuationCoordinator::scheduleForCondition(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForIncrement(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForCondition, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores the completed for condition, consumes ';', and advances to the increment clause.
void Parser::ParseContinuationCoordinator::finishForCondition(const Work& work)
{
    forFrames_[work.frameIndex].state.condition = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForIncrement(work.frameIndex);
}

// Schedules the optional increment expression list or advances directly to the body.
void Parser::ParseContinuationCoordinator::scheduleForIncrement(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::RightParen))
    {
        parser_.consume();
        scheduleForBody(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForIncrement, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed increment list and closes the parenthesized for header.
void Parser::ParseContinuationCoordinator::finishForIncrement(const Work& work)
{
    forFrames_[work.frameIndex].state.increment = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    scheduleForBody(work.frameIndex);
}

// Requires the canonical block body and schedules it without nested parser invocation.
void Parser::ParseContinuationCoordinator::scheduleForBody(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after for clause");
        complete(frame.resultSlot, parser_.lowerForStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishForBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed block to the canonical for lowering.
void Parser::ParseContinuationCoordinator::finishForBody(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerForStatement(frame.state, childValue(work)));
}

// Creates one explicit frame for a comma-separated for-clause expression list.
void Parser::ParseContinuationCoordinator::startForExpressionList(
    std::size_t resultSlot,
    std::size_t beginIndex)
{
    const std::size_t frameIndex = forExpressionFrames_.size();
    forExpressionFrames_.push_back({beginIndex, resultSlot, nullptr, nullptr});
    Work next{WorkKind::ContinueForExpressionList, resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(next);
}

// Schedules exactly one expression from the current for-clause list.
void Parser::ParseContinuationCoordinator::continueForExpressionList(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    Work finish{WorkKind::FinishForExpression, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Appends one expression and either publishes the list or schedules its next element.
void Parser::ParseContinuationCoordinator::finishForExpression(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    ast::AstNode* expression = childValue(work);
    if (frame.first == nullptr) frame.first = expression;
    if (frame.sequence != nullptr && expression != nullptr)
        frame.sequence->addChild(parser_.makeExpressionStatement(expression));
    if (!parser_.at(TokenKind::Comma))
    {
        ast::AstNode* result = frame.sequence != nullptr ? frame.sequence : frame.first;
        if (frame.sequence != nullptr)
            frame.sequence->setRange(parser_.tokenRangeFrom(frame.beginIndex));
        complete(frame.resultSlot, result);
        return;
    }
    if (frame.sequence == nullptr)
    {
        frame.sequence = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            frame.first != nullptr ? frame.first->range() : parser_.current().range());
        if (frame.first != nullptr)
            frame.sequence->addChild(parser_.makeExpressionStatement(frame.first));
    }
    parser_.consume();
    Work next{WorkKind::ContinueForExpressionList, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

ast::AstNode* Parser::parseWithContinuation(ContinuationRequestKind kind)
{
    if (activeContinuationCoordinator_ != nullptr)
        return activeContinuationCoordinator_->parse(kind);
    ParseContinuationCoordinator coordinator(*this);
    return coordinator.parse(kind);
}
}
