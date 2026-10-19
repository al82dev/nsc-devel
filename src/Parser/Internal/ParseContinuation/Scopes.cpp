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
void Parser::ParseContinuationCoordinator::startUsing(const Work& work)
{
    const std::size_t frameIndex = usingFrames_.size();
    usingFrames_.emplace_back();
    UsingFrame& frame = usingFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after using");
    if (parser_.canStartTypeAt(parser_.index_))
        frame.resourceType = parser_.parseType();
    else
        parser_.expect(TokenKind::Identifier, "resource type");
    if (parser_.at(TokenKind::Identifier))
        frame.resourceName = std::string(parser_.consume().lexeme());
    else
        parser_.expect(TokenKind::Identifier, "resource variable name");

    Work finish{WorkKind::FinishUsingInitializer, work.resultSlot};
    finish.frameIndex = frameIndex;
    if (parser_.expect(TokenKind::Equal, "'=' in using resource declaration"))
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    work_.push_back(finish);
}

// Materializes the canonical UsingStatement after the initializer and before its body.
void Parser::ParseContinuationCoordinator::finishUsingInitializer(const Work& work)
{
    UsingFrame& frame = usingFrames_[work.frameIndex];
    parser_.expect(TokenKind::RightParen, "')' after using resource declaration");
    frame.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingStatement,
        parser_.tokenRangeFrom(frame.beginIndex));
    frame.node->setTypeSyntax(frame.resourceType);
    frame.node->setText(std::move(frame.resourceName));
    frame.node->setFirst(childValue(work));
    scheduleUsingBody(work.frameIndex);
}

// Requires the historical braced body and publishes malformed headers without blocking drain.
void Parser::ParseContinuationCoordinator::scheduleUsingBody(std::size_t frameIndex)
{
    UsingFrame& frame = usingFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after using declaration");
        completeUsing(frame, nullptr);
        return;
    }
    Work finish{WorkKind::FinishUsingBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed body and publishes the resource statement.
void Parser::ParseContinuationCoordinator::finishUsingBody(const Work& work)
{
    completeUsing(usingFrames_[work.frameIndex], childValue(work));
}

// Preserves the full historical range and AST field layout for using.
void Parser::ParseContinuationCoordinator::completeUsing(
    UsingFrame& frame,
    ast::AstNode* body)
{
    frame.node->setSecond(body);
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Opens one defer frame and parses its bounded capture list without nested parser reentry.
void Parser::ParseContinuationCoordinator::startDefer(const Work& work)
{
    const std::size_t frameIndex = deferFrames_.size();
    deferFrames_.push_back({parser_.index_, work.resultSlot, parser_.current().range(), {}});
    DeferFrame& frame = deferFrames_.back();
    parser_.consume();
    parser_.expect(TokenKind::LeftBracket, "'[' after defer");
    parseDeferCaptures(frame);
    parser_.expect(TokenKind::RightBracket, "']' after defer captures");
    scheduleDeferBody(frameIndex);
}

// Preserves source-order capture parsing and the historical duplicate-name diagnostic.
void Parser::ParseContinuationCoordinator::parseDeferCaptures(DeferFrame& frame)
{
    std::unordered_set<std::string> names;
    if (parser_.at(TokenKind::RightBracket)) return;
    while (true)
    {
        parseDeferCapture(frame, names);
        if (!parser_.consumeIf(TokenKind::Comma)) return;
    }
}

// Parses one explicit ref/move capture, including receiver and malformed-name recovery.
void Parser::ParseContinuationCoordinator::parseDeferCapture(
    DeferFrame& frame,
    std::unordered_set<std::string>& names)
{
    DeferCapture capture;
    if (parser_.atContextualKeyword("ref"))
    {
        capture.byReference = true;
        parser_.consume();
    }
    else if (parser_.at(TokenKind::KwMove) || parser_.atContextualKeyword("move"))
        parser_.consume();
    else
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "defer capture requires explicit 'ref' or 'move'");

    capture.range = parser_.current().range();
    if (parser_.at(TokenKind::KwThis))
    {
        capture.receiver = true;
        capture.name = "this";
        parser_.consume();
        if (!capture.byReference)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                capture.range,
                "defer can capture this only by reference");
    }
    else if (parser_.at(TokenKind::Identifier))
        capture.name = std::string(parser_.consume().lexeme());
    else
    {
        parser_.expect(TokenKind::Identifier, "defer capture name");
        capture.name = "__invalid_defer_capture";
    }
    if (!names.insert(capture.name).second)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            capture.range,
            "duplicate defer capture '" + capture.name + "'");
    frame.captures.push_back(std::move(capture));
}

