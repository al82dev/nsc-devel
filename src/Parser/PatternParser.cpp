#include "nativesharp/Parser/Parser.h"

#include "Internal/PatternAlternativeExpander.h"
#include "Internal/PatternBindingEmitter.h"
#include "Internal/PatternConditionBuilder.h"
#include "Internal/PatternIterationParser.h"
#include "Internal/PatternLoweringMachine.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}








































Parser::PatternConditionBuilder::PatternConditionBuilder(
    Parser& parser,
    const Pattern& root,
    PatternExpressionFactory source)
    : parser_(parser), root_(root), rootSource_(std::move(source))
{
    sources_.push_back(SourcePlan{});
}

ast::AstNode* Parser::PatternConditionBuilder::run()
{
    scheduleBuild(root_, 0);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
    return takeValue();
}

void Parser::PatternConditionBuilder::schedule(Work work)
{
    work_.push_back(std::move(work));
}

void Parser::PatternConditionBuilder::scheduleBuild(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work work;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    schedule(std::move(work));
}

ast::AstNode* Parser::PatternConditionBuilder::takeValue()
{
    ast::AstNode* const value = values_.back();
    values_.pop_back();
    return value;
}

void Parser::PatternConditionBuilder::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::Build: return build(*work.pattern, work.sourcePlan);
        case WorkKind::FinishNot: return finishNot(std::move(work));
        case WorkKind::FinishVariant: return finishVariant(std::move(work));
        case WorkKind::FinishLogical: return finishLogical(std::move(work));
        case WorkKind::ContinueStruct: return continueStruct(std::move(work));
        case WorkKind::FinishStruct: return finishStruct(std::move(work));
        case WorkKind::ContinueList: return continueList(std::move(work));
        case WorkKind::FinishList: return finishList(std::move(work));
    }
}

std::size_t Parser::PatternConditionBuilder::addSourcePlan(
    std::size_t parent,
    SourceStepKind kind,
    std::string text,
    ast::AstNode* index)
{
    SourcePlan plan;
    plan.parent = parent;
    plan.kind = kind;
    plan.text = std::move(text);
    plan.index = index;
    sources_.push_back(std::move(plan));
    return sources_.size() - 1U;
}

ast::AstNode* Parser::PatternConditionBuilder::evaluateSource(std::size_t sourcePlan)
{
    std::vector<std::size_t> path;
    while (sourcePlan != 0U)
    {
        path.push_back(sourcePlan);
        sourcePlan = sources_[sourcePlan].parent;
    }
    ast::AstNode* result = rootSource_();
    for (auto stepIndex = path.rbegin(); stepIndex != path.rend(); ++stepIndex)
    {
        const SourcePlan& step = sources_[*stepIndex];
        if (step.kind == SourceStepKind::Member)
            result = parser_.makeMemberExpression(result, step.text);
        else if (step.kind == SourceStepKind::VariantPayload)
        {
            result = parser_.makeZeroArgCallExpression(
                parser_.makeMemberExpression(result, "__nsc_variant_extract_" + step.text));
            result->setAuxiliaryText("variant-pattern-extract");
        }
        else if (step.kind == SourceStepKind::Index)
            result = parser_.makeIndexExpression(
                result, parser_.cloneDeclarationNode(step.index));
    }
    return result;
}

ast::AstNode* Parser::PatternConditionBuilder::buildLeaf(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    if (pattern.kind == PatternKind::Wildcard || pattern.kind == PatternKind::Binding ||
        pattern.kind == PatternKind::Positional)
        return parser_.makeBooleanLiteral(pattern.range, true);
    if (pattern.kind == PatternKind::Tuple)
        return parser_.makeBooleanLiteral(pattern.range, false);
    if (pattern.kind == PatternKind::Literal)
        return parser_.makeBinaryExpression(
            TokenKind::EqualEqual, evaluateSource(sourcePlan), pattern.firstValue);
    if (pattern.kind == PatternKind::Range)
    {
        ast::AstNode* lower = parser_.makeBinaryExpression(
            TokenKind::GreaterEqual, evaluateSource(sourcePlan), pattern.firstValue);
        ast::AstNode* upper = parser_.makeBinaryExpression(
            pattern.name == "..=" ? TokenKind::LessEqual : TokenKind::Less,
            evaluateSource(sourcePlan),
            pattern.secondValue);
        return parser_.makeBinaryExpression(TokenKind::AmpAmp, lower, upper);
    }
    TokenKind operation = TokenKind::Less;
    if (pattern.name == "<=") operation = TokenKind::LessEqual;
    else if (pattern.name == ">") operation = TokenKind::Greater;
    else if (pattern.name == ">=") operation = TokenKind::GreaterEqual;
    if (pattern.kind == PatternKind::Relational)
        return parser_.makeBinaryExpression(
            operation, evaluateSource(sourcePlan), pattern.firstValue);
    return parser_.makeBooleanLiteral(pattern.range, false);
}

void Parser::PatternConditionBuilder::build(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    if (pattern.kind == PatternKind::Struct)
        return startStruct(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Variant)
        return startVariant(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Or)
        return startLogical(pattern, sourcePlan, TokenKind::PipePipe);
    if (pattern.kind == PatternKind::List)
        return startList(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Not)
        return startNot(pattern, sourcePlan);
    if (pattern.kind == PatternKind::And)
        return startLogical(pattern, sourcePlan, TokenKind::AmpAmp);
    values_.push_back(buildLeaf(pattern, sourcePlan));
}

void Parser::PatternConditionBuilder::startNot(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work finish;
    finish.kind = WorkKind::FinishNot;
    finish.pattern = &pattern;
    schedule(std::move(finish));
    scheduleBuild(pattern.children.front(), sourcePlan);
}

