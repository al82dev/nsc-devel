#include "StructuredControlParser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
void reportDeferredLanguage3(
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange range,
    language::Language3Feature feature)
{
    diagnostics.report(
        diagnostics::DiagnosticId::NS3049DeferredLanguageSurface,
        diagnostics::DiagnosticSeverity::Error,
        range,
        language::language3SurfaceDiagnostic(feature));
}
}

Parser::StructuredControlParser::StructuredControlParser(Parser& parser) noexcept
    : parser_(parser)
{}

ast::AstNode* Parser::StructuredControlParser::parse()
{
        work_.push_back({WorkKind::ParseControl, 0});
        execute();
        return values_.empty() ? nullptr : values_.back();
    }

void Parser::StructuredControlParser::execute()
{
        while (!work_.empty())
        {
            const Work current = work_.back();
            work_.pop_back();
            dispatch(current);
        }
    }

bool Parser::StructuredControlParser::ordinaryIfAtCursor() const noexcept
{
        const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return parser_.at(TokenKind::KwIf) && !conditional;
    }

bool Parser::StructuredControlParser::ordinaryWhileAtCursor() const noexcept
{
        const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return parser_.at(TokenKind::KwWhile) && !conditional;
    }

bool Parser::StructuredControlParser::conditionalPatternAtCursor() const noexcept
{
        const bool conditionalControl =
            (parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile)) &&
            parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return conditionalControl || parser_.atContextualKeyword("let");
    }

bool Parser::StructuredControlParser::structuredControlAtCursor() const noexcept
{
        return ordinaryIfAtCursor() || ordinaryWhileAtCursor() ||
            parser_.at(TokenKind::KwDo) || parser_.at(TokenKind::KwFor) ||
            parser_.at(TokenKind::KwTry) || parser_.at(TokenKind::KwUsing) ||
            parser_.atContextualKeyword("defer") ||
            parser_.at(TokenKind::KwForeach) ||
            conditionalPatternAtCursor() ||
            parser_.at(TokenKind::KwSwitch) ||
            parser_.startsParenthesizedBlock("match");
    }

ast::AstNode* Parser::StructuredControlParser::wrapEmbedded(ast::AstNode* statement)
{
        if (statement != nullptr &&
            (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
             statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement ||
             (statement->kind() == ast::AstNodeKind::BlockStatement &&
              statement->auxiliaryText() == "conditional-pattern-inline")))
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "a declaration used as an if or else body requires an explicit '{ ... }' block");
        }
        const SourceRange range = statement != nullptr
            ? statement->range()
            : parser_.current().range();
        ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, range);
        if (statement != nullptr) block->addChild(statement);
        return block;
    }

void Parser::StructuredControlParser::dispatch(const Work& current)
{
        if (current.kind <= WorkKind::ParseBlock)
        {
            dispatchParse(current);
            return;
        }
        if (current.kind <= WorkKind::FinishForeach)
        {
            dispatchFinish(current);
            return;
        }
        dispatchContinuation(current);
    }

void Parser::StructuredControlParser::dispatchParse(const Work& current)
{
        switch (current.kind)
        {
        case WorkKind::ParseControl: parseControl(); break;
        case WorkKind::ParseIf: parseIf(); break;
        case WorkKind::ParseWhile: parseWhile(); break;
        case WorkKind::ParseDoWhile: parseDoWhile(); break;
        case WorkKind::ParseFor: parseFor(); break;
        case WorkKind::ParseUsing: parseUsing(); break;
        case WorkKind::ParseDefer: parseDefer(); break;
        case WorkKind::ParseForeach: parseForeach(); break;
        case WorkKind::ParseConditionalPattern: parseConditionalPattern(); break;
        case WorkKind::ParseSwitch: parseSwitch(); break;
        case WorkKind::ParseMatch: parseMatch(); break;
        case WorkKind::ParseEmbedded: parseEmbedded(); break;
        case WorkKind::WrapEmbedded: finishWrapEmbedded(); break;
        case WorkKind::ParseBlock: parseBlock(); break;
        default: break;
        }
    }

void Parser::StructuredControlParser::dispatchFinish(const Work& current)
{
        switch (current.kind)
        {
        case WorkKind::ContinueBlock: continueBlock(current); break;
        case WorkKind::AppendBlock: appendBlock(current.stateIndex); break;
        case WorkKind::FinishThen: finishThen(current.stateIndex); break;
        case WorkKind::FinishElse: finishElse(current.stateIndex); break;
        case WorkKind::FinishWhile: finishWhile(current.stateIndex); break;
        case WorkKind::FinishDoWhile: finishDoWhile(current.stateIndex); break;
        case WorkKind::FinishFor: finishFor(current.stateIndex); break;
        case WorkKind::FinishUsing: finishUsing(current.stateIndex, popValue()); break;
        case WorkKind::FinishDefer: finishDefer(current.stateIndex, popValue()); break;
        case WorkKind::FinishForeach: finishForeach(current.stateIndex, popValue()); break;
        default: break;
        }
    }

void Parser::StructuredControlParser::dispatchContinuation(const Work& current)
{
        switch (current.kind)
        {
        case WorkKind::FinishConditionalPatternSuccess:
            finishConditionalPatternSuccess(current.stateIndex, popValue()); break;
        case WorkKind::FinishConditionalPatternFailure:
            finishConditionalPatternFailure(current.stateIndex, popValue()); break;
        case WorkKind::FinishConditionalPatternWhile:
            finishConditionalPatternWhile(current.stateIndex, popValue()); break;
        case WorkKind::FinishConditionalPatternLetElse:
            finishConditionalPatternLetElse(current.stateIndex, popValue()); break;
        case WorkKind::ContinueSwitch: continueSwitch(current.stateIndex); break;
        case WorkKind::ContinueSwitchSection: continueSwitchSection(current); break;
        case WorkKind::AppendSwitchStatement: appendSwitchStatement(current.stateIndex); break;
        case WorkKind::ContinueMatch: continueMatch(current.stateIndex); break;
        case WorkKind::FinishMatchArm: finishMatchArm(current.stateIndex, popValue()); break;
        default: break;
        }
    }

