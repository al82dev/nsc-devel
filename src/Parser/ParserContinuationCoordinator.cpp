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
        FinishStaticAssert
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
        StaticAssert
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

    Parser& parser_;
    ParseContinuationCoordinator* previous_ = nullptr;
    std::deque<ResultSlot> resultSlots_;
    std::deque<BlockFrame> blockFrames_;
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

// Mirrors declaration disambiguation so value statements alone enter the B worklist.
bool Parser::ParseContinuationCoordinator::declarationStatementStarts() const noexcept
{
    if (parser_.at(TokenKind::KwVar) && parser_.peek().kind() == TokenKind::LeftParen) return true;
    if (parser_.looksLikeLocalFunction() || parser_.atContextualKeyword("await")) return true;
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

// Reproduces the canonical dispatcher order while isolating the simple B-owned statements.
Parser::ParseContinuationCoordinator::StatementKind
Parser::ParseContinuationCoordinator::classifyStatement() const
{
    if (parser_.at(TokenKind::LeftBrace)) return StatementKind::Block;
    if (scopedStatementStarts() || controlStatementStarts()) return StatementKind::Direct;
    if (parser_.atContextualKeyword("yield"))
        return parser_.comprehensionContexts_.empty() ? StatementKind::Yield : StatementKind::Direct;
    if (parser_.at(TokenKind::KwStatic)) return StatementKind::Direct;
    if (parser_.at(TokenKind::KwBreak) || parser_.at(TokenKind::KwContinue)) return StatementKind::Jump;
    if (parser_.atContextualKeyword("must_tail")) return StatementKind::MustTailReturn;
    if (parser_.at(TokenKind::KwReturn)) return StatementKind::Return;
    if (parser_.atContextualKeyword("defer") || parser_.at(TokenKind::KwTry) ||
        parser_.at(TokenKind::KwUsing)) return StatementKind::Direct;
    if (parser_.at(TokenKind::KwThrow)) return StatementKind::Throw;
    if (parser_.at(TokenKind::KwStaticAssert)) return StatementKind::StaticAssert;
    if (declarationStatementStarts()) return StatementKind::Direct;
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
    parser_.expect(TokenKind::Semicolon, "';'");
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

ast::AstNode* Parser::parseWithContinuation(ContinuationRequestKind kind)
{
    if (activeContinuationCoordinator_ != nullptr)
        return activeContinuationCoordinator_->parse(kind);
    ParseContinuationCoordinator coordinator(*this);
    return coordinator.parse(kind);
}
}