void Parser::PatternConditionBuilder::finishNot(Work work)
{
    ast::AstNode* unary = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, work.pattern->range);
    unary->setTokenKind(TokenKind::Bang);
    unary->setFirst(takeValue());
    values_.push_back(unary);
}

void Parser::PatternConditionBuilder::startVariant(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    ast::AstNode* test = parser_.makeZeroArgCallExpression(
        parser_.makeMemberExpression(evaluateSource(sourcePlan), "Is" + pattern.name));
    if (pattern.children.empty())
    {
        values_.push_back(test);
        return;
    }
    Work finish;
    finish.kind = WorkKind::FinishVariant;
    finish.result = test;
    schedule(std::move(finish));
    const std::size_t payload = addSourcePlan(
        sourcePlan, SourceStepKind::VariantPayload, pattern.name);
    scheduleBuild(pattern.children.front(), payload);
}

void Parser::PatternConditionBuilder::finishVariant(Work work)
{
    values_.push_back(parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue()));
}

void Parser::PatternConditionBuilder::startLogical(
    const Pattern& pattern,
    std::size_t sourcePlan,
    TokenKind operation)
{
    Work finish;
    finish.kind = WorkKind::FinishLogical;
    finish.pattern = &pattern;
    finish.sourcePlan = sourcePlan;
    finish.nextChild = 1U;
    finish.operation = operation;
    schedule(std::move(finish));
    scheduleBuild(pattern.children.front(), sourcePlan);
}

void Parser::PatternConditionBuilder::finishLogical(Work work)
{
    ast::AstNode* const child = takeValue();
    work.result = work.result == nullptr
        ? child
        : parser_.makeBinaryExpression(work.operation, work.result, child);
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const Pattern& next = work.pattern->children[work.nextChild++];
    const std::size_t sourcePlan = work.sourcePlan;
    schedule(std::move(work));
    scheduleBuild(next, sourcePlan);
}