void Parser::StructuredControlParser::parseControl()
{
        if (ordinaryIfAtCursor()) work_.push_back({WorkKind::ParseIf, 0});
        else if (ordinaryWhileAtCursor()) work_.push_back({WorkKind::ParseWhile, 0});
        else if (parser_.at(TokenKind::KwDo)) work_.push_back({WorkKind::ParseDoWhile, 0});
        else if (parser_.at(TokenKind::KwFor)) work_.push_back({WorkKind::ParseFor, 0});
        else if (parser_.at(TokenKind::KwTry))
            values_.push_back(parser_.parseTryStatement());
        else if (parser_.at(TokenKind::KwUsing)) work_.push_back({WorkKind::ParseUsing, 0});
        else if (parser_.atContextualKeyword("defer"))
            work_.push_back({WorkKind::ParseDefer, 0});
        else if (parser_.at(TokenKind::KwForeach))
            work_.push_back({WorkKind::ParseForeach, 0});
        else if (conditionalPatternAtCursor())
            work_.push_back({WorkKind::ParseConditionalPattern, 0});
        else if (parser_.at(TokenKind::KwSwitch)) work_.push_back({WorkKind::ParseSwitch, 0});
        else if (parser_.startsParenthesizedBlock("match"))
            work_.push_back({WorkKind::ParseMatch, 0});
        else values_.push_back(parser_.parseStatement());
    }

void Parser::StructuredControlParser::parseIf()
{
        const std::size_t index = ifStates_.size();
        ifStates_.emplace_back();
        parser_.parseIfHeader(ifStates_.back());
        work_.push_back({WorkKind::FinishThen, index});
        work_.push_back({WorkKind::ParseEmbedded, 0});
    }

void Parser::StructuredControlParser::parseWhile()
{
        const std::size_t index = whileStates_.size();
        whileStates_.emplace_back();
        parser_.parseWhileHeader(whileStates_.back());
        if (!parser_.at(TokenKind::LeftBrace))
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after while condition");
            values_.push_back(parser_.lowerWhileStatement(whileStates_[index], nullptr));
            return;
        }
        work_.push_back({WorkKind::FinishWhile, index});
        work_.push_back({WorkKind::ParseBlock, 0});
    }

void Parser::StructuredControlParser::parseDoWhile()
{
        const std::size_t index = doWhileStates_.size();
        doWhileStates_.push_back({parser_.index_});
        parser_.consume();
        if (!parser_.at(TokenKind::LeftBrace))
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after 'do'");
            values_.push_back(parser_.lowerDoWhileStatement(doWhileStates_[index], nullptr));
            return;
        }
        work_.push_back({WorkKind::FinishDoWhile, index});
        work_.push_back({WorkKind::ParseBlock, 0});
    }

void Parser::StructuredControlParser::parseFor()
{
        const std::size_t index = forStates_.size();
        forStates_.emplace_back();
        parser_.parseForHeader(forStates_.back());
        if (!parser_.at(TokenKind::LeftBrace))
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after for clause");
            values_.push_back(parser_.lowerForStatement(forStates_[index], nullptr));
            return;
        }
        work_.push_back({WorkKind::FinishFor, index});
        work_.push_back({WorkKind::ParseBlock, 0});
    }

void Parser::StructuredControlParser::parseUsing()
{
        const std::size_t index = usingStates_.size();
        usingStates_.push_back({parser_.index_});
        UsingParseState& state = usingStates_.back();
        parser_.consume();
        parser_.expect(TokenKind::LeftParen, "'(' after using");

        ast::TypeSyntax* resourceType = nullptr;
        if (parser_.canStartTypeAt(parser_.index_))
            resourceType = parser_.parseType();
        else
            parser_.expect(TokenKind::Identifier, "resource type");

        std::string resourceName;
        if (parser_.at(TokenKind::Identifier))
            resourceName = std::string(parser_.consume().lexeme());
        else
            parser_.expect(TokenKind::Identifier, "resource variable name");

        ast::AstNode* initializer = nullptr;
        if (parser_.expect(TokenKind::Equal, "'=' in using resource declaration"))
            initializer = parser_.parseExpression();
        parser_.expect(TokenKind::RightParen, "')' after using resource declaration");

        state.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UsingStatement,
            parser_.tokenRangeFrom(state.beginIndex));
        state.node->setTypeSyntax(resourceType);
        state.node->setText(std::move(resourceName));
        state.node->setFirst(initializer);
        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishUsing, index});
            work_.push_back({WorkKind::ParseBlock, 0});
        }
        else
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after using declaration");
            finishUsing(index, nullptr);
        }
    }

void Parser::StructuredControlParser::finishUsing(std::size_t index, ast::AstNode* body)
{
        UsingParseState& state = usingStates_[index];
        state.node->setSecond(body);
        state.node->setRange(parser_.tokenRangeFrom(state.beginIndex));
        values_.push_back(state.node);
    }

void Parser::StructuredControlParser::parseDefer()
{
        const std::size_t index = deferStates_.size();
        deferStates_.push_back({parser_.index_, parser_.current().range(), {}});
        DeferParseState& state = deferStates_.back();
        parser_.consume();
        std::unordered_set<std::string> names;

        parser_.expect(TokenKind::LeftBracket, "'[' after defer");
        if (!parser_.at(TokenKind::RightBracket))
        {
            while (true)
            {
                bool byReference = false;
                if (parser_.atContextualKeyword("ref"))
                {
                    byReference = true;
                    parser_.consume();
                }
                else if (parser_.at(TokenKind::KwMove) || parser_.atContextualKeyword("move"))
                {
                    parser_.consume();
                }
                else
                {
                    parser_.diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedToken,
                        diagnostics::DiagnosticSeverity::Error,
                        parser_.current().range(),
                        "defer capture requires explicit 'ref' or 'move'");
                }

                DeferCapture capture;
                capture.byReference = byReference;
                capture.range = parser_.current().range();
                if (parser_.at(TokenKind::KwThis))
                {
                    capture.receiver = true;
                    capture.name = "this";
                    parser_.consume();
                    if (!byReference)
                    {
                        parser_.diagnostics_.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            capture.range,
                            "defer can capture this only by reference");
                    }
                }
                else if (parser_.at(TokenKind::Identifier))
                {
                    capture.name = std::string(parser_.consume().lexeme());
                }
                else
                {
                    parser_.expect(TokenKind::Identifier, "defer capture name");
                    capture.name = "__invalid_defer_capture";
                }

                if (!names.insert(capture.name).second)
                {
                    parser_.diagnostics_.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        capture.range,
                        "duplicate defer capture '" + capture.name + "'");
                }
                state.captures.push_back(std::move(capture));
                if (!parser_.consumeIf(TokenKind::Comma)) break;
            }
        }
        parser_.expect(TokenKind::RightBracket, "']' after defer captures");

        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishDefer, index});
            work_.push_back({WorkKind::ParseBlock, 0});
        }
        else
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after defer capture list");
            ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, state.deferRange);
            finishDefer(index, body);
        }
    }

