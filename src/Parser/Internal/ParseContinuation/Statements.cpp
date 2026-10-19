#include "../ParseContinuationCoordinator.h"

#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;
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
    if (parser_.atContextualKeyword("let") ||
        ((parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile)) &&
         parser_.peek().kind() == TokenKind::LeftParen &&
         parser_.peek(2).kind() == TokenKind::Identifier &&
         parser_.peek(2).lexeme() == "let")) return StatementKind::ConditionalPattern;
    if (ordinaryIfStarts()) return StatementKind::If;
    if (ordinaryWhileStarts()) return StatementKind::While;
    if (parser_.at(TokenKind::KwDo)) return StatementKind::DoWhile;
    if (parser_.at(TokenKind::KwFor)) return StatementKind::For;
    if (parser_.at(TokenKind::KwForeach)) return StatementKind::Foreach;
    if (parser_.at(TokenKind::KwSwitch)) return StatementKind::Switch;
    if (parser_.startsParenthesizedBlock("match")) return StatementKind::Match;
    if (parser_.at(TokenKind::KwTry)) return StatementKind::Try;
    if (parser_.at(TokenKind::KwUsing)) return StatementKind::Using;
    if (parser_.atContextualKeyword("fp") && parser_.peek().kind() == TokenKind::LeftParen)
        return StatementKind::FloatingPointScope;
    if (parser_.atContextualKeyword("unsafe") &&
        (parser_.peek().kind() == TokenKind::LeftBrace ||
         parser_.peek().kind() == TokenKind::LeftParen)) return StatementKind::UnsafeScope;
    if (parser_.atContextualKeyword("asm") &&
        (parser_.peek().kind() == TokenKind::LeftBrace ||
         parser_.peek().kind() == TokenKind::LeftParen)) return StatementKind::InlineAssembly;
    if (parser_.atContextualKeyword("yield"))
        return parser_.comprehensionContexts_.empty()
            ? StatementKind::Yield : StatementKind::ComprehensionYield;
    if (parser_.at(TokenKind::KwStatic))
        return parser_.looksLikeLocalFunction()
            ? StatementKind::LocalFunction : StatementKind::StaticLocal;
    if (parser_.at(TokenKind::KwBreak) || parser_.at(TokenKind::KwContinue)) return StatementKind::Jump;
    if (parser_.atContextualKeyword("must_tail")) return StatementKind::MustTailReturn;
    if (parser_.at(TokenKind::KwReturn)) return StatementKind::Return;
    if (parser_.atContextualKeyword("defer")) return StatementKind::Defer;
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
    case StatementKind::Foreach: startForeach(work); return;
    case StatementKind::ConditionalPattern: startConditionalPattern(work); return;
    case StatementKind::Switch: startSwitch(work); return;
    case StatementKind::Match: startMatch(work); return;
    case StatementKind::Try: startTry(work); return;
    case StatementKind::Using: startUsing(work); return;
    case StatementKind::Defer: startDefer(work); return;
    case StatementKind::FloatingPointScope: startFloatingPointScope(work); return;
    case StatementKind::UnsafeScope: startUnsafeScope(work); return;
    case StatementKind::InlineAssembly: startInlineAssembly(work); return;
    case StatementKind::ComprehensionYield: startComprehensionYield(work); return;
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
}