// Schedules the braced cleanup body or completes the canonical malformed-body shape.
void Parser::ParseContinuationCoordinator::scheduleDeferBody(std::size_t frameIndex)
{
    DeferFrame& frame = deferFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after defer capture list");
        ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, frame.deferRange);
        complete(frame.resultSlot, lowerDeferRegistration(frame, body));
        return;
    }
    Work finish{WorkKind::FinishDeferBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Lowers the completed cleanup block and publishes it to the enclosing block continuation.
void Parser::ParseContinuationCoordinator::finishDeferBody(const Work& work)
{
    DeferFrame& frame = deferFrames_[work.frameIndex];
    complete(frame.resultSlot, lowerDeferRegistration(frame, childValue(work)));
}

// Records the exact capture boundary contract consumed by body binding.
void Parser::ParseContinuationCoordinator::configureDeferBody(
    const DeferFrame& frame,
    ast::AstNode* body)
{
    std::string allowedNames;
    for (std::size_t index = 0; index < frame.captures.size(); ++index)
    {
        if (index != 0) allowedNames.push_back(',');
        allowedNames += frame.captures[index].name;
    }
    body->setAuxiliaryText("defer-capture-boundary");
    body->setText(std::move(allowedNames));
}

// Builds the canonical inferred local type used by defer capture temporaries.
ast::TypeSyntax* Parser::ParseContinuationCoordinator::makeDeferVarType(SourceRange range)
{
    return parser_.arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVar, std::string("var"));
}

// Builds the canonical move expression for source and hidden defer values.
ast::AstNode* Parser::ParseContinuationCoordinator::makeDeferMoveExpression(
    SourceRange range,
    std::string name)
{
    ast::AstNode* move = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, range);
    move->setFirst(parser_.makeIdentifierExpression(range, std::move(name)));
    move->setAuxiliaryText("defer-value-capture");
    return move;
}

// Materializes one capture's outer snapshot and cleanup-local binding in source order.
void Parser::ParseContinuationCoordinator::addDeferCaptureLowering(
    const DeferCapture& capture,
    std::size_t deferOrdinal,
    ast::AstNode* registration,
    ast::AstNode* cleanup)
{
    if (capture.receiver) return;
    const std::string hidden = "__nsc_defer_capture_" +
        std::to_string(deferOrdinal) + "_" + capture.name;
    if (!capture.byReference)
    {
        ast::AstNode* outer = parser_.makeVariableDeclaration(
            makeDeferVarType(capture.range), hidden,
            makeDeferMoveExpression(capture.range, capture.name), false);
        outer->setAuxiliaryText("local:defer-value-capture");
        registration->addChild(outer);
    }
    ast::AstNode* initializer = capture.byReference
        ? parser_.makeIdentifierExpression(capture.range, capture.name)
        : makeDeferMoveExpression(capture.range, hidden);
    ast::AstNode* inner = parser_.makeVariableDeclaration(
        makeDeferVarType(capture.range), capture.name, initializer, false);
    inner->setAuxiliaryText(
        capture.byReference ? "local:ref" : "local:defer-value-capture");
    cleanup->addChild(inner);
}