void Parser::StructuredControlParser::finishDefer(std::size_t index, ast::AstNode* body)
{
        DeferParseState& state = deferStates_[index];
        std::string allowedNames;
        for (std::size_t captureIndex = 0; captureIndex < state.captures.size(); ++captureIndex)
        {
            if (captureIndex != 0) allowedNames.push_back(',');
            allowedNames += state.captures[captureIndex].name;
        }
        body->setAuxiliaryText("defer-capture-boundary");
        body->setText(std::move(allowedNames));

        const std::size_t deferOrdinal = parser_.nextDeferTemporary_++;
        const auto varType = [&](SourceRange range)
        {
            return parser_.arena_.make<ast::TypeSyntax>(
                range, TokenKind::KwVar, std::string("var"));
        };
        const auto moveExpression = [&](SourceRange range, std::string name)
        {
            ast::AstNode* move = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::MoveExpression, range);
            move->setFirst(parser_.makeIdentifierExpression(range, std::move(name)));
            move->setAuxiliaryText("defer-value-capture");
            return move;
        };

        ast::AstNode* registration = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, state.deferRange);
        registration->setAuxiliaryText("defer-registration");
        ast::AstNode* cleanup = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, body->range());

        for (const DeferCapture& capture : state.captures)
        {
            if (capture.receiver) continue;
            const std::string hidden = "__nsc_defer_capture_" +
                std::to_string(deferOrdinal) + "_" + capture.name;
            if (!capture.byReference)
            {
                ast::AstNode* outer = parser_.makeVariableDeclaration(
                    varType(capture.range), hidden,
                    moveExpression(capture.range, capture.name), false);
                outer->setAuxiliaryText("local:defer-value-capture");
                registration->addChild(outer);
            }

            ast::AstNode* innerInitializer = capture.byReference
                ? parser_.makeIdentifierExpression(capture.range, capture.name)
                : moveExpression(capture.range, hidden);
            ast::AstNode* inner = parser_.makeVariableDeclaration(
                varType(capture.range), capture.name, innerInitializer, false);
            inner->setAuxiliaryText(
                capture.byReference ? "local:ref" : "local:defer-value-capture");
            cleanup->addChild(inner);
        }
        cleanup->addChild(body);

        ast::AstNode* protectedBlock = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, state.deferRange);
        ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::TryStatement, parser_.tokenRangeFrom(state.beginIndex));
        lowered->setFirst(protectedBlock);
        lowered->setSecond(cleanup);
        lowered->setAuxiliaryText("defer-lowered");
        registration->addChild(lowered);
        registration->setRange(parser_.tokenRangeFrom(state.beginIndex));
        values_.push_back(registration);
    }

void Parser::StructuredControlParser::parseForeach()
{
        const std::size_t index = foreachStates_.size();
        foreachStates_.emplace_back();
        if (!parser_.parseForeachHeader(foreachStates_.back()))
        {
            values_.push_back(nullptr);
            return;
        }
        if (!parser_.at(TokenKind::LeftBrace))
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after foreach clause");
            values_.push_back(parser_.lowerForeachStatement(
                std::move(foreachStates_[index]), nullptr));
            return;
        }
        work_.push_back({WorkKind::FinishForeach, index});
        work_.push_back({WorkKind::ParseBlock, 0});
    }

void Parser::StructuredControlParser::finishForeach(std::size_t index, ast::AstNode* body)
{
        values_.push_back(parser_.lowerForeachStatement(
            std::move(foreachStates_[index]), body));
    }

void Parser::StructuredControlParser::parseConditionalPatternPrefix(ConditionalPatternParseState& state)
{
        state.beginIndex = parser_.index_;
        state.featureRange = parser_.current().range();
        if (parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile))
        {
            state.form = parser_.at(TokenKind::KwIf)
                ? ConditionalPatternForm::If
                : ConditionalPatternForm::While;
            state.featureRange = parser_.consume().range();
            parser_.expect(TokenKind::LeftParen, state.form == ConditionalPatternForm::If
                ? "'(' after 'if'"
                : "'(' after 'while'");
            parser_.expectContextualKeyword("let", "'let' in conditional pattern");
            return;
        }
        state.form = ConditionalPatternForm::LetElse;
        parser_.expectContextualKeyword("let", "'let'");
    }

void Parser::StructuredControlParser::parseConditionalPatternPayload(ConditionalPatternParseState& state)
{
        state.pattern = parser_.parsePattern(false);
        parser_.expect(TokenKind::Equal, "'=' between conditional pattern and source expression");
        state.sourceExpression = parser_.parseAssignmentExpression();
        if (state.form != ConditionalPatternForm::LetElse)
            parser_.expect(TokenKind::RightParen, "')' after conditional pattern");
    }

void Parser::StructuredControlParser::parseConditionalPatternHeader(ConditionalPatternParseState& state)
{
        parseConditionalPatternPrefix(state);
        parseConditionalPatternPayload(state);
        const Parser::ConditionalPatternAnalysis analysis =
            parser_.analyzeConditionalPattern(state.pattern);
        state.hasReferenceBinding = analysis.hasReferenceBinding;
        state.hasMoveBinding = analysis.hasMoveBinding;
    }

void Parser::StructuredControlParser::parseConditionalPattern()
{
        const std::size_t index = conditionalPatternStates_.size();
        conditionalPatternStates_.emplace_back();
        ConditionalPatternParseState& state = conditionalPatternStates_.back();
        parseConditionalPatternHeader(state);
        if (state.form == ConditionalPatternForm::If)
        {
            work_.push_back({WorkKind::FinishConditionalPatternSuccess, index});
            work_.push_back({WorkKind::ParseEmbedded, 0});
            return;
        }
        if (state.form == ConditionalPatternForm::While)
        {
            work_.push_back({WorkKind::FinishConditionalPatternWhile, index});
            work_.push_back({WorkKind::ParseEmbedded, 0});
            return;
        }

        parser_.expect(TokenKind::KwElse, "'else' after let pattern initializer");
        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishConditionalPatternLetElse, index});
            work_.push_back({WorkKind::ParseBlock, 0});
        }
        else
        {
            parser_.expect(TokenKind::LeftBrace, "'{' to open the divergent let-else body");
            finishConditionalPatternLetElse(index, nullptr);
        }
    }