void Parser::PatternConditionBuilder::startStruct(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work work;
    work.kind = WorkKind::ContinueStruct;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.result = parser_.makeBooleanLiteral(pattern.range, true);
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::continueStruct(Work work)
{
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    const std::size_t member = addSourcePlan(
        work.sourcePlan,
        SourceStepKind::Member,
        work.pattern->fieldNames[childIndex]);
    const Pattern* const child = &work.pattern->children[childIndex];
    work.kind = WorkKind::FinishStruct;
    schedule(std::move(work));
    scheduleBuild(*child, member);
}

void Parser::PatternConditionBuilder::finishStruct(Work work)
{
    work.result = parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue());
    work.kind = WorkKind::ContinueStruct;
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::startList(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    const std::size_t fixedCount = pattern.children.size() -
        (pattern.restIndex.has_value() ? 1U : 0U);
    ast::AstNode* length = parser_.makeMemberExpression(
        evaluateSource(sourcePlan), "Length");
    length->setAuxiliaryText("list-pattern-length");
    Work work;
    work.kind = WorkKind::ContinueList;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.result = parser_.makeBinaryExpression(
        pattern.restIndex.has_value() ? TokenKind::GreaterEqual : TokenKind::EqualEqual,
        length,
        parser_.makeIntegerLiteralExpression(pattern.range, fixedCount));
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::continueList(Work work)
{
    while (work.nextChild < work.pattern->children.size() &&
           work.pattern->restIndex == work.nextChild)
        ++work.nextChild;
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    ast::AstNode* elementIndex = nullptr;
    const std::size_t restOffset =
        work.pattern->restIndex.value_or(work.pattern->children.size());
    if (!work.pattern->restIndex.has_value() || childIndex < restOffset)
        elementIndex = parser_.makeIntegerLiteralExpression(work.pattern->range, childIndex);
    else
    {
        ast::AstNode* length = parser_.makeMemberExpression(
            evaluateSource(work.sourcePlan), "Length");
        length->setAuxiliaryText("list-pattern-length");
        elementIndex = parser_.makeBinaryExpression(
            TokenKind::Minus,
            length,
            parser_.makeIntegerLiteralExpression(
                work.pattern->range, work.pattern->children.size() - childIndex));
    }
    const std::size_t element = addSourcePlan(
        work.sourcePlan, SourceStepKind::Index, {}, elementIndex);
    const Pattern* const child = &work.pattern->children[childIndex];
    work.kind = WorkKind::FinishList;
    schedule(std::move(work));
    scheduleBuild(*child, element);
}

void Parser::PatternConditionBuilder::finishList(Work work)
{
    work.result = parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue());
    work.kind = WorkKind::ContinueList;
    schedule(std::move(work));
}

Parser::PatternLoweringMachine::PatternLoweringMachine(
    Parser& parser,
    const Pattern& root,
    PatternExpressionFactory source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
    : parser_(parser),
      root_(root),
      rootSource_(std::move(source)),
      continuation_(continuation),
      temporaryPrefix_(temporaryPrefix),
      emitBindings_(emitBindings)
{
    sources_.push_back(SourcePlan{});
}

ast::AstNode* Parser::PatternLoweringMachine::run()
{
    scheduleBuild(root_, 0, continuation_);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
    return takeValue();
}

void Parser::PatternLoweringMachine::schedule(Work work)
{
    work_.push_back(std::move(work));
}

void Parser::PatternLoweringMachine::scheduleBuild(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.continuation = continuation;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::Build:
            return build(*work.pattern, work.sourcePlan, work.continuation);
        case WorkKind::ContinueOr: return continueOr(std::move(work));
        case WorkKind::FinishOrChild: return finishOrChild(std::move(work));
        case WorkKind::ContinueSequential: return continueSequential(std::move(work));
        case WorkKind::FinishSequential: return finishSequential(std::move(work));
        case WorkKind::FinishVariant: return finishVariant(std::move(work));
    }
}

ast::AstNode* Parser::PatternLoweringMachine::takeValue()
{
    ast::AstNode* const value = values_.back();
    values_.pop_back();
    return value;
}

std::size_t Parser::PatternLoweringMachine::addSourcePlan(
    std::size_t parent,
    SourceKind kind,
    SourceRange range,
    std::string text)
{
    SourcePlan plan;
    plan.parent = parent;
    plan.kind = kind;
    plan.range = range;
    plan.text = std::move(text);
    sources_.push_back(std::move(plan));
    return sources_.size() - 1U;
}

ast::AstNode* Parser::PatternLoweringMachine::evaluateSource(std::size_t sourcePlan)
{
    std::vector<std::size_t> members;
    while (sources_[sourcePlan].kind == SourceKind::Member)
    {
        members.push_back(sourcePlan);
        sourcePlan = sources_[sourcePlan].parent;
    }
    const SourcePlan& base = sources_[sourcePlan];
    ast::AstNode* result = base.kind == SourceKind::Root
        ? rootSource_()
        : parser_.makeIdentifierExpression(base.range, base.text);
    for (auto member = members.rbegin(); member != members.rend(); ++member)
        result = parser_.makeMemberExpression(result, sources_[*member].text);
    return result;
}

Parser::PatternExpressionFactory Parser::PatternLoweringMachine::makeSourceFactory(
    std::size_t sourcePlan)
{
    return [this, sourcePlan]
    {
        return evaluateSource(sourcePlan);
    };
}

void Parser::PatternLoweringMachine::finishClassification(
    std::vector<ClassificationFrame>& frames,
    bool value)
{
    conditionOnly_[frames.back().pattern] = value;
    frames.pop_back();
    if (!frames.empty() && !value) frames.back().conditionOnly = false;
}

bool Parser::PatternLoweringMachine::isConditionOnly(const Pattern& pattern)
{
    if (const auto found = conditionOnly_.find(&pattern); found != conditionOnly_.end())
        return found->second;
    std::vector<ClassificationFrame> frames{{&pattern}};
    while (!frames.empty())
    {
        ClassificationFrame& frame = frames.back();
        if (!frame.conditionOnly || frame.pattern->kind == PatternKind::Binding ||
            frame.pattern->kind == PatternKind::Tuple || frame.pattern->kind == PatternKind::List)
        {
            finishClassification(frames, false);
            continue;
        }
        if (frame.nextChild == frame.pattern->children.size())
        {
            finishClassification(frames, true);
            continue;
        }
        const Pattern* const child = &frame.pattern->children[frame.nextChild++];
        if (const auto found = conditionOnly_.find(child); found != conditionOnly_.end())
        {
            if (!found->second) frame.conditionOnly = false;
            continue;
        }
        frames.push_back(ClassificationFrame{child});
    }
    return conditionOnly_.at(&pattern);
}

void Parser::PatternLoweringMachine::build(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    if (pattern.kind == PatternKind::Wildcard)
        values_.push_back(continuation);
    else if (pattern.kind == PatternKind::Binding)
        buildBinding(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Or && !isConditionOnly(pattern))
        startOr(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Tuple || pattern.kind == PatternKind::Struct ||
            pattern.kind == PatternKind::Positional)
        startSequential(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::List)
        buildList(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Variant)
        startVariant(pattern, sourcePlan, continuation);
    else
        buildConditional(pattern, sourcePlan, continuation);
}

void Parser::PatternLoweringMachine::buildBinding(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    if (!emitBindings_)
    {
        values_.push_back(continuation);
        return;
    }
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    ast::AstNode* declaration = parser_.makeVariableDeclaration(
        type,
        pattern.name,
        evaluateSource(sourcePlan),
        false,
        pattern.bindingMode == PatternBindingMode::Value ||
            pattern.bindingMode == PatternBindingMode::Copy ||
            pattern.bindingMode == PatternBindingMode::Reference);
    parser_.applyPatternBindingMode(declaration, pattern.bindingMode);
    block->addChild(declaration);
    block->addChild(continuation);
    values_.push_back(block);
}

void Parser::PatternLoweringMachine::startOr(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.kind = WorkKind::ContinueOr;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.continuation = continuation;
    work.block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    work.selectedName = temporaryPrefix_ + "_or_selected_" +
        std::to_string(parser_.nextMatchTemporary_++);
    ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwBool, "bool");
    work.block->addChild(parser_.makeVariableDeclaration(
        boolType, work.selectedName, parser_.makeBooleanLiteral(pattern.range, false),
        false, false));
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::continueOr(Work work)
{
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.block);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    const Pattern* const child = &work.pattern->children[childIndex];
    ast::AstNode* selectedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, child->range);
    selectedBody->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
        parser_.makeIdentifierExpression(child->range, work.selectedName),
        parser_.makeBooleanLiteral(child->range, true))));
    selectedBody->addChild(childIndex + 1U == work.pattern->children.size()
        ? work.continuation
        : parser_.cloneDeclarationNode(work.continuation));
    work.kind = WorkKind::FinishOrChild;
    work.result = selectedBody;
    const std::size_t sourcePlan = work.sourcePlan;
    schedule(std::move(work));
    scheduleBuild(*child, sourcePlan, selectedBody);
}

