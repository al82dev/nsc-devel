#include "ComptimeFunctionEvaluator.h"
#include "ComptimeDeclaration.h"
#include "ComptimeTask.h"
#include "ComptimeWalkItem.h"
#include "IntegerExpression.h"
#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend::internal
{
namespace
{
using ast::AstNode;
using ast::AstNodeKind;
using lexer::TokenKind;
using Declaration = ComptimeDeclaration;
using Environment = ComptimeEnvironment;
using Task = ComptimeTask;
using WalkItem = ComptimeWalkItem;

constexpr std::size_t kParseBytesPerSourceByte = 2048;
constexpr std::size_t kMaximumParameters = 16;

// The returned nodes remain valid for the lifetime of arena.
AstNode* parseSource(std::string text, ast::AstArena& arena, std::string& error)
{
    source::SourceFile file(FileId(1), "comptime-source.ns", std::move(text));
    diagnostics::DiagnosticEngine diagnostics;
    lexer::Lexer lexer(file, diagnostics);
    const auto tokens = lexer.lexAll();
    if (diagnostics.hasErrors())
    {
        error = "comptime sandbox forbids source that cannot be lexed for a function call";
        return nullptr;
    }
    parser::Parser parser(tokens, arena, diagnostics);
    AstNode* root = parser.parseCompilationUnit();
    if (diagnostics.hasErrors())
    {
        error = "comptime sandbox forbids source that cannot be parsed for a function call";
        return nullptr;
    }
    return root;
}

// Mask directives while preserving byte offsets and source line breaks.
bool maskRunDirectives(
    std::string& text, std::vector<std::size_t>& directiveOffsets, std::string& error)
{
    enum class State { Code, Line, Block, String, Character };
    State state = State::Code;
    bool escape = false;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        const char next = i + 1 < text.size() ? text[i + 1] : '\0';
        if (state == State::Line) { if (c == '\n') state = State::Code; continue; }
        if (state == State::Block)
        {
            if (c == '*' && next == '/') { state = State::Code; ++i; }
            continue;
        }
        if (state == State::String || state == State::Character)
        {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if ((state == State::String && c == '"') ||
                     (state == State::Character && c == '\'')) state = State::Code;
            continue;
        }
        if (c == '/' && next == '/') { state = State::Line; ++i; continue; }
        if (c == '/' && next == '*') { state = State::Block; ++i; continue; }
        if (c == '"') { state = State::String; continue; }
        if (c == '\'') { state = State::Character; continue; }
        if (text.compare(i, 8, "comptime") == 0 &&
            (i == 0 || !identifierByte(text[i - 1])) &&
            (i + 8 == text.size() || !identifierByte(text[i + 8])))
        {
            text.replace(i, 8, 8, ' ');
            i += 7;
            continue;
        }
        if (c != '#') continue;
        if (text.compare(i, 4, "#run") != 0)
        {
            error = "comptime sandbox forbids other directives while resolving a function call";
            return false;
        }
        std::size_t open = i + 4;
        while (open < text.size() && (text[open] == ' ' || text[open] == '\t' ||
               text[open] == '\r' || text[open] == '\n')) ++open;
        if (open == text.size() || text[open] != '(')
        {
            error = "expected '(' after #run";
            return false;
        }
        std::size_t end = open + 1;
        std::size_t depth = 1;
        while (end < text.size() && depth)
        {
            if (text[end] == '(') ++depth;
            else if (text[end] == ')') --depth;
            ++end;
        }
        if (depth)
        {
            error = "unterminated #run expression";
            return false;
        }
        directiveOffsets.push_back(i);
        text[i] = '0';
        for (std::size_t j = i + 1; j < end; ++j)
            if (text[j] != '\n' && text[j] != '\r') text[j] = ' ';
        i = end - 1;
    }
    return true;
}

// Plain int values do not carry reference, ownership or compound type modifiers.
bool plainInt(const ast::TypeSyntax* type)
{
    return type != nullptr && type->baseTokenKind() == TokenKind::KwInt &&
        type->modifiers().empty() && type->typeArguments().empty() &&
        type->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None &&
        type->referenceQualifier() == ast::ReferenceQualifierSyntaxKind::None;
}

// These nodes can introduce bindings not yet resolved by this early stage.
bool unsupportedBindingScope(AstNodeKind kind)
{
    return kind == AstNodeKind::LambdaExpression ||
        kind == AstNodeKind::DeconstructionDeclarationStatement ||
        kind == AstNodeKind::ForeachStatement || kind == AstNodeKind::MatchExpression ||
        kind == AstNodeKind::CatchClause || kind == AstNodeKind::UsingStatement;
}

// Names in these declarations can hide a function with the same spelling.
bool canHideFunction(AstNodeKind kind)
{
    return kind == AstNodeKind::FunctionDeclaration || kind == AstNodeKind::MethodDeclaration ||
        kind == AstNodeKind::ParameterDeclaration || kind == AstNodeKind::VariableDeclarationStatement ||
        kind == AstNodeKind::FieldDeclaration || kind == AstNodeKind::PropertyDeclaration ||
        kind == AstNodeKind::EventDeclaration || kind == AstNodeKind::EnumMemberDeclaration ||
        kind == AstNodeKind::AssociatedConstantDeclaration || kind == AstNodeKind::ClassDeclaration ||
        kind == AstNodeKind::StructDeclaration || kind == AstNodeKind::EnumDeclaration ||
        kind == AstNodeKind::AssociatedTypeDeclaration || kind == AstNodeKind::InterfaceDeclaration ||
        kind == AstNodeKind::DelegateDeclaration || kind == AstNodeKind::NamespaceDeclaration ||
        kind == AstNodeKind::GlobalVariableDeclaration;
}

// Reject scopes whose implicit receiver cannot be resolved at this stage.
bool inspectContainingScope(
    const AstNode* node, std::size_t offset, bool& containingType,
    bool& containingStaticMethod, std::string& error)
{
    const bool contains = node->range().isValid() &&
        node->range().begin().offset() <= offset && offset < node->range().end().offset();
    if (!contains) return true;
    if (node->kind() == AstNodeKind::ClassDeclaration ||
        node->kind() == AstNodeKind::StructDeclaration)
    {
        containingType = true;
        if (!node->typeSyntaxes().empty() || !node->genericParameters().empty())
        {
            error = "comptime sandbox forbids unresolved inherited or generic function lookup";
            return false;
        }
    }
    if (node->kind() == AstNodeKind::InterfaceDeclaration ||
        node->kind() == AstNodeKind::ExtensionDeclaration)
    {
        error = "comptime sandbox forbids unresolved interface or extension lookup";
        return false;
    }
    if (node->kind() == AstNodeKind::MethodDeclaration && node->hasModifier(TokenKind::KwStatic))
        containingStaticMethod = true;
    return true;
}

// A masked, not-yet-evaluated directive cannot be treated as a literal in a callee.
bool hasPendingDirective(const AstNode* callable, const std::vector<std::size_t>& offsets)
{
    const AstNode* body = callable->first();
    if (body == nullptr || !body->range().isValid()) return false;
    const auto begin = body->range().begin().offset();
    const auto end = body->range().end().offset();
    for (std::size_t offset : offsets)
        if (offset >= begin && offset < end) return true;
    return false;
}

// State-bearing types can perform initialization outside a method body.
bool typeMayInitializeState(const AstNode* type)
{
    for (const AstNode* member : type->children())
        if (member->kind() == AstNodeKind::FieldDeclaration ||
            member->kind() == AstNodeKind::PropertyDeclaration ||
            member->kind() == AstNodeKind::EventDeclaration ||
            member->kind() == AstNodeKind::ConstructorDeclaration)
            return true;
    return false;
}

// Build a conservative declaration inventory before attempting any execution.
bool inventory(
    const AstNode* root, std::size_t offset, const std::vector<std::size_t>& directiveOffsets,
    std::vector<Declaration>& functions,
    std::vector<std::string>& otherNames, std::string& callerNamespace, std::string& callerClass,
    const ComptimeLimits& limits, std::size_t& steps,
    const std::function<bool()>& cancelled, std::string& error)
{
    std::vector<WalkItem> work{{root, {}, {}, true, false, false}};
    std::unordered_set<const AstNode*> visited;
    std::size_t namespaceDepth = 0;
    bool containingClass = false;
    bool containingStaticMethod = false;
    while (!work.empty())
    {
        WalkItem item = std::move(work.back());
        work.pop_back();
        const AstNode* node = item.node;
        if (node == nullptr || !visited.insert(node).second) continue;
        if (cancelled && cancelled()) { error = "comptime evaluation cancelled"; return false; }
        if (steps >= limits.maximumSteps) { error = "comptime step budget exceeded"; return false; }
        ++steps;
        if (node->kind() == AstNodeKind::UsingNamespaceDirective ||
            node->kind() == AstNodeKind::UsingAliasDirective)
        {
            error = "comptime sandbox forbids function lookup through imports";
            return false;
        }
        if (unsupportedBindingScope(node->kind()))
        {
            error = "comptime sandbox forbids unsupported binding scopes";
            return false;
        }
        if (node->kind() == AstNodeKind::NamespaceDeclaration)
        {
            if (!item.nameSpace.empty()) item.nameSpace += ".";
            item.nameSpace += node->text();
            if (node->range().isValid() && node->range().begin().offset() <= offset &&
                offset < node->range().end().offset() && item.nameSpace.size() >= namespaceDepth)
            {
                callerNamespace = item.nameSpace;
                namespaceDepth = item.nameSpace.size();
            }
        }
        if (node->kind() == AstNodeKind::ClassDeclaration ||
            node->kind() == AstNodeKind::StructDeclaration)
        {
            if (!item.ownerClass.empty()) item.ownerClass += ".";
            item.ownerClass += node->text();
            item.ownerHasState = typeMayInitializeState(node);
            if (node->range().isValid() && node->range().begin().offset() <= offset &&
                offset < node->range().end().offset())
                callerClass = item.ownerClass;
        }
        if ((node->kind() == AstNodeKind::FunctionDeclaration && item.namespaceMember) ||
            (node->kind() == AstNodeKind::MethodDeclaration && item.classMember &&
             node->hasModifier(TokenKind::KwStatic)))
            functions.push_back({node, item.nameSpace, item.ownerClass,
                                 hasPendingDirective(node, directiveOffsets),
                                 item.classMember && item.ownerHasState});
        else if (canHideFunction(node->kind()))
            otherNames.push_back(node->text());
        if (!inspectContainingScope(node, offset, containingClass, containingStaticMethod, error))
            return false;
        const bool namespaceMember = node->kind() == AstNodeKind::CompilationUnit ||
            node->kind() == AstNodeKind::NamespaceDeclaration;
        const bool classMember = node->kind() == AstNodeKind::ClassDeclaration ||
            node->kind() == AstNodeKind::StructDeclaration;
        for (const AstNode* child : node->children())
            work.push_back({child, item.nameSpace, item.ownerClass, namespaceMember, classMember, item.ownerHasState});
        work.push_back({node->first(), item.nameSpace, item.ownerClass, false, false, item.ownerHasState});
        work.push_back({node->second(), item.nameSpace, item.ownerClass, false, false, item.ownerHasState});
        work.push_back({node->third(), item.nameSpace, item.ownerClass, false, false, item.ownerHasState});
    }
    if (containingClass && !containingStaticMethod)
    {
        error = "comptime sandbox forbids unresolved instance function lookup";
        return false;
    }
    return true;
}

// No name is selected when it is ambiguous or could name a local value.
const Declaration* findFunction(
    std::string_view name, std::string_view nameSpace, std::string_view ownerClass, std::size_t arity,
    bool qualified, const std::vector<Declaration>& functions, const std::vector<std::string>& otherNames,
    std::string& error)
{
    if (arity > kMaximumParameters)
    {
        error = "comptime sandbox forbids too many function parameters";
        return nullptr;
    }
    if (std::ranges::find(otherNames, name) != otherNames.end())
    {
        error = "comptime sandbox forbids a potentially shadowed function name";
        return nullptr;
    }
    const Declaration* found = nullptr;
    for (const Declaration& declaration : functions)
    {
        if (declaration.node->text() != name) continue;
        if (qualified && (declaration.ownerClass != ownerClass || declaration.nameSpace != nameSpace))
            continue;
        if (found != nullptr || declaration.nameSpace != nameSpace ||
            (!declaration.ownerClass.empty() && declaration.ownerClass != ownerClass))
        {
            error = "comptime sandbox forbids ambiguous function lookup";
            return nullptr;
        }
        found = &declaration;
    }
    if (found == nullptr)
    {
        error = "comptime sandbox forbids unresolved or external function calls";
        return nullptr;
    }
    return found;
}

// A function is eligible only if it has a single, explicitly evaluable return.
bool supportedFunction(const AstNode* function, std::size_t arity, std::string& error)
{
    if (!plainInt(function->typeSyntax()) || function->flag() || function->isUnsafe() ||
        function->isReference() || function->isVariadic() || function->throwsTypeSyntax() != nullptr ||
        !function->auxiliaryText().empty() || !function->genericParameters().empty() ||
        function->children().size() != arity ||
        arity > kMaximumParameters || function->first() == nullptr ||
        function->first()->kind() != AstNodeKind::BlockStatement ||
        function->first()->children().size() != 1 ||
        function->first()->children()[0]->kind() != AstNodeKind::ReturnStatement ||
        function->first()->children()[0]->first() == nullptr)
    {
        error = "comptime sandbox forbids unsupported function signatures or effects";
        return false;
    }
    for (const AstNode* parameter : function->children())
    {
        if (parameter->kind() != AstNodeKind::ParameterDeclaration ||
            !plainInt(parameter->typeSyntax()) || parameter->isReference() ||
            !parameter->auxiliaryText().empty() || parameter->first() != nullptr)
        {
            error = "comptime sandbox forbids unsupported function parameters";
            return false;
        }
    }
    return true;
}

std::int32_t fromBits(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

// Compute wrapping int operations in unsigned arithmetic to avoid host UB.
bool binary(
    TokenKind op, std::int32_t a, std::int32_t b, std::int32_t& result, std::string& error)
{
    const auto ua = std::bit_cast<std::uint32_t>(a);
    const auto ub = std::bit_cast<std::uint32_t>(b);
    switch (op)
    {
    case TokenKind::Plus: result = fromBits(ua + ub); return true;
    case TokenKind::Minus: result = fromBits(ua - ub); return true;
    case TokenKind::Star: result = fromBits(ua * ub); return true;
    case TokenKind::Amp: result = fromBits(ua & ub); return true;
    case TokenKind::Pipe: result = fromBits(ua | ub); return true;
    case TokenKind::Caret: result = fromBits(ua ^ ub); return true;
    case TokenKind::Slash:
    case TokenKind::Percent:
        if (b == 0 || (a == std::numeric_limits<std::int32_t>::min() && b == -1))
        {
            error = "comptime integer division trap";
            return false;
        }
        result = op == TokenKind::Slash ? a / b : a % b;
        return true;
    default: error = "comptime sandbox forbids unsupported integer operator"; return false;
    }
}

// A qualified call is accepted only when its receiver is the current, unshadowed type.
std::optional<std::string_view> callName(
    const AstNode* call, const Task& task, const std::vector<Declaration>& functions,
    const std::vector<std::string>& otherNames, bool& qualified, std::string& error)
{
    const AstNode* target = call->first();
    if (target != nullptr && target->kind() == AstNodeKind::IdentifierExpression)
        return target->text();
    if (target == nullptr || target->kind() != AstNodeKind::MemberExpression ||
        target->first() == nullptr || target->first()->kind() != AstNodeKind::IdentifierExpression ||
        task.ownerClass.empty() || target->first()->text() != task.ownerClass ||
        std::ranges::count(otherNames, task.ownerClass) != 1)
    {
        error = "comptime sandbox forbids unsupported qualified function lookup";
        return std::nullopt;
    }
    for (const Declaration& declaration : functions)
        if (declaration.node->text() == task.ownerClass)
        {
            error = "comptime sandbox forbids a potentially shadowed type name";
            return std::nullopt;
        }
    qualified = true;
    return target->text();
}

// Schedule children on an explicit stack. Arguments execute once, left to right.
bool schedule(
    const Task& task, std::vector<Task>& work, std::vector<std::int32_t>& values,
    const std::vector<Declaration>& functions, const std::vector<std::string>& otherNames,
    std::string& error)
{
    const AstNode* node = task.node;
    if (node == nullptr) { error = "invalid comptime expression"; return false; }
    if (node->kind() == AstNodeKind::IntegerLiteralExpression)
    {
        if (node->integerValue() > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            error = "comptime sandbox forbids integer literals outside int";
            return false;
        }
        for (char c : node->text())
            if ((c < '0' || c > '9') && c != '_')
            {
                error = "comptime sandbox forbids non-decimal or suffixed integer literals";
                return false;
            }
        values.push_back(static_cast<std::int32_t>(node->integerValue()));
        return true;
    }
    if (node->kind() == AstNodeKind::IdentifierExpression)
    {
        const auto found = task.environment->find(node->text());
        if (found == task.environment->end())
        {
            error = "comptime sandbox forbids reading non-parameter state";
            return false;
        }
        values.push_back(found->second);
        return true;
    }
    if (node->kind() == AstNodeKind::UnaryExpression ||
        node->kind() == AstNodeKind::BinaryExpression)
    {
        work.push_back({node, 1, task.depth, task.environment, task.nameSpace, task.ownerClass});
        if (node->kind() == AstNodeKind::BinaryExpression)
            work.push_back({node->second(), 0, task.depth, task.environment, task.nameSpace, task.ownerClass});
        work.push_back({node->first(), 0, task.depth, task.environment, task.nameSpace, task.ownerClass});
        return true;
    }
    if (node->kind() != AstNodeKind::CallExpression || node->flag())
    {
        error = "comptime sandbox forbids unsupported expressions or effects";
        return false;
    }
    bool qualified = false;
    const auto name = callName(node, task, functions, otherNames, qualified, error);
    if (!name.has_value()) return false;
    const Declaration* declaration = findFunction(
        *name, task.nameSpace, task.ownerClass, node->children().size(),
        qualified, functions, otherNames, error);
    if (declaration == nullptr || !supportedFunction(declaration->node, node->children().size(), error))
        return false;
    if (declaration->pendingDirective)
    {
        error = "comptime sandbox forbids an unevaluated #run inside a called function";
        return false;
    }
    if (declaration->ownerHasState)
    {
        error = "comptime sandbox forbids potential type initialization";
        return false;
    }
    for (const AstNode* argument : node->children())
        if (!argument->argumentName().empty())
        {
            error = "comptime sandbox forbids named function arguments";
            return false;
        }
    work.push_back({node, 1, task.depth, task.environment, task.nameSpace, task.ownerClass});
    for (auto argument = node->children().rbegin(); argument != node->children().rend(); ++argument)
        work.push_back({*argument, 0, task.depth, task.environment, task.nameSpace, task.ownerClass});
    return true;
}

// Consume evaluated operands and enter a function only after checking its body.
bool finish(
    const Task& task, std::vector<Task>& work, std::vector<std::int32_t>& values,
    const std::vector<Declaration>& functions, const std::vector<std::string>& otherNames,
    std::string& error)
{
    const AstNode* node = task.node;
    if (node->kind() == AstNodeKind::UnaryExpression)
    {
        if (values.empty()) { error = "invalid comptime operand"; return false; }
        const auto bits = std::bit_cast<std::uint32_t>(values.back());
        if (node->tokenKind() == TokenKind::Minus) values.back() = fromBits(0U - bits);
        else if (node->tokenKind() == TokenKind::Tilde) values.back() = fromBits(~bits);
        else if (node->tokenKind() != TokenKind::Plus)
        {
            error = "comptime sandbox forbids unsupported unary operator";
            return false;
        }
        return true;
    }
    if (node->kind() == AstNodeKind::BinaryExpression)
    {
        if (values.size() < 2) { error = "invalid comptime operands"; return false; }
        const std::int32_t right = values.back(); values.pop_back();
        const std::int32_t left = values.back(); values.pop_back();
        std::int32_t result = 0;
        if (!binary(node->tokenKind(), left, right, result, error)) return false;
        values.push_back(result);
        return true;
    }
    bool qualified = false;
    const auto name = callName(node, task, functions, otherNames, qualified, error);
    if (!name.has_value()) return false;
    const Declaration* declaration = findFunction(
        *name, task.nameSpace, task.ownerClass, node->children().size(),
        qualified, functions, otherNames, error);
    if (declaration == nullptr || values.size() < node->children().size()) return false;
    const AstNode* function = declaration->node;
    const std::size_t base = values.size() - node->children().size();
    auto environment = std::make_shared<Environment>();
    for (std::size_t i = 0; i < node->children().size(); ++i)
        if (!environment->emplace(function->children()[i]->text(), values[base + i]).second)
        {
            error = "comptime sandbox forbids duplicate parameter names";
            return false;
        }
    values.resize(base);
    work.push_back({function->first()->children()[0]->first(), 0, task.depth + 1,
                    std::move(environment), declaration->nameSpace, declaration->ownerClass});
    return true;
}

// The worklist enforces both total effort and call/expression depth.
std::optional<std::int64_t> execute(
    const AstNode* expression, std::string nameSpace, std::string ownerClass,
    const std::vector<Declaration>& functions,
    const std::vector<std::string>& otherNames, const ComptimeLimits& limits,
    std::size_t& steps, const std::function<bool()>& cancelled, std::string& error)
{
    auto environment = std::make_shared<Environment>();
    std::vector<Task> work{{expression, 0, 0, environment, std::move(nameSpace), std::move(ownerClass)}};
    std::vector<std::int32_t> values;
    while (!work.empty())
    {
        if (cancelled && cancelled()) { error = "comptime evaluation cancelled"; return std::nullopt; }
        if (steps >= limits.maximumSteps) { error = "comptime step budget exceeded"; return std::nullopt; }
        ++steps;
        if (work.size() > limits.maximumNesting || work.back().depth >= limits.maximumNesting)
        {
            error = "comptime nesting limit exceeded";
            return std::nullopt;
        }
        Task task = std::move(work.back()); work.pop_back();
        const bool ok = task.phase == 0
            ? schedule(task, work, values, functions, otherNames, error)
            : finish(task, work, values, functions, otherNames, error);
        if (!ok) return std::nullopt;
    }
    if (values.size() != 1) { error = "invalid comptime result"; return std::nullopt; }
    return values.back();
}
}

// Parse only a bounded source snapshot and reject constructs requiring project-wide binding.
std::optional<std::int64_t> evaluateComptimeFunction(
    std::string_view source, std::string_view expression, std::size_t directiveOffset,
    const ComptimeLimits& limits, std::size_t& steps, std::size_t& memoryUsed,
    const std::function<bool()>& cancellationRequested, std::string& error)
{
    if (source.size() > maximumComptimeFunctionSourceBytes ||
        expression.size() > maximumComptimeFunctionSourceBytes)
    {
        error = "comptime function source limit exceeded";
        return std::nullopt;
    }
    if (source.find("partial") != std::string_view::npos ||
        source.find("\"\"\"") != std::string_view::npos)
    {
        error = "comptime sandbox forbids partial declarations and raw strings in function lookup";
        return std::nullopt;
    }
    const std::size_t cost = (source.size() + expression.size() + 32) * kParseBytesPerSourceByte;
    if (memoryUsed > limits.maximumMemoryBytes || cost > limits.maximumMemoryBytes - memoryUsed)
    {
        error = "comptime memory budget exceeded while parsing function source";
        return std::nullopt;
    }
    memoryUsed += cost;
    const std::size_t parseSteps = source.size() + expression.size();
    if (steps > limits.maximumSteps || parseSteps > limits.maximumSteps - steps)
    {
        error = "comptime step budget exceeded while parsing function source";
        return std::nullopt;
    }
    steps += parseSteps;
    if (cancellationRequested && cancellationRequested())
    {
        error = "comptime evaluation cancelled";
        return std::nullopt;
    }
    std::string masked(source);
    std::vector<std::size_t> directiveOffsets;
    if (!maskRunDirectives(masked, directiveOffsets, error)) return std::nullopt;
    ast::AstArena sourceArena;
    AstNode* root = parseSource(std::move(masked), sourceArena, error);
    if (root == nullptr) return std::nullopt;
    std::vector<Declaration> functions;
    std::vector<std::string> otherNames;
    std::string nameSpace;
    std::string ownerClass;
    if (!inventory(root, directiveOffset, directiveOffsets, functions, otherNames, nameSpace, ownerClass,
                   limits, steps, cancellationRequested, error))
        return std::nullopt;
    ast::AstArena expressionArena;
    std::string wrapper = "int __comptime_expression() { return ";
    wrapper.append(expression);
    wrapper += "; }";
    AstNode* expressionRoot = parseSource(std::move(wrapper), expressionArena, error);
    if (expressionRoot == nullptr || expressionRoot->children().size() != 1)
        return std::nullopt;
    const AstNode* function = expressionRoot->children()[0];
    if (function->first() == nullptr || function->first()->children().size() != 1)
    {
        error = "comptime sandbox forbids malformed function expressions";
        return std::nullopt;
    }
    const AstNode* evaluated = function->first()->children()[0]->first();
    if (evaluated == nullptr || evaluated->kind() != AstNodeKind::CallExpression)
    {
        error = "comptime sandbox permits only a direct function call";
        return std::nullopt;
    }
    return execute(evaluated, std::move(nameSpace), std::move(ownerClass), functions, otherNames,
                   limits, steps, cancellationRequested, error);
}
}