void Parser::StructuredControlParser::finishConditionalPatternSuccess(std::size_t index, ast::AstNode* successBody)
{
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.successBody = successBody;
        if (!parser_.consumeIf(TokenKind::KwElse))
        {
            values_.push_back(lowerConditionalPattern(state));
            return;
        }
        work_.push_back({WorkKind::FinishConditionalPatternFailure, index});
        if (parser_.at(TokenKind::KwIf))
        {
            work_.push_back({conditionalPatternAtCursor()
                ? WorkKind::ParseConditionalPattern
                : WorkKind::ParseIf, 0});
        }
        else
        {
            work_.push_back({WorkKind::ParseEmbedded, 0});
        }
    }

void Parser::StructuredControlParser::finishConditionalPatternFailure(std::size_t index, ast::AstNode* failureBody)
{
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.failureBody = failureBody;
        values_.push_back(lowerConditionalPattern(state));
    }

void Parser::StructuredControlParser::finishConditionalPatternWhile(std::size_t index, ast::AstNode* successBody)
{
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.successBody = successBody;
        values_.push_back(lowerConditionalPattern(state));
    }

void Parser::StructuredControlParser::finishConditionalPatternLetElse(std::size_t index, ast::AstNode* failureBody)
{
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.failureBody = failureBody;
        if (!parser_.statementAlwaysDiverges(failureBody))
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                failureBody != nullptr ? failureBody->range() : state.featureRange,
                "the else body of a let-else statement must diverge with return, throw, break, "
                "continue, or an if whose branches both diverge");
        }
        parser_.expect(TokenKind::Semicolon, "';' after let-else statement");
        values_.push_back(lowerConditionalPattern(state));
    }

Parser::PatternExpressionFactory Parser::StructuredControlParser::makeConditionalPatternRoot(
    const ConditionalPatternLoweringContext& context)
{
        return [this, range = context.statementRange, temporary = context.temporary]
        {
            return parser_.makeIdentifierExpression(range, temporary);
        };
    }

void Parser::StructuredControlParser::addConditionalPatternPrelude(
    const ConditionalPatternParseState& state,
    const ConditionalPatternLoweringContext& context,
    ast::AstNode* destination)
{
        ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
            state.featureRange, TokenKind::KwVar, "var");
        ast::AstNode* declaration = parser_.makeVariableDeclaration(
            inferred, context.temporary, state.sourceExpression, false,
            !state.hasReferenceBinding && !state.hasMoveBinding);
        if (state.hasReferenceBinding) declaration->setAuxiliaryText("local:ref");
        destination->addChild(declaration);
        ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
            state.featureRange, TokenKind::KwBool, "bool");
        destination->addChild(parser_.makeVariableDeclaration(
            boolType,
            context.matched,
            parser_.makeBooleanLiteral(state.featureRange, false),
            false,
            false));
    }

ast::AstNode* Parser::StructuredControlParser::makeConditionalPatternSuccess(
    const ConditionalPatternParseState& state,
    const ConditionalPatternLoweringContext& context,
    bool includeBody)
{
        ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, context.statementRange);
        body->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
            parser_.makeIdentifierExpression(context.statementRange, context.matched),
            parser_.makeBooleanLiteral(context.statementRange, true))));
        if (includeBody && state.successBody != nullptr)
        {
            for (ast::AstNode* child : state.successBody->children()) body->addChild(child);
        }
        return body;
    }

ast::AstNode* Parser::StructuredControlParser::lowerConditionalPatternWhile(
    ConditionalPatternParseState& state,
    const ConditionalPatternLoweringContext& context)
{
        PatternExpressionFactory root = makeConditionalPatternRoot(context);
        ast::AstNode* iteration = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, context.statementRange);
        addConditionalPatternPrelude(state, context, iteration);
        iteration->addChild(parser_.lowerPattern(
            state.pattern,
            root,
            makeConditionalPatternSuccess(state, context, true),
            context.temporary));
        ast::AstNode* stopBody = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, context.statementRange);
        stopBody->addChild(parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BreakStatement, context.statementRange));
        ast::AstNode* stop = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, context.statementRange);
        stop->setFirst(makeConditionalPatternNotMatched(
            context.statementRange, context.matched));
        stop->setSecond(stopBody);
        iteration->addChild(stop);

        ast::AstNode* loop = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::WhileStatement, context.statementRange);
        loop->setFirst(parser_.makeBooleanLiteral(state.featureRange, true));
        loop->setSecond(iteration);
        return loop;
    }

ast::AstNode* Parser::StructuredControlParser::lowerConditionalPatternSelection(
    ConditionalPatternParseState& state,
    const ConditionalPatternLoweringContext& context)
{
        PatternExpressionFactory root = makeConditionalPatternRoot(context);
        ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, context.statementRange);
        addConditionalPatternPrelude(state, context, lowered);
        lowered->addChild(parser_.lowerPattern(
            state.pattern,
            root,
            makeConditionalPatternSuccess(
                state, context, state.form == ConditionalPatternForm::If),
            context.temporary,
            state.form != ConditionalPatternForm::LetElse));
        finishConditionalPatternLowering(state, root, lowered, context);
        return lowered;
    }

ast::AstNode* Parser::StructuredControlParser::lowerConditionalPattern(ConditionalPatternParseState& state)
{
        ConditionalPatternLoweringContext context;
        context.statementRange = parser_.tokenRangeFrom(state.beginIndex);
        context.temporary =
            "$__nsc_conditional_pattern_" + std::to_string(parser_.nextMatchTemporary_++);
        context.matched = context.temporary + "_matched";
        if (state.form == ConditionalPatternForm::While)
            return lowerConditionalPatternWhile(state, context);
        return lowerConditionalPatternSelection(state, context);
    }

ast::AstNode* Parser::StructuredControlParser::makeConditionalPatternNotMatched(
    SourceRange range,
    const std::string& matched)
{
        ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, range);
        result->setTokenKind(TokenKind::Bang);
        result->setFirst(parser_.makeIdentifierExpression(range, matched));
        return result;
    }

void Parser::StructuredControlParser::finishConditionalPatternLowering(
    ConditionalPatternParseState& state,
    const PatternExpressionFactory& root,
    ast::AstNode* lowered,
    const ConditionalPatternLoweringContext& context)
{
        if (state.failureBody != nullptr)
        {
            ast::AstNode* failure = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::IfStatement, context.statementRange);
            failure->setFirst(makeConditionalPatternNotMatched(
                context.statementRange, context.matched));
            failure->setSecond(state.failureBody);
            lowered->addChild(failure);
        }
        if (state.form != ConditionalPatternForm::LetElse) return;
        parser_.emitPatternBindings(state.pattern, root, lowered, context.temporary);
        lowered->setAuxiliaryText("conditional-pattern-inline");
    }