void Parser::PatternLoweringMachine::finishOrChild(Work work)
{
    ast::AstNode* notSelected = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, work.result->range());
    notSelected->setTokenKind(TokenKind::Bang);
    notSelected->setFirst(parser_.makeIdentifierExpression(
        work.result->range(), work.selectedName));
    ast::AstNode* guarded = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, work.result->range());
    guarded->setFirst(notSelected);
    ast::AstNode* guardedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, work.result->range());
    guardedBody->addChild(takeValue());
    guarded->setSecond(guardedBody);
    work.block->addChild(guarded);
    work.kind = WorkKind::ContinueOr;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::initializeDeconstruction(Work& work)
{
    work.block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, work.pattern->range);
    ast::AstNode* deconstruction = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, work.pattern->range);
    deconstruction->setFirst(evaluateSource(work.sourcePlan));
    if (work.pattern->kind == PatternKind::Positional)
        deconstruction->setTypeSyntax(parser_.arena_.make<ast::TypeSyntax>(
            work.pattern->range, TokenKind::Identifier, work.pattern->name));
    work.names.reserve(work.pattern->children.size());
    const std::string marker = work.pattern->kind == PatternKind::Tuple ? "_tuple_" : "_pos_";
    for (std::size_t index = 0; index < work.pattern->children.size(); ++index)
    {
        const std::string name = temporaryPrefix_ + marker +
            std::to_string(parser_.nextMatchTemporary_++) + "_" + std::to_string(index);
        work.names.push_back(name);
        deconstruction->addChild(
            parser_.makeIdentifierExpression(work.pattern->range, name));
    }
    work.block->addChild(deconstruction);
}

void Parser::PatternLoweringMachine::startSequential(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.kind = WorkKind::ContinueSequential;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.nextChild = pattern.children.size();
    work.result = continuation;
    if (pattern.kind != PatternKind::Struct) initializeDeconstruction(work);
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::continueSequential(Work work)
{
    if (work.nextChild == 0U)
    {
        if (work.block != nullptr)
        {
            work.block->addChild(work.result);
            values_.push_back(work.block);
        }
        else
            values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = --work.nextChild;
    const std::size_t childSource = work.pattern->kind == PatternKind::Struct
        ? addSourcePlan(
              work.sourcePlan,
              SourceKind::Member,
              work.pattern->range,
              work.pattern->fieldNames[childIndex])
        : addSourcePlan(0, SourceKind::Identifier, work.pattern->range, work.names[childIndex]);
    const Pattern* const child = &work.pattern->children[childIndex];
    ast::AstNode* const continuation = work.result;
    work.kind = WorkKind::FinishSequential;
    schedule(std::move(work));
    scheduleBuild(*child, childSource, continuation);
}

void Parser::PatternLoweringMachine::finishSequential(Work work)
{
    work.result = takeValue();
    work.kind = WorkKind::ContinueSequential;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::buildList(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    ast::AstNode* thenBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    PatternExpressionFactory source = makeSourceFactory(sourcePlan);
    if (emitBindings_)
        parser_.emitPatternBindings(pattern, source, thenBlock, temporaryPrefix_);
    thenBlock->addChild(continuation);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(parser_.makePatternCondition(pattern, source));
    decision->setSecond(thenBlock);
    values_.push_back(decision);
}

void Parser::PatternLoweringMachine::startVariant(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    ast::AstNode* test = parser_.makeZeroArgCallExpression(
        parser_.makeMemberExpression(evaluateSource(sourcePlan), "Is" + pattern.name));
    ast::AstNode* thenBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(test);
    decision->setSecond(thenBlock);
    if (pattern.children.empty() || pattern.children.front().kind == PatternKind::Wildcard)
    {
        thenBlock->addChild(continuation);
        values_.push_back(decision);
        return;
    }
    const std::string payloadName = temporaryPrefix_ + "_payload_" +
        std::to_string(parser_.nextMatchTemporary_++);
    ast::AstNode* extract = parser_.makeZeroArgCallExpression(parser_.makeMemberExpression(
        evaluateSource(sourcePlan), "__nsc_variant_extract_" + pattern.name));
    extract->setAuxiliaryText("variant-pattern-extract");
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    thenBlock->addChild(parser_.makeVariableDeclaration(type, payloadName, extract, false, true));
    Work finish;
    finish.kind = WorkKind::FinishVariant;
    finish.block = thenBlock;
    finish.decision = decision;
    schedule(std::move(finish));
    const std::size_t payload = addSourcePlan(
        0, SourceKind::Identifier, pattern.range, payloadName);
    scheduleBuild(pattern.children.front(), payload, continuation);
}

void Parser::PatternLoweringMachine::finishVariant(Work work)
{
    work.block->addChild(takeValue());
    values_.push_back(work.decision);
}

void Parser::PatternLoweringMachine::buildConditional(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    PatternExpressionFactory source = makeSourceFactory(sourcePlan);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(parser_.makePatternCondition(pattern, source));
    decision->setSecond(continuation);
    values_.push_back(decision);
}

bool Parser::patternLiteralStartsHere() const noexcept
{
    const TokenKind kind = current().kind();
    return kind == TokenKind::IntegerLiteral || kind == TokenKind::FloatingLiteral ||
        kind == TokenKind::CharacterLiteral || kind == TokenKind::StringLiteral ||
        kind == TokenKind::KwTrue || kind == TokenKind::KwFalse || kind == TokenKind::KwNull ||
        (kind == TokenKind::Minus && peek().kind() == TokenKind::IntegerLiteral);
}

std::pair<ast::AstNode*, std::optional<std::int64_t>>
Parser::parseSignedPatternIntegerLiteral()
{
    const bool negative = consumeIf(TokenKind::Minus);
    if (!at(TokenKind::IntegerLiteral))
    {
        expect(TokenKind::IntegerLiteral, "integer literal in range pattern");
        return {makeErrorExpression(), std::nullopt};
    }
    ast::AstNode* literal = parsePrimaryExpression();
    const std::uint64_t magnitude = literal->integerValue();
    std::optional<std::int64_t> value;
    if (negative)
    {
        constexpr std::uint64_t minMagnitude =
            std::uint64_t{1} << (std::numeric_limits<std::int64_t>::digits);
        if (magnitude <= minMagnitude)
            value = magnitude == minMagnitude
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(magnitude);
        ast::AstNode* unary = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression,
            SourceRange(tokens_[index_ - 2].range().begin(), literal->range().end()));
        unary->setTokenKind(TokenKind::Minus);
        unary->setFirst(literal);
        literal = unary;
    }
    else if (magnitude <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        value = static_cast<std::int64_t>(magnitude);
    }
    return {literal, value};
}

Parser::Pattern Parser::parseBindingPattern(std::size_t patternBegin)
{
    Pattern result;
    if (at(TokenKind::KwMove))
    {
        result.bindingMode = PatternBindingMode::Move;
        consume();
    }
    else if (atContextualKeyword("copy"))
    {
        result.bindingMode = PatternBindingMode::Copy;
        consume();
    }
    else if (atContextualKeyword("ref"))
    {
        consume();
        if (atContextualKeyword("mut"))
        {
            result.bindingMode = PatternBindingMode::ReferenceMutable;
            consume();
        }
        else
        {
            result.bindingMode = PatternBindingMode::Reference;
        }
    }
    else
    {
        consume();
    }
    result.kind = PatternKind::Binding;
    if (!at(TokenKind::Identifier))
        expect(TokenKind::Identifier, "binding name after pattern binding mode");
    else
        result.name = std::string(consume().lexeme());
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseRelationalPattern(std::size_t patternBegin)
{
    Pattern result;
    result.kind = PatternKind::Relational;
    result.name = std::string(consume().lexeme());
    if (patternLiteralStartsHere() || at(TokenKind::Identifier))
        result.firstValue = parsePrimaryExpression();
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected a literal or constant operand after a relational pattern operator");
        result.firstValue = makeErrorExpression();
    }
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseLiteralOrRangePattern(
    std::size_t patternBegin,
    SourceRange start)
{
    Pattern result;
    result.kind = PatternKind::Literal;
    if (at(TokenKind::Minus))
    {
        auto [literal, value] = parseSignedPatternIntegerLiteral();
        result.firstValue = literal;
        result.firstInteger = value;
    }
    else
    {
        const TokenKind literalKind = current().kind();
        result.firstValue = parsePrimaryExpression();
        if (literalKind == TokenKind::IntegerLiteral)
        {
            const std::uint64_t value = result.firstValue->integerValue();
            if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                result.firstInteger = static_cast<std::int64_t>(value);
        }
        if (literalKind == TokenKind::FloatingLiteral)
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                result.firstValue->range(),
                "floating-point literals are not permitted in patterns because NaN and "
                "signed-zero equality do not define a closed pattern domain");
    }
    if (at(TokenKind::DotDot) || at(TokenKind::DotDotEqual))
    {
        result.name = std::string(consume().lexeme());
        result.kind = PatternKind::Range;
        if (!at(TokenKind::IntegerLiteral) &&
            !(at(TokenKind::Minus) && peek().kind() == TokenKind::IntegerLiteral))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "a range pattern requires an integer literal upper bound");
            result.secondValue = makeErrorExpression();
        }
        else
        {
            auto [upper, value] = parseSignedPatternIntegerLiteral();
            result.secondValue = upper;
            result.secondInteger = value;
        }
        if (result.firstInteger.has_value() && result.secondInteger.has_value() &&
            (result.name == "..="
                ? *result.firstInteger > *result.secondInteger
                : *result.firstInteger >= *result.secondInteger))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                start,
                result.name == "..="
                    ? "an inclusive range pattern requires its lower bound not to exceed its upper bound"
                    : "a half-open range pattern requires its lower bound to be less than its upper bound");
        }
    }
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parsePattern(bool implicitBinding)
{
    return PatternIterationParser(*this, implicitBinding).run();
}