// Recreates the historical registration block and nested try/finally lowering exactly once.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerDeferRegistration(
    DeferFrame& frame,
    ast::AstNode* body)
{
    configureDeferBody(frame, body);
    const std::size_t deferOrdinal = parser_.nextDeferTemporary_++;
    ast::AstNode* registration = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, frame.deferRange);
    registration->setAuxiliaryText("defer-registration");
    ast::AstNode* cleanup = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, body->range());
    for (const DeferCapture& capture : frame.captures)
        addDeferCaptureLowering(capture, deferOrdinal, registration, cleanup);
    cleanup->addChild(body);

    ast::AstNode* protectedBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, frame.deferRange);
    ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement, parser_.tokenRangeFrom(frame.beginIndex));
    lowered->setFirst(protectedBlock);
    lowered->setSecond(cleanup);
    lowered->setAuxiliaryText("defer-lowered");
    registration->addChild(lowered);
    registration->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    return registration;
}

// Parses the bounded fp header and schedules its body on the shared worklist.
void Parser::ParseContinuationCoordinator::startFloatingPointScope(const Work& work)
{
    parser_.consume();
    parser_.consume();
    FloatingPointScopeFrame frame;
    frame.resultSlot = work.resultSlot;
    if (parser_.atContextualKeyword("strict"))
        frame.mode = ast::FloatingPointModeSyntaxKind::Strict;
    else if (parser_.atContextualKeyword("precise"))
        frame.mode = ast::FloatingPointModeSyntaxKind::Precise;
    else if (parser_.atContextualKeyword("fast"))
        frame.mode = ast::FloatingPointModeSyntaxKind::Fast;
    else
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "floating-point scope mode must be strict, precise, or fast");
    if (frame.mode != ast::FloatingPointModeSyntaxKind::Inherit)
        frame.modeName = std::string(parser_.consume().lexeme());
    parser_.expect(TokenKind::RightParen, "')' after floating-point scope mode");
    const std::size_t frameIndex = floatingPointScopeFrames_.size();
    floatingPointScopeFrames_.push_back(std::move(frame));
    Work finish{WorkKind::FinishFloatingPointScope, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, childSlot});
}

// Applies the inherited mode after the complete block is available.
void Parser::ParseContinuationCoordinator::finishFloatingPointScope(const Work& work)
{
    FloatingPointScopeFrame& frame = floatingPointScopeFrames_[work.frameIndex];
    ast::AstNode* block = childValue(work);
    block->setAuxiliaryText("fp-scope:" + frame.modeName);
    parser_.applyMemberFloatingPointMode(block, frame.mode);
    complete(frame.resultSlot, block);
}

// Parses the optional unsafe reason and schedules the body on the shared worklist.
void Parser::ParseContinuationCoordinator::startUnsafeScope(const Work& work)
{
    parser_.consume();
    UnsafeScopeFrame frame;
    frame.resultSlot = work.resultSlot;
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        if (parser_.at(TokenKind::StringLiteral))
            frame.reason = std::string(parser_.consume().lexeme());
        else
            parser_.expect(TokenKind::StringLiteral, "unsafe block reason string");
        parser_.expect(TokenKind::RightParen, "')' after unsafe block reason");
    }
    const std::size_t frameIndex = unsafeScopeFrames_.size();
    unsafeScopeFrames_.push_back(std::move(frame));
    Work finish{WorkKind::FinishUnsafeScope, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, childSlot});
}

// Restores unsafe annotations and the historical breadth warning after its body finishes.
void Parser::ParseContinuationCoordinator::finishUnsafeScope(const Work& work)
{
    UnsafeScopeFrame& frame = unsafeScopeFrames_[work.frameIndex];
    ast::AstNode* block = childValue(work);
    block->setAuxiliaryText("unsafe-block");
    block->setText(frame.reason);
    if (language::unsafeBlockTooBroad(block->children().size(), !frame.reason.empty()))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::NS3030MissingEffectContext,
            diagnostics::DiagnosticSeverity::Warning,
            block->range(),
            "unsafe block covers more than eight statements; narrow it or write unsafe(\"reason\") { ... }");
    complete(frame.resultSlot, block);
}