void Parser::StructuredControlParser::parseSwitch()
{
        const std::size_t index = switchStates_.size();
        switchStates_.emplace_back();
        SwitchParseState& state = switchStates_.back();
        state.beginIndex = parser_.index_;
        parser_.consume();
        parser_.expect(TokenKind::LeftParen, "'(' after 'switch'");
        ast::AstNode* governing = parseSwitchGoverning(state);
        parser_.expect(TokenKind::RightParen, "')' after the switch governing expression");
        parser_.expect(TokenKind::LeftBrace, "'{' to open the switch body");
        state.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SwitchStatement,
            parser_.tokenRangeFrom(state.beginIndex));
        state.node->setFirst(governing);
        work_.push_back({WorkKind::ContinueSwitch, index});
    }

ast::AstNode* Parser::StructuredControlParser::parseSwitchGoverning(SwitchParseState& state)
{
        if (!parser_.looksLikeConditionDeclaration()) return parser_.parseExpression();
        state.conditionDeclaration = parser_.parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            state.conditionDeclaration,
            parser_.diagnostics_);
        return binding != nullptr
            ? parser_.makeIdentifierExpression(binding->range(), binding->text())
            : nullptr;
    }

void Parser::StructuredControlParser::continueSwitch(std::size_t index)
{
        SwitchParseState& state = switchStates_[index];
        if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
        {
            values_.push_back(finishSwitch(state));
            return;
        }
        if (!parser_.atSwitchLabel())
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "expected 'case' or 'default' at the start of a switch section");
            parser_.consume();
            work_.push_back({WorkKind::ContinueSwitch, index});
            return;
        }
        startSwitchSection(index);
    }

void Parser::StructuredControlParser::startSwitchSection(std::size_t index)
{
        SwitchParseState& state = switchStates_[index];
        state.sectionBegin = parser_.index_;
        state.section = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SwitchSection,
            parser_.current().range());
        std::uint64_t labelCount = 0;
        while (parser_.atSwitchLabel())
        {
            appendSwitchLabel(state.section);
            ++labelCount;
        }
        state.section->setIntegerValue(labelCount);
        work_.push_back({WorkKind::ContinueSwitch, index});
        work_.push_back({WorkKind::ContinueSwitchSection, index});
    }

void Parser::StructuredControlParser::appendSwitchLabel(ast::AstNode* section)
{
        const std::size_t beginIndex = parser_.index_;
        const bool isDefault = parser_.at(TokenKind::KwDefault);
        parser_.consume();
        if (!isDefault && parser_.at(TokenKind::LeftBracket))
            reportDeferredLanguage3(
                parser_.diagnostics_,
                parser_.current().range(),
                language::Language3Feature::ListPatterns);
        ast::AstNode* value = isDefault ? nullptr : parser_.parseExpression();
        parser_.expect(
            TokenKind::Colon,
            isDefault ? "':' after 'default'" : "':' after the case value");
        ast::AstNode* label = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SwitchLabel,
            parser_.tokenRangeFrom(beginIndex));
        label->setFlag(isDefault);
        label->setFirst(value);
        section->addChild(label);
    }

void Parser::StructuredControlParser::continueSwitchSection(const Work& current)
{
        SwitchParseState& state = switchStates_[current.stateIndex];
        if (parser_.atSwitchLabel() || parser_.at(TokenKind::RightBrace) ||
            parser_.at(TokenKind::EndOfFile))
        {
            state.section->setRange(parser_.tokenRangeFrom(state.sectionBegin));
            state.node->addChild(state.section);
            return;
        }
        const std::size_t before = parser_.index_;
        work_.push_back(current);
        if (structuredControlAtCursor())
            pushSwitchStatement(current.stateIndex, WorkKind::ParseControl);
        else if (parser_.at(TokenKind::LeftBrace))
            pushSwitchStatement(current.stateIndex, WorkKind::ParseBlock);
        else
        {
            appendSwitchStatement(state, parser_.parseStatement());
            if (parser_.index_ == before) parser_.consume();
        }
    }

void Parser::StructuredControlParser::pushSwitchStatement(std::size_t index, WorkKind child)
{
        work_.push_back({WorkKind::AppendSwitchStatement, index});
        work_.push_back({child, 0});
    }

void Parser::StructuredControlParser::appendSwitchStatement(std::size_t index)
{
        appendSwitchStatement(switchStates_[index], popValue());
    }

void Parser::StructuredControlParser::appendSwitchStatement(SwitchParseState& state, ast::AstNode* statement)
{
        if (statement == nullptr) return;
        if (statement->kind() == ast::AstNodeKind::BlockStatement &&
            statement->auxiliaryText() == "conditional-pattern-inline")
        {
            for (ast::AstNode* child : statement->children()) state.section->addChild(child);
            return;
        }
        state.section->addChild(statement);
    }

ast::AstNode* Parser::StructuredControlParser::finishSwitch(SwitchParseState& state)
{
        parser_.expect(TokenKind::RightBrace, "'}' to close the switch");
        state.node->setRange(parser_.tokenRangeFrom(state.beginIndex));
        if (state.conditionDeclaration == nullptr) return state.node;
        ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            state.node->range());
        lowered->setAuxiliaryText("condition-declaration");
        lowered->addChild(state.conditionDeclaration);
        lowered->addChild(state.node);
        return lowered;
    }

// Consumes the statement header and schedules arm parsing without retaining this C++ frame.
void Parser::StructuredControlParser::parseMatch()
{
        const std::size_t index = matchStates_.size();
        matchStates_.emplace_back();
        MatchParseState& state = matchStates_.back();
        state.beginIndex = parser_.index_;
        state.keywordRange = parser_.consume().range();
        parser_.expect(TokenKind::LeftParen, "'(' after 'match'");
        state.governing = parser_.parseExpression();
        parser_.expect(TokenKind::RightParen, "')' after match governing expression");
        parser_.expect(TokenKind::LeftBrace, "'{' to open match arms");
        work_.push_back({WorkKind::ContinueMatch, index});
    }

// Advances one arm or lowers the completed match; stateIndex remains stable across reallocations.
void Parser::StructuredControlParser::continueMatch(std::size_t index)
{
        MatchParseState& state = matchStates_[index];
        if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
        {
            parser_.expect(TokenKind::RightBrace, "'}' to close match arms");
            values_.push_back(lowerMatchStatement(state));
            return;
        }
        parseMatchArmHeader(state);
        work_.push_back({WorkKind::ContinueMatch, index});
        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishMatchArm, index});
            work_.push_back({WorkKind::ParseBlock, 0});
            return;
        }
        reportMissingMatchArmBody();
        ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            parser_.current().range());
        parser_.synchronizeStatement();
        finishMatchArm(index, body);
    }