bool Parser::patternHasBindings(const Pattern& pattern) const
{
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding) return true;
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
    return false;
}

std::vector<Parser::Pattern> Parser::expandPatternAlternatives(const Pattern& pattern) const
{
    return PatternAlternativeExpander(pattern).run();
}

ast::AstNode* Parser::makeBooleanLiteral(const SourceRange& range, bool value)
{
    ast::AstNode* literal = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BooleanLiteralExpression, range);
    literal->setTokenKind(value ? TokenKind::KwTrue : TokenKind::KwFalse);
    literal->setIntegerValue(value ? 1U : 0U);
    return literal;
}

bool Parser::patternIsConditionOnly(const Pattern& pattern) const
{
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding ||
            currentPattern.kind == PatternKind::Tuple ||
            currentPattern.kind == PatternKind::List)
            return false;
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
    return true;
}

ast::AstNode* Parser::makePatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    return PatternConditionBuilder(*this, pattern, source).run();
}

ast::AstNode* Parser::lowerPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    return PatternLoweringMachine(
        *this,
        pattern,
        source,
        continuation,
        temporaryPrefix,
        emitBindings).run();
}

void Parser::collectUniquePatternBindings(
    const Pattern& pattern,
    std::vector<std::pair<std::string, PatternBindingMode>>& bindings) const
{
    std::unordered_set<std::string> names;
    names.reserve(bindings.size());
    for (const auto& binding : bindings) names.insert(binding.first);
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding)
        {
            if (names.insert(currentPattern.name).second)
                bindings.emplace_back(currentPattern.name, currentPattern.bindingMode);
            continue;
        }
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
}