// Parses the linear asm header/body before operand expressions enter the shared worklist.
void Parser::ParseContinuationCoordinator::startInlineAssembly(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    parser_.consume();
    std::string targetMetadata;
    std::string dialectMetadata;
    parser_.parseInlineAssemblyHeader(targetMetadata, dialectMetadata);
    parser_.expect(TokenKind::LeftBrace, "'{' after asm");
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::InlineAssemblyStatement,
        parser_.tokenRangeFrom(beginIndex));
    if (!targetMetadata.empty() || !dialectMetadata.empty())
        statement->setAuxiliaryText(
            "target:" + targetMetadata + ";dialect:" + dialectMetadata);
    std::string exactBody;
    parser_.parseInlineAssemblyBody(statement, exactBody);
    parser_.expect(TokenKind::RightBrace, "'}' to close asm block");
    statement->setRange(parser_.tokenRangeFrom(beginIndex));
    statement->setText(std::move(exactBody));
    parser_.validateInlineAssemblyControlFlow(statement);
    const std::size_t frameIndex = inlineAssemblyFrames_.size();
    inlineAssemblyFrames_.push_back({beginIndex, work.resultSlot, statement});
    Work next{WorkKind::ContinueInlineAssembly, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Selects the next operand clause or publishes the completed inline-assembly statement.
void Parser::ParseContinuationCoordinator::continueInlineAssembly(const Work& work)
{
    InlineAssemblyFrame& frame = inlineAssemblyFrames_[work.frameIndex];
    if (parser_.atContextualKeyword("out") || parser_.at(TokenKind::KwIn))
    {
        const bool output = parser_.atContextualKeyword("out");
        frame.hasOperandClauses = true;
        parser_.consume();
        if (!parser_.expect(TokenKind::LeftParen,
                output ? "'(' after asm out" : "'(' after asm in"))
        {
            work_.push_back(work);
            return;
        }
        if (!parser_.at(TokenKind::RightParen))
            return scheduleInlineAssemblyOperand(work.frameIndex, output, parser_.index_);
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            output ? "asm out clause requires at least one output target" :
                "asm in clause requires at least one input expression");
        parser_.consume();
        work_.push_back(work);
        return;
    }
    if (parser_.atContextualKeyword("clobber"))
    {
        frame.hasOperandClauses = true;
        parser_.parseInlineAssemblyClobberClause(frame.statement);
        work_.push_back(work);
        return;
    }
    completeInlineAssembly(frame);
}