// Parses and validates only the pattern/guard/arrow prefix; the body is a separate task.
void Parser::StructuredControlParser::parseMatchArmHeader(MatchParseState& state)
{
        state.armBeginIndex = parser_.index_;
        state.pendingArm = MatchArm{};
        state.pendingArm.pattern = parser_.parsePattern(false);
        state.pendingArm.range = state.pendingArm.pattern.range;
        state.pendingArm.analysis =
            parser_.analyzeMatchExpressionPattern(state.pendingArm.pattern, false);
        reportMatchPatternDiagnostics(state.pendingArm.analysis);
        accumulateMatchOwnership(state, state.pendingArm.analysis);
        if (parser_.atContextualKeyword("when"))
        {
            parser_.consume();
            state.pendingArm.guard = parser_.parseConditionalExpression();
        }
        validateMatchArmReachability(state);
        parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    }

// Preserves the historical missing-block diagnostic before statement synchronization.
void Parser::StructuredControlParser::reportMissingMatchArmBody()
{
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "a match arm requires a block body");
    }

// Returns the stable spelling embedded in binding-mode diagnostics.
std::string_view Parser::StructuredControlParser::matchBindingModeName(PatternBindingMode mode) noexcept
{
        switch (mode)
        {
        case PatternBindingMode::Value: return "value";
        case PatternBindingMode::Reference: return "ref";
        case PatternBindingMode::Move: return "move";
        case PatternBindingMode::Copy: return "copy";
        case PatternBindingMode::ReferenceMutable: return "ref mut";
        }
        return "value";
    }

// Publishes the two historical diagnostic families from one source-ordered fact record.
void Parser::StructuredControlParser::reportMatchPatternDiagnostics(const Parser::MatchPatternAnalysis& analysis)
{
        for (const auto& [range, mode] : analysis.nonAddressableBindings)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                range,
                std::string(matchBindingModeName(mode)) +
                    " pattern binding cannot cross a tuple deconstruction or variant payload "
                    "copy; bind the aggregate by ref/move first");
        for (const SourceRange& range : analysis.invalidOrRanges)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                range,
                "OR-pattern alternatives must bind identical names with identical "
                "value/ref/move modes");
    }

// Accumulates ownership requirements without rescanning already analyzed arm patterns.
void Parser::StructuredControlParser::accumulateMatchOwnership(
    MatchParseState& state,
    const Parser::MatchPatternAnalysis& analysis)
{
        for (const auto& [name, mode] : analysis.bindings)
        {
            static_cast<void>(name);
            state.hasReferenceBinding = state.hasReferenceBinding ||
                mode == PatternBindingMode::Reference ||
                mode == PatternBindingMode::ReferenceMutable;
            state.hasMoveBinding = state.hasMoveBinding || mode == PatternBindingMode::Move;
        }
    }

// Updates cross-arm reachability state while preserving diagnostic order.
void Parser::StructuredControlParser::validateMatchArmReachability(MatchParseState& state)
{
        MatchArm& arm = state.pendingArm;
        const bool guardAlwaysTrue = arm.guard == nullptr ||
            (arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
             arm.guard->tokenKind() == TokenKind::KwTrue);
        const bool guardAlwaysFalse = arm.guard != nullptr &&
            arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
            arm.guard->tokenKind() == TokenKind::KwFalse;
        if (guardAlwaysFalse) reportConstantFalseMatchGuard(arm);
        if (state.sawCatchAll) reportMatchArmAfterCatchAll(arm);
        if (arm.analysis.wildcard && guardAlwaysTrue) state.sawCatchAll = true;
        validateMatchIntegerIntervals(state, guardAlwaysTrue);
    }

// Emits the established constant-false guard diagnostic at the guard range.
void Parser::StructuredControlParser::reportConstantFalseMatchGuard(const MatchArm& arm)
{
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            arm.guard->range(),
            "match arm is unreachable because its guard is the constant false");
    }

// Emits the established unreachable-arm diagnostic after an unconditional catch-all.
void Parser::StructuredControlParser::reportMatchArmAfterCatchAll(const MatchArm& arm)
{
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            arm.range,
            "match arm is unreachable after a catch-all wildcard or binding arm");
    }

// Collects a complete literal/range OR domain; false means the mixed domain is not comparable.
bool Parser::StructuredControlParser::collectMatchIntegerIntervals(
    const Pattern& root,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
        bool complete = true;
        std::vector<const Pattern*> pending{&root};
        while (!pending.empty())
        {
            const Pattern* pattern = pending.back();
            pending.pop_back();
            if (appendMatchIntegerInterval(*pattern, intervals)) continue;
            if (pattern->kind != PatternKind::Or)
            {
                complete = false;
                continue;
            }
            for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                pending.push_back(&*child);
        }
        return complete;
    }

// Appends one half-open integer interval when the pattern has a bounded integral domain.
bool Parser::StructuredControlParser::appendMatchIntegerInterval(
    const Pattern& pattern,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
        if (pattern.kind == PatternKind::Literal && pattern.firstInteger.has_value() &&
            *pattern.firstInteger != std::numeric_limits<std::int64_t>::max())
        {
            intervals.emplace_back(*pattern.firstInteger, *pattern.firstInteger + 1);
            return true;
        }
        if (pattern.kind == PatternKind::Range && pattern.firstInteger.has_value() &&
            pattern.secondInteger.has_value() && *pattern.firstInteger < *pattern.secondInteger)
        {
            intervals.emplace_back(*pattern.firstInteger, *pattern.secondInteger);
            return true;
        }
        return false;
    }

// Diagnoses overlap and publishes ranges only for arms that are not guard-restricted.
void Parser::StructuredControlParser::validateMatchIntegerIntervals(MatchParseState& state, bool guardAlwaysTrue)
{
        std::vector<std::pair<std::int64_t, std::int64_t>> current;
        if (!collectMatchIntegerIntervals(state.pendingArm.pattern, current)) return;
        const bool overlapsWithinArm = normalizeMatchIntegerIntervals(current);
        const bool overlapsEarlierArm = matchIntervalsOverlap(
            current, state.coveredIntegerRanges);
        if (overlapsWithinArm || overlapsEarlierArm)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                state.pendingArm.range,
                "integer literal/range pattern overlaps an earlier alternative");
        if (guardAlwaysTrue)
            for (const auto& interval : current)
                insertMatchIntegerInterval(state.coveredIntegerRanges, interval);
    }