void Parser::applyPatternBindingMode(
    ast::AstNode* declaration,
    PatternBindingMode mode) const
{
    if (mode == PatternBindingMode::Reference)
        declaration->setAuxiliaryText("local:pattern-ref");
    else if (mode == PatternBindingMode::ReferenceMutable)
        declaration->setAuxiliaryText("local:pattern-ref-mut");
}

Parser::PatternBindingEmitter::PatternBindingEmitter(
    Parser& parser,
    const Pattern& root,
    PatternExpressionFactory source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
    : parser_(parser),
      root_(root),
      rootSource_(std::move(source)),
      destination_(destination),
      temporaryPrefix_(temporaryPrefix)
{
    sources_.push_back(SourcePlan{});
}

void Parser::PatternBindingEmitter::run()
{
    scheduleEmit(root_, 0, destination_);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
}

void Parser::PatternBindingEmitter::schedule(Work work)
{
    work_.push_back(std::move(work));
}

void Parser::PatternBindingEmitter::scheduleEmit(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    Work work;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.destination = destination;
    schedule(std::move(work));
}

void Parser::PatternBindingEmitter::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::Emit: return emit(*work.pattern, work.sourcePlan, work.destination);
        case WorkKind::ContinuePositional: return continuePositional(work.state);
        case WorkKind::FinishPositionalChild:
            return finishPositionalChild(work.state, work.sourcePlan);
        case WorkKind::ContinueOrBinding: return continueOrBinding(work.state);
        case WorkKind::ContinueOrAlternative: return continueOrAlternative(work.state);
        case WorkKind::FinishOrAlternative: return finishOrAlternative(work.state);
        case WorkKind::FinishOrBinding: return finishOrBinding(work.state);
    }
}

std::size_t Parser::PatternBindingEmitter::addSource(
    std::size_t parent,
    SourceKind kind,
    SourceRange range,
    SourceRange containerRange,
    std::string text,
    std::size_t value,
    std::size_t extent)
{
    sources_.push_back(SourcePlan{
        parent, kind, range, containerRange, std::move(text), value, extent});
    return sources_.size() - 1U;
}

ast::AstNode* Parser::PatternBindingEmitter::evaluateSource(std::size_t sourcePlan)
{
    std::vector<SourceWork> pending{{SourceWorkKind::Build, sourcePlan, nullptr}};
    std::vector<ast::AstNode*> values;
    while (!pending.empty())
    {
        SourceWork work = pending.back();
        pending.pop_back();
        dispatchSourceWork(work, pending, values);
    }
    return values.back();
}

void Parser::PatternBindingEmitter::dispatchSourceWork(
    SourceWork work,
    std::vector<SourceWork>& pending,
    std::vector<ast::AstNode*>& values)
{
    const SourcePlan& source = sources_[work.sourcePlan];
    if (work.kind == SourceWorkKind::Build && source.kind == SourceKind::Root)
        return values.push_back(rootSource_());
    if (work.kind == SourceWorkKind::Build && source.kind == SourceKind::Identifier)
        return values.push_back(parser_.makeIdentifierExpression(source.range, source.text));
    if (work.kind == SourceWorkKind::Build && source.kind == SourceKind::MatchValue)
        return values.push_back(parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::MatchValueExpression, source.range));
    if (work.kind == SourceWorkKind::FinishFromEndFirst)
    {
        SourceWork finish{SourceWorkKind::FinishFromEndSecond, work.sourcePlan, values.back()};
        values.pop_back();
        pending.push_back(finish);
        return pending.push_back({SourceWorkKind::Build, source.parent, nullptr});
    }
    if (work.kind == SourceWorkKind::FinishRestFirst)
    {
        SourceWork finish{SourceWorkKind::FinishRestSecond, work.sourcePlan, values.back()};
        values.pop_back();
        pending.push_back(finish);
        return pending.push_back({SourceWorkKind::Build, source.parent, nullptr});
    }
    if (work.kind == SourceWorkKind::FinishFromEndSecond)
        return finishFromEndSource(work, values);
    if (work.kind == SourceWorkKind::FinishRestSecond)
        return finishRestSource(work, values);
    if (work.kind == SourceWorkKind::FinishMember)
        values.back() = parser_.makeMemberExpression(values.back(), source.text);
    else if (work.kind == SourceWorkKind::FinishFixedIndex)
        values.back() = parser_.makeIndexExpression(values.back(),
            parser_.makeIntegerLiteralExpression(source.containerRange, source.value));
    else
    {
        SourceWorkKind finish = SourceWorkKind::FinishMember;
        if (source.kind == SourceKind::FixedIndex) finish = SourceWorkKind::FinishFixedIndex;
        else if (source.kind == SourceKind::FromEndIndex)
            finish = SourceWorkKind::FinishFromEndFirst;
        else if (source.kind == SourceKind::RestSlice)
            finish = SourceWorkKind::FinishRestFirst;
        pending.push_back({finish, work.sourcePlan, nullptr});
        pending.push_back({SourceWorkKind::Build, source.parent, nullptr});
    }
}

void Parser::PatternBindingEmitter::finishFromEndSource(
    const SourceWork& work,
    std::vector<ast::AstNode*>& values)
{
    const SourcePlan& source = sources_[work.sourcePlan];
    ast::AstNode* receiver = values.back();
    values.pop_back();
    ast::AstNode* length = parser_.makeMemberExpression(work.first, "Length");
    length->setAuxiliaryText("list-pattern-length");
    ast::AstNode* index = parser_.makeBinaryExpression(
        TokenKind::Minus, length,
        parser_.makeIntegerLiteralExpression(source.containerRange, source.value));
    values.push_back(parser_.makeIndexExpression(receiver, index));
}