// Schedules one output-unary or input-assignment expression with stable operand metadata.
void Parser::ParseContinuationCoordinator::scheduleInlineAssemblyOperand(
    std::size_t frameIndex,
    bool output,
    std::size_t operandBegin)
{
    InlineAssemblyFrame& frame = inlineAssemblyFrames_[frameIndex];
    Work finish{
        output ? WorkKind::FinishInlineAssemblyOutput : WorkKind::FinishInlineAssemblyInput,
        frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.beginIndex = operandBegin;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({output ? WorkKind::UnaryExpression : WorkKind::Expression, childSlot});
}

// Attaches one output operand and either repeats the clause or resumes clause discovery.
void Parser::ParseContinuationCoordinator::finishInlineAssemblyOutput(const Work& work)
{
    InlineAssemblyFrame& frame = inlineAssemblyFrames_[work.frameIndex];
    appendInlineAssemblyOperand(frame, childValue(work), true, work.beginIndex);
    if (parser_.consumeIf(TokenKind::Comma))
        scheduleInlineAssemblyOperand(work.frameIndex, true, parser_.index_);
    else
    {
        parser_.expect(TokenKind::RightParen, "')' after asm output targets");
        Work next{WorkKind::ContinueInlineAssembly, frame.resultSlot};
        next.frameIndex = work.frameIndex;
        work_.push_back(std::move(next));
    }
}

// Attaches one input operand and either repeats the clause or resumes clause discovery.
void Parser::ParseContinuationCoordinator::finishInlineAssemblyInput(const Work& work)
{
    InlineAssemblyFrame& frame = inlineAssemblyFrames_[work.frameIndex];
    appendInlineAssemblyOperand(frame, childValue(work), false, work.beginIndex);
    if (parser_.consumeIf(TokenKind::Comma))
        scheduleInlineAssemblyOperand(work.frameIndex, false, parser_.index_);
    else
    {
        parser_.expect(TokenKind::RightParen, "')' after asm input operands");
        Work next{WorkKind::ContinueInlineAssembly, frame.resultSlot};
        next.frameIndex = work.frameIndex;
        work_.push_back(std::move(next));
    }
}

// Recreates one historical operand node after its expression continuation completes.
void Parser::ParseContinuationCoordinator::appendInlineAssemblyOperand(
    InlineAssemblyFrame& frame,
    ast::AstNode* value,
    bool output,
    std::size_t operandBegin)
{
    ast::AstNode* operand = parser_.arena_.make<ast::AstNode>(
        output ? ast::AstNodeKind::InlineAssemblyOutputOperand :
            ast::AstNodeKind::InlineAssemblyInputOperand,
        parser_.tokenRangeFrom(operandBegin));
    operand->setFirst(value);
    operand->setIntegerValue(output ? frame.outputOrdinal++ : frame.inputOrdinal++);
    bool explicitConstraint = false;
    operand->setAuxiliaryText(
        parser_.parseInlineAssemblyConstraint(output, explicitConstraint));
    operand->setFlag(explicitConstraint);
    operand->setRange(parser_.tokenRangeFrom(operandBegin));
    frame.statement->addChild(operand);
}

// Preserves the optional clause semicolon, final range, and placeholder validation order.
void Parser::ParseContinuationCoordinator::completeInlineAssembly(InlineAssemblyFrame& frame)
{
    if (frame.hasOperandClauses)
    {
        parser_.expect(TokenKind::Semicolon, "';' after asm operand clauses");
        frame.statement->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    }
    parser_.validateInlineAssemblyPlaceholders(frame.statement);
    complete(frame.resultSlot, frame.statement);
}

// Copies comprehension context before scheduling the optional yielded expression.
void Parser::ParseContinuationCoordinator::startComprehensionYield(const Work& work)
{
    ComprehensionYieldFrame frame;
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    frame.yieldRange = parser_.consume().range();
    frame.builderName = parser_.comprehensionContexts_.back().builderName;
    frame.loopLabel = parser_.comprehensionContexts_.back().loopLabel;
    if (parser_.consumeIf(TokenKind::KwBreak))
    {
        parser_.expect(TokenKind::Semicolon, "';' after 'yield break'");
        ast::AstNode* stop = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BreakStatement,
            parser_.tokenRangeFrom(frame.beginIndex));
        stop->setText(frame.loopLabel);
        ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, frame.yieldRange);
        result->setFirst(parser_.makeIdentifierExpression(frame.yieldRange, frame.builderName));
        result->setAuxiliaryText("comprehension-result");
        stop->setFirst(result);
        complete(frame.resultSlot, stop);
        return;
    }
    parser_.consumeIf(TokenKind::KwReturn);
    const std::size_t frameIndex = comprehensionYieldFrames_.size();
    comprehensionYieldFrames_.push_back(std::move(frame));
    Work finish{WorkKind::FinishComprehensionYield, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Expression, childSlot});
}

// Recreates builder.Add(value) after the yielded expression is complete.
void Parser::ParseContinuationCoordinator::finishComprehensionYield(const Work& work)
{
    ComprehensionYieldFrame& frame = comprehensionYieldFrames_[work.frameIndex];
    parser_.expect(TokenKind::Semicolon, "';' after comprehension yield");
    ast::AstNode* add = parser_.makeZeroArgCallExpression(parser_.makeMemberExpression(
        parser_.makeIdentifierExpression(frame.yieldRange, frame.builderName), "Add"));
    if (ast::AstNode* value = childValue(work)) add->addChild(value);
    add->setAuxiliaryText("comprehension-builder-add");
    complete(frame.resultSlot, parser_.makeExpressionStatement(add));
}

ast::AstNode* Parser::parseWithContinuation(ContinuationRequestKind kind)
{
    if (activeContinuationCoordinator_ != nullptr)
        return activeContinuationCoordinator_->parse(kind);
    ParseContinuationCoordinator coordinator(*this);
    return coordinator.parse(kind);
}
}