// Sorts and coalesces one arm, reporting whether strict half-open overlap existed within it.
bool Parser::StructuredControlParser::normalizeMatchIntegerIntervals(
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
        std::ranges::sort(intervals);
        std::size_t output = 0;
        bool overlaps = false;
        for (const auto& interval : intervals)
        {
            if (output == 0 || intervals[output - 1].second < interval.first)
            {
                intervals[output++] = interval;
                continue;
            }
            overlaps = overlaps || interval.first < intervals[output - 1].second;
            intervals[output - 1].second =
                std::max(intervals[output - 1].second, interval.second);
        }
        intervals.resize(output);
        return overlaps;
    }

// Queries the disjoint ordered index in logarithmic time for each normalized interval.
bool Parser::StructuredControlParser::matchIntervalsOverlap(
    const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
    const MatchIntegerIntervalIndex& covered)
{
        for (const auto& interval : current)
        {
            const auto next = covered.lower_bound(interval.first);
            if (next != covered.end() && next->first < interval.second) return true;
            if (next == covered.begin()) continue;
            const auto previous = std::prev(next);
            if (interval.first < previous->second) return true;
        }
        return false;
    }

// Inserts one interval and coalesces overlap or adjacency without changing half-open queries.
void Parser::StructuredControlParser::insertMatchIntegerInterval(
    MatchIntegerIntervalIndex& covered,
    std::pair<std::int64_t, std::int64_t> interval)
{
        auto next = covered.lower_bound(interval.first);
        if (next != covered.begin())
        {
            const auto previous = std::prev(next);
            if (previous->second >= interval.first)
            {
                interval.first = previous->first;
                interval.second = std::max(interval.second, previous->second);
                covered.erase(previous);
            }
        }
        next = covered.lower_bound(interval.first);
        while (next != covered.end() && next->first <= interval.second)
        {
            interval.second = std::max(interval.second, next->second);
            next = covered.erase(next);
        }
        covered.emplace(interval);
    }

// Attaches a completed body, closes the source range, and consumes the optional comma.
void Parser::StructuredControlParser::finishMatchArm(std::size_t index, ast::AstNode* body)
{
        MatchParseState& state = matchStates_[index];
        state.pendingArm.body = body;
        state.pendingArm.range = parser_.tokenRangeFrom(state.armBeginIndex);
        state.arms.push_back(std::move(state.pendingArm));
        parser_.consumeIf(TokenKind::Comma);
    }

// Materializes the historical early lowering after every arm body has completed.
ast::AstNode* Parser::StructuredControlParser::lowerMatchStatement(MatchParseState& state)
{
        if (state.arms.empty()) reportEmptyMatch(state);
        const SourceRange range = parser_.tokenRangeFrom(state.beginIndex);
        ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            range);
        const std::string temporary =
            "$__nsc_match_" + std::to_string(parser_.nextMatchTemporary_++);
        lowered->setText(temporary);
        lowered->setAuxiliaryText(buildMatchCoverage(state.arms));
        appendMatchGoverningState(*lowered, state, temporary);
        for (const MatchArm& arm : state.arms)
            lowered->addChild(lowerMatchArm(arm, range, temporary));
        return lowered;
    }

// Reports an empty statement match while retaining its lowering shell.
void Parser::StructuredControlParser::reportEmptyMatch(const MatchParseState& state)
{
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            state.keywordRange,
            "a match statement requires at least one arm");
    }

// Builds the source-ordered semantic coverage bridge consumed by later analysis.
std::string Parser::StructuredControlParser::buildMatchCoverage(const std::vector<MatchArm>& arms)
{
        std::string coverage = "match-coverage:v1:";
        for (std::size_t index = 0; index < arms.size(); ++index)
        {
            if (index != 0) coverage += ';';
            appendMatchArmCoverage(coverage, arms[index]);
        }
        return coverage;
    }

// Encodes one arm as wildcard, list-length, nominal-case, or unknown coverage.
void Parser::StructuredControlParser::appendMatchArmCoverage(std::string& coverage, const MatchArm& arm)
{
        std::vector<std::string> cases = arm.analysis.cases;
        std::vector<std::pair<std::size_t, bool>> listLengths = arm.analysis.listLengths;
        const bool restricted = arm.guard != nullptr &&
            !(arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
              arm.guard->tokenKind() == TokenKind::KwTrue);
        if (arm.analysis.wildcard)
        {
            if (restricted) coverage += '~';
            coverage += '*';
            return;
        }
        if (!listLengths.empty())
        {
            appendMatchListCoverage(
                coverage, listLengths, arm.analysis.partial || restricted);
            return;
        }
        appendMatchCaseCoverage(
            coverage, cases, arm.analysis.partial || restricted);
    }

// Sorts and deduplicates list domains before emitting their stable marker format.
void Parser::StructuredControlParser::appendMatchListCoverage(
    std::string& coverage,
    std::vector<std::pair<std::size_t, bool>>& lengths,
    bool partial)
{
        std::ranges::sort(lengths);
        lengths.erase(std::unique(lengths.begin(), lengths.end()), lengths.end());
        if (partial) coverage += '~';
        for (std::size_t index = 0; index < lengths.size(); ++index)
        {
            if (index != 0) coverage += '|';
            coverage += '#' + std::to_string(lengths[index].first);
            if (lengths[index].second) coverage += '+';
        }
    }

// Sorts and deduplicates nominal cases before emitting their stable marker format.
void Parser::StructuredControlParser::appendMatchCaseCoverage(
    std::string& coverage,
    std::vector<std::string>& cases,
    bool partial)
{
        std::ranges::sort(cases);
        cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
        if (cases.empty())
        {
            coverage += '?';
            return;
        }
        if (partial) coverage += '~';
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            if (index != 0) coverage += '|';
            coverage += cases[index];
        }
    }

// Creates the governing-value and selected-flag locals with preserved ownership mode.
void Parser::StructuredControlParser::appendMatchGoverningState(
    ast::AstNode& lowered,
    const MatchParseState& state,
    const std::string& temporary)
{
        ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
            state.keywordRange,
            TokenKind::KwVar,
            "var");
        ast::AstNode* governing = parser_.makeVariableDeclaration(
            inferred,
            temporary,
            state.governing,
            false,
            !state.hasReferenceBinding && !state.hasMoveBinding);
        if (state.hasReferenceBinding) governing->setAuxiliaryText("local:ref");
        lowered.addChild(governing);
        lowered.addChild(makeMatchSelectedDeclaration(state.keywordRange, temporary));
    }