void Parser::PatternBindingEmitter::finishRestSource(
    const SourceWork& work,
    std::vector<ast::AstNode*>& values)
{
    const SourcePlan& source = sources_[work.sourcePlan];
    ast::AstNode* lengthBase = values.back();
    values.pop_back();
    ast::AstNode* call = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::CallExpression, source.range);
    call->setFirst(parser_.makeMemberExpression(work.first, "Slice"));
    call->setAuxiliaryText("list-pattern-rest");
    call->addChild(parser_.makeIntegerLiteralExpression(source.containerRange, source.value));
    ast::AstNode* length = parser_.makeMemberExpression(lengthBase, "Length");
    length->setAuxiliaryText("list-pattern-length");
    call->addChild(parser_.makeBinaryExpression(
        TokenKind::Minus, length,
        parser_.makeIntegerLiteralExpression(source.containerRange, source.extent)));
    values.push_back(call);
}

Parser::PatternExpressionFactory Parser::PatternBindingEmitter::makeSourceFactory(
    std::size_t sourcePlan)
{
    return [this, sourcePlan] { return evaluateSource(sourcePlan); };
}

void Parser::PatternBindingEmitter::emit(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    if (pattern.kind == PatternKind::Or && parser_.patternHasBindings(pattern))
        return startOr(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::Binding)
        return emitSingle(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::Tuple)
        return emitTuple(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::Positional)
        return startPositional(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::Struct)
        return emitStruct(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::List)
        return emitList(pattern, sourcePlan, destination);
    if (pattern.kind == PatternKind::Variant && !pattern.children.empty())
        emitVariant(pattern, sourcePlan, destination);
}

void Parser::PatternBindingEmitter::emitSingle(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    ast::AstNode* initializer = evaluateSource(sourcePlan);
    const bool mutableRestView =
        pattern.bindingMode == PatternBindingMode::Value && initializer != nullptr &&
        initializer->kind() == ast::AstNodeKind::CallExpression &&
        initializer->auxiliaryText() == "list-pattern-rest";
    const bool immutable = !mutableRestView &&
        (pattern.bindingMode == PatternBindingMode::Value ||
         pattern.bindingMode == PatternBindingMode::Copy ||
         pattern.bindingMode == PatternBindingMode::Reference);
    ast::AstNode* declaration = parser_.makeVariableDeclaration(
        type, pattern.name, initializer, false, immutable);
    parser_.applyPatternBindingMode(declaration, pattern.bindingMode);
    destination->addChild(declaration);
}

void Parser::PatternBindingEmitter::emitTuple(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    ast::AstNode* deconstruction = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(evaluateSource(sourcePlan));
    std::vector<std::size_t> childSources;
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = temporaryPrefix_ + "_tuple_" +
            std::to_string(parser_.nextMatchTemporary_++) + "_" + std::to_string(index);
        deconstruction->addChild(parser_.makeIdentifierExpression(pattern.range, name));
        childSources.push_back(addSource(
            0, SourceKind::Identifier, pattern.range, {}, name));
    }
    destination->addChild(deconstruction);
    for (std::size_t index = pattern.children.size(); index-- > 0U;)
        scheduleEmit(pattern.children[index], childSources[index], destination);
}

void Parser::PatternBindingEmitter::startPositional(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    ast::AstNode* deconstruction = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(evaluateSource(sourcePlan));
    deconstruction->setTypeSyntax(parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::Identifier, pattern.name));
    PositionalState state;
    state.pattern = &pattern;
    state.destination = destination;
    state.condition = parser_.makeBooleanLiteral(pattern.range, true);
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        state.names.push_back(temporaryPrefix_ + "_pos_" +
            std::to_string(parser_.nextMatchTemporary_++) + "_" + std::to_string(index));
        deconstruction->addChild(
            parser_.makeIdentifierExpression(pattern.range, state.names.back()));
    }
    destination->addChild(deconstruction);
    positionalStates_.push_back(std::move(state));
    schedule({WorkKind::ContinuePositional, nullptr, 0, nullptr,
        positionalStates_.size() - 1U});
}

void Parser::PatternBindingEmitter::continuePositional(std::size_t stateIndex)
{
    PositionalState& state = positionalStates_[stateIndex];
    if (state.nextChild == state.pattern->children.size())
    {
        if (state.destination->kind() == ast::AstNodeKind::MatchArmExpression)
        {
            if (ast::AstNode* guard = state.destination->second(); guard != nullptr)
                state.condition = parser_.makeBinaryExpression(
                    TokenKind::AmpAmp, state.condition, guard);
            state.destination->setSecond(state.condition);
        }
        return;
    }
    const std::size_t source = addSource(
        0, SourceKind::Identifier, state.pattern->range, {}, state.names[state.nextChild]);
    schedule({WorkKind::FinishPositionalChild, nullptr, source, nullptr, stateIndex});
    scheduleEmit(state.pattern->children[state.nextChild], source, state.destination);
}

void Parser::PatternBindingEmitter::finishPositionalChild(
    std::size_t stateIndex,
    std::size_t sourcePlan)
{
    PositionalState& state = positionalStates_[stateIndex];
    const std::size_t child = state.nextChild;
    state.condition = parser_.makeBinaryExpression(
        TokenKind::AmpAmp, state.condition,
        parser_.makePatternCondition(
            state.pattern->children[child], makeSourceFactory(sourcePlan)));
    ++state.nextChild;
    schedule({WorkKind::ContinuePositional, nullptr, 0, nullptr, stateIndex});
}