// Creates the false-initialized selected flag used by the lowered decision chain.
ast::AstNode* Parser::StructuredControlParser::makeMatchSelectedDeclaration(
    SourceRange range,
    const std::string& temporary)
{
        ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
            range,
            TokenKind::KwBool,
            "bool");
        return parser_.makeVariableDeclaration(
            boolType,
            temporary + "_selected",
            parser_.makeBooleanLiteral(range, false),
            false,
            false);
    }

// Lowers one arm to the historical not-selected guard and pattern matcher.
ast::AstNode* Parser::StructuredControlParser::lowerMatchArm(
    const MatchArm& arm,
    SourceRange matchRange,
    const std::string& temporary)
{
        ast::AstNode* selectedBody = makeMatchSelectedBody(arm, temporary);
        PatternExpressionFactory root = [this, &temporary, range = arm.range]
        {
            return parser_.makeIdentifierExpression(range, temporary);
        };
        ast::AstNode* continuation = wrapMatchGuard(arm, selectedBody);
        ast::AstNode* matcher = parser_.lowerPattern(
            arm.pattern,
            root,
            continuation,
            temporary);
        ast::AstNode* guardedBody = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            arm.range);
        guardedBody->addChild(matcher);
        ast::AstNode* guard = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement,
            arm.range);
        guard->setFirst(makeMatchNotSelected(matchRange, temporary));
        guard->setSecond(guardedBody);
        return guard;
    }

// Prepends selected=true to the original body statements without copying their AST nodes.
ast::AstNode* Parser::StructuredControlParser::makeMatchSelectedBody(const MatchArm& arm, const std::string& temporary)
{
        ast::AstNode* selected = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            arm.range);
        selected->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
            parser_.makeIdentifierExpression(arm.range, temporary + "_selected"),
            parser_.makeBooleanLiteral(arm.range, true))));
        for (ast::AstNode* statement : arm.body->children()) selected->addChild(statement);
        return selected;
    }

// Evaluates a source guard only after pattern bindings have been materialized.
ast::AstNode* Parser::StructuredControlParser::wrapMatchGuard(const MatchArm& arm, ast::AstNode* selectedBody)
{
        if (arm.guard == nullptr) return selectedBody;
        ast::AstNode* guarded = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement,
            arm.guard->range());
        guarded->setFirst(arm.guard);
        guarded->setSecond(selectedBody);
        ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            arm.guard->range());
        block->addChild(guarded);
        return block;
    }

// Builds the outer !selected condition that enforces first-match semantics.
ast::AstNode* Parser::StructuredControlParser::makeMatchNotSelected(SourceRange range, const std::string& temporary)
{
        ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression,
            range);
        result->setTokenKind(TokenKind::Bang);
        result->setFirst(parser_.makeIdentifierExpression(range, temporary + "_selected"));
        return result;
    }

void Parser::StructuredControlParser::parseEmbedded()
{
        if (parser_.at(TokenKind::LeftBrace))
            work_.push_back({WorkKind::ParseBlock, 0});
        else if (parser_.at(TokenKind::Semicolon))
            values_.push_back(parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, parser_.consume().range()));
        else if (structuredControlAtCursor())
        {
            work_.push_back({WorkKind::WrapEmbedded, 0});
            work_.push_back({WorkKind::ParseControl, 0});
        }
        else
            values_.push_back(wrapEmbedded(parser_.parseStatement()));
    }

void Parser::StructuredControlParser::finishWrapEmbedded()
{
        ast::AstNode* statement = popValue();
        values_.push_back(wrapEmbedded(statement));
    }

void Parser::StructuredControlParser::parseBlock()
{
        BlockParseState state;
        state.beginIndex = parser_.index_;
        parser_.expect(TokenKind::LeftBrace, "'{'");
        state.block = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            parser_.tokenRangeFrom(state.beginIndex));
        state.destination = state.block;
        const std::size_t index = blockStates_.size();
        blockStates_.push_back(state);
        work_.push_back({WorkKind::ContinueBlock, index});
    }

void Parser::StructuredControlParser::continueBlock(const Work& current)
{
        BlockParseState& state = blockStates_[current.stateIndex];
        if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
        {
            parser_.expect(TokenKind::RightBrace, "'}'");
            state.block->setRange(parser_.tokenRangeFrom(state.beginIndex));
            values_.push_back(state.block);
            return;
        }

        const std::size_t before = parser_.index_;
        work_.push_back(current);
        if (structuredControlAtCursor()) pushBlockChild(current.stateIndex, WorkKind::ParseControl);
        else if (parser_.at(TokenKind::LeftBrace)) pushBlockChild(current.stateIndex, WorkKind::ParseBlock);
        else
        {
            parser_.appendParsedStatement(state.destination, parser_.parseStatement());
            if (parser_.index_ == before) parser_.consume();
        }
    }

void Parser::StructuredControlParser::pushBlockChild(std::size_t blockIndex, WorkKind child)
{
        work_.push_back({WorkKind::AppendBlock, blockIndex});
        work_.push_back({child, 0});
    }

void Parser::StructuredControlParser::appendBlock(std::size_t index)
{
        parser_.appendParsedStatement(blockStates_[index].destination, popValue());
    }

void Parser::StructuredControlParser::finishThen(std::size_t index)
{
        IfParseState& state = ifStates_[index];
        state.thenBlock = popValue();
        if (!parser_.consumeIf(TokenKind::KwElse))
        {
            values_.push_back(parser_.lowerIfStatement(state, nullptr));
            return;
        }
        work_.push_back({WorkKind::FinishElse, index});
        work_.push_back({parser_.at(TokenKind::KwIf)
            ? WorkKind::ParseIf
            : WorkKind::ParseEmbedded, 0});
    }

void Parser::StructuredControlParser::finishElse(std::size_t index)
{
        values_.push_back(parser_.lowerIfStatement(ifStates_[index], popValue()));
    }

void Parser::StructuredControlParser::finishWhile(std::size_t index)
{
        values_.push_back(parser_.lowerWhileStatement(whileStates_[index], popValue()));
    }

void Parser::StructuredControlParser::finishDoWhile(std::size_t index)
{
        values_.push_back(parser_.lowerDoWhileStatement(doWhileStates_[index], popValue()));
    }

void Parser::StructuredControlParser::finishFor(std::size_t index)
{
        values_.push_back(parser_.lowerForStatement(forStates_[index], popValue()));
    }

ast::AstNode* Parser::StructuredControlParser::popValue()
{
        ast::AstNode* value = values_.back();
        values_.pop_back();
        return value;
    }
}