void Parser::PatternBindingEmitter::emitStruct(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    std::vector<std::size_t> childSources;
    childSources.reserve(pattern.children.size());
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
        childSources.push_back(addSource(
            sourcePlan, SourceKind::Member, pattern.range, {}, pattern.fieldNames[index]));
    for (std::size_t index = pattern.children.size(); index-- > 0U;)
        scheduleEmit(pattern.children[index], childSources[index], destination);
}

void Parser::PatternBindingEmitter::emitList(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    const std::size_t restOffset = pattern.restIndex.value_or(pattern.children.size());
    std::vector<std::size_t> childSources;
    childSources.reserve(pattern.children.size());
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        SourceKind kind = SourceKind::FixedIndex;
        std::size_t value = index;
        std::size_t fixedCount = 0;
        if (pattern.restIndex == index)
        {
            kind = SourceKind::RestSlice;
            value = restOffset;
            fixedCount = pattern.children.size() - 1U;
        }
        else if (pattern.restIndex.has_value() && index > restOffset)
        {
            kind = SourceKind::FromEndIndex;
            value = pattern.children.size() - index;
        }
        childSources.push_back(addSource(
            sourcePlan, kind, pattern.children[index].range,
            pattern.range, {}, value, fixedCount));
    }
    for (std::size_t index = pattern.children.size(); index-- > 0U;)
        scheduleEmit(pattern.children[index], childSources[index], destination);
}

void Parser::PatternBindingEmitter::emitVariant(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    const std::string payloadName = temporaryPrefix_ + "_payload_" +
        std::to_string(parser_.nextMatchTemporary_++);
    ast::AstNode* extract = parser_.makeZeroArgCallExpression(parser_.makeMemberExpression(
        evaluateSource(sourcePlan), "__nsc_variant_extract_" + pattern.name));
    extract->setAuxiliaryText("variant-pattern-extract");
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    destination->addChild(
        parser_.makeVariableDeclaration(type, payloadName, extract, false, true));
    const std::size_t payload = addSource(
        0, SourceKind::Identifier, pattern.range, {}, payloadName);
    scheduleEmit(pattern.children.front(), payload, destination);
}

void Parser::PatternBindingEmitter::startOr(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* destination)
{
    OrState state;
    state.pattern = &pattern;
    state.sourcePlan = sourcePlan;
    state.destination = destination;
    state.alternatives = parser_.expandPatternAlternatives(pattern);
    if (!state.alternatives.empty())
        parser_.collectUniquePatternBindings(state.alternatives.front(), state.bindings);
    orStates_.push_back(std::move(state));
    schedule({WorkKind::ContinueOrBinding, nullptr, 0, nullptr, orStates_.size() - 1U});
}

void Parser::PatternBindingEmitter::continueOrBinding(std::size_t stateIndex)
{
    OrState& state = orStates_[stateIndex];
    if (state.nextBinding == state.bindings.size()) return;
    state.selector = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::MatchExpression, state.pattern->range);
    state.selector->setFirst(evaluateSource(state.sourcePlan));
    state.selector->setText(temporaryPrefix_ + "_or_binding_" +
        std::to_string(parser_.nextMatchTemporary_++));
    state.nextAlternative = 0;
    schedule({WorkKind::ContinueOrAlternative, nullptr, 0, nullptr, stateIndex});
}

void Parser::PatternBindingEmitter::continueOrAlternative(std::size_t stateIndex)
{
    OrState& state = orStates_[stateIndex];
    if (state.nextAlternative == state.alternatives.size())
    {
        schedule({WorkKind::FinishOrBinding, nullptr, 0, nullptr, stateIndex});
        return;
    }
    const Pattern& alternative = state.alternatives[state.nextAlternative];
    state.arm = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::MatchArmExpression, alternative.range);
    const std::size_t matchValue = addSource(
        0, SourceKind::MatchValue, alternative.range);
    state.arm->setFirst(
        parser_.makePatternCondition(alternative, makeSourceFactory(matchValue)));
    schedule({WorkKind::FinishOrAlternative, nullptr, 0, nullptr, stateIndex});
    scheduleEmit(alternative, matchValue, state.arm);
}

void Parser::PatternBindingEmitter::finishOrAlternative(std::size_t stateIndex)
{
    OrState& state = orStates_[stateIndex];
    const Pattern& alternative = state.alternatives[state.nextAlternative];
    const auto& [name, mode] = state.bindings[state.nextBinding];
    state.arm->setThird(parser_.makeIdentifierExpression(alternative.range, name));
    state.arm->setFlag(mode == PatternBindingMode::Reference ||
        mode == PatternBindingMode::ReferenceMutable);
    state.selector->addChild(state.arm);
    ++state.nextAlternative;
    schedule({WorkKind::ContinueOrAlternative, nullptr, 0, nullptr, stateIndex});
}

void Parser::PatternBindingEmitter::finishOrBinding(std::size_t stateIndex)
{
    OrState& state = orStates_[stateIndex];
    const auto& [name, mode] = state.bindings[state.nextBinding];
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        state.pattern->range, TokenKind::KwVar, "var");
    const bool immutable = mode == PatternBindingMode::Value ||
        mode == PatternBindingMode::Copy || mode == PatternBindingMode::Reference;
    ast::AstNode* declaration = parser_.makeVariableDeclaration(
        type, name, state.selector, false, immutable);
    parser_.applyPatternBindingMode(declaration, mode);
    state.destination->addChild(declaration);
    ++state.nextBinding;
    schedule({WorkKind::ContinueOrBinding, nullptr, 0, nullptr, stateIndex});
}

void Parser::emitPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    PatternBindingEmitter(*this, pattern, source, destination, temporaryPrefix).run();
}
}
