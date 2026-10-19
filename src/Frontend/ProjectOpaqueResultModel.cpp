#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"
#include "Internal/LocatedDeclaration.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
using ast::AstNode;
using ast::AstNodeKind;
using ast::TypeSyntax;
using lexer::TokenKind;

using internal::LocatedDeclaration;

std::string qualify(std::string_view prefix, std::string_view name)
{
    if (prefix.empty()) return std::string(name);
    if (name.empty()) return std::string(prefix);
    return std::string(prefix) + "." + std::string(name);
}

std::string typeSpelling(const TypeSyntax* type)
{
    if (type == nullptr) return "<missing>";
    std::string result;
    if (type->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        result += ast::ownershipQualifierSyntaxKindName(type->ownershipQualifier());
        result.push_back(' ');
    }
    if (type->isDynamicTrait()) result += "dyn ";
    if (type->isTuple())
    {
        result.push_back('(');
        for (std::size_t index = 0; index < type->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(type->typeArguments()[index]);
        }
        result.push_back(')');
    }
    else
    {
        result += type->baseName();
        if (!type->typeArguments().empty())
        {
            result.push_back('<');
            for (std::size_t index = 0; index < type->typeArguments().size(); ++index)
            {
                if (index != 0) result.push_back(',');
                result += typeSpelling(type->typeArguments()[index]);
            }
            result.push_back('>');
        }
    }
    if (type->isPointeeConst()) result += " const";
    for (const ast::TypeModifierSyntax& modifier : type->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) result.push_back('*');
        else result += "[]";
    }
    return result;
}

bool containsOpaqueResult(const TypeSyntax* type)
{
    if (type == nullptr) return false;
    if (type->isOpaqueResult()) return true;
    return std::ranges::any_of(type->typeArguments(), [](const TypeSyntax* argument)
    {
        return containsOpaqueResult(argument);
    });
}

const LocatedDeclaration* findDeclaration(
    const std::unordered_map<std::string, LocatedDeclaration>& declarations,
    std::string_view name,
    std::string_view namespaceName)
{
    if (const auto found = declarations.find(std::string(name)); found != declarations.end())
        return &found->second;
    if (name.find('.') == std::string_view::npos && !namespaceName.empty())
        if (const auto found = declarations.find(qualify(namespaceName, name));
            found != declarations.end())
            return &found->second;
    return nullptr;
}

bool typeConformsToTrait(
    const TypeSyntax* concreteType,
    std::string_view traitSpelling,
    std::string_view namespaceName,
    const std::unordered_map<std::string, LocatedDeclaration>& concreteDeclarations,
    const std::vector<LocatedDeclaration>& traitImplementations,
    std::unordered_set<const AstNode*>& visited)
{
    if (concreteType == nullptr) return false;
    const LocatedDeclaration* concrete = findDeclaration(
        concreteDeclarations, concreteType->baseName(), namespaceName);
    if (concrete == nullptr || concrete->syntax == nullptr ||
        !visited.insert(concrete->syntax).second)
        return false;

    std::unordered_map<std::string, const TypeSyntax*> substitutions;
    const auto& parameters = concrete->syntax->genericParameters();
    for (std::size_t index = 0;
         index < parameters.size() && index < concreteType->typeArguments().size(); ++index)
        substitutions.emplace(parameters[index], concreteType->typeArguments()[index]);

    const auto substitutedSpelling = [&](const TypeSyntax* type)
    {
        if (type == nullptr) return std::string{"<missing>"};
        const auto replacement = substitutions.find(type->baseName());
        if (replacement != substitutions.end() && type->typeArguments().empty())
            return typeSpelling(replacement->second);
        TypeSyntax scratch(type->range(), type->baseTokenKind(), type->baseName());
        scratch.setOwnershipQualifier(type->ownershipQualifier());
        scratch.setDynamicTrait(type->isDynamicTrait());
        scratch.setPointeeConst(type->isPointeeConst());
        scratch.setTuple(type->isTuple());
        for (const TypeSyntax* argument : type->typeArguments())
        {
            const auto argumentReplacement = argument == nullptr
                ? substitutions.end() : substitutions.find(argument->baseName());
            scratch.addTypeArgument(
                argumentReplacement != substitutions.end() && argument->typeArguments().empty()
                    ? const_cast<TypeSyntax*>(argumentReplacement->second)
                    : const_cast<TypeSyntax*>(argument));
        }
        for (const ast::TypeModifierSyntax& modifier : type->modifiers())
            scratch.addModifier(modifier);
        return typeSpelling(&scratch);
    };

    for (const TypeSyntax* base : concrete->syntax->typeSyntaxes())
    {
        if (substitutedSpelling(base) == traitSpelling) return true;
        if (typeConformsToTrait(
                base, traitSpelling, concrete->namespaceName,
                concreteDeclarations, traitImplementations, visited))
            return true;
    }
    for (const LocatedDeclaration& implementation : traitImplementations)
    {
        if (implementation.syntax == nullptr || implementation.syntax->typeSyntax() == nullptr)
            continue;
        if (implementation.syntax->text() == concrete->syntax->text() &&
            typeSpelling(implementation.syntax->typeSyntax()) == traitSpelling)
            return true;
    }
    return false;
}

std::string stableSuffix(std::string_view input)
{
    std::uint64_t value = 1469598103934665603ULL;
    for (const unsigned char byte : input)
    {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

TypeSyntax* cloneType(
    ast::AstArena& arena,
    const TypeSyntax* source,
    const std::unordered_map<std::string, const TypeSyntax*>& substitutions = {})
{
    if (source == nullptr) return nullptr;
    if (const auto found = substitutions.find(source->baseName());
        found != substitutions.end() && source->typeArguments().empty())
        return cloneType(arena, found->second);

    TypeSyntax* result = arena.make<TypeSyntax>(
        source->range(), source->baseTokenKind(), source->baseName());
    result->setOwnershipQualifier(source->ownershipQualifier());
    result->setDynamicTrait(source->isDynamicTrait());
    result->setOpaqueResult(source->isOpaqueResult());
    result->setOpaqueResultTraitName(source->opaqueResultTraitName());
    result->setExplicitTraitImplementation(source->isExplicitTraitImplementation());
    result->setPointeeConst(source->isPointeeConst());
    result->setTuple(source->isTuple());
    result->setFunction(source->isFunction());
    result->setFunctionCarrier(source->functionCarrier());
    result->setFunctionCallingConvention(source->functionCallingConvention());
    result->setFunctionVariadic(source->functionIsVariadic());
    result->setFunctionUnsafe(source->functionIsUnsafe());
    result->setFunctionNoHeap(source->functionIsNoHeap());
    result->setFunctionAsync(source->functionIsAsync());
    result->setFunctionNullable(source->functionIsNullable());
    result->setFunctionLifetimeName(source->functionLifetimeName());
    result->setFunctionReturnType(cloneType(
        arena, source->functionReturnType(), substitutions));
    result->setFunctionErrorType(cloneType(
        arena, source->functionErrorType(), substitutions));
    result->setPackExpansion(source->isPackExpansion());
    if (source->originProjectOrdinal().has_value())
        result->setOriginProjectOrdinal(*source->originProjectOrdinal());
    if (source->originNamespace().has_value())
        result->setOriginNamespace(*source->originNamespace());
    for (std::size_t index = 0; index < source->typeArguments().size(); ++index)
    {
        TypeSyntax* argument = cloneType(arena, source->typeArguments()[index], substitutions);
        if (source->isTuple())
        {
            const std::string name = index < source->tupleElementNames().size()
                ? source->tupleElementNames()[index] : std::string{};
            result->addTupleElement(argument, name);
        }
        else result->addTypeArgument(argument);
    }
    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind, modifier.arrayCount, modifier.range, modifier.arrayRank});
    return result;
}

AstNode* cloneNode(
    ast::AstArena& arena,
    const AstNode* source,
    const std::unordered_map<std::string, const TypeSyntax*>& substitutions = {})
{
    if (source == nullptr) return nullptr;
    AstNode* result = arena.make<AstNode>(source->kind(), source->range());
    result->setText(source->text());
    result->setAuxiliaryText(source->auxiliaryText());
    result->setArgumentName(source->argumentName());
    result->setTokenKind(source->tokenKind());
    result->setFlag(source->flag());
    result->setReference(source->isReference());
    result->setVariadic(source->isVariadic());
    result->setUnsafe(source->isUnsafe());
    result->setFloatingPointMode(source->floatingPointMode());
    result->setFfiUnwindPolicy(source->ffiUnwindPolicy());
    result->setIntegerValue(source->integerValue());
    result->setFloatingValue(source->floatingValue());
    for (TokenKind modifier : source->modifiers()) result->addModifier(modifier);
    result->setTypeSyntax(cloneType(arena, source->typeSyntax(), substitutions));
    for (const TypeSyntax* syntax : source->typeSyntaxes())
        result->addTypeSyntax(cloneType(arena, syntax, substitutions));
    for (const ast::GenericParameterSyntax& parameter : source->genericParameterSyntaxes())
        result->addGenericParameter(parameter);
    for (const ast::GenericConstraintClause& clause : source->genericConstraints())
        result->addGenericConstraint(clause);
    result->setFirst(cloneNode(arena, source->first(), substitutions));
    result->setSecond(cloneNode(arena, source->second(), substitutions));
    result->setThird(cloneNode(arena, source->third(), substitutions));
    for (const AstNode* child : source->children())
        result->addChild(cloneNode(arena, child, substitutions));
    return result;
}

AstNode* identifier(ast::AstArena& arena, SourceRange range, std::string name)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::IdentifierExpression, range);
    node->setText(std::move(name));
    return node;
}

AstNode* member(ast::AstArena& arena, AstNode* receiver, std::string name)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::MemberExpression, receiver->range());
    node->setFirst(receiver);
    node->setText(std::move(name));
    return node;
}

AstNode* call(ast::AstArena& arena, AstNode* callee, std::vector<AstNode*> arguments)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::CallExpression, callee->range());
    node->setFirst(callee);
    for (AstNode* argument : arguments) node->addChild(argument);
    return node;
}

AstNode* returnStatement(ast::AstArena& arena, SourceRange range, AstNode* value)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::ReturnStatement, range);
    node->setFirst(value);
    return node;
}

bool isCallable(AstNodeKind kind)
{
    return kind == AstNodeKind::MethodDeclaration ||
        kind == AstNodeKind::FunctionDeclaration ||
        kind == AstNodeKind::ConstructorDeclaration ||
        kind == AstNodeKind::DestructorDeclaration ||
        kind == AstNodeKind::LambdaExpression;
}

std::string calleeName(const AstNode* expression)
{
    if (expression == nullptr || expression->kind() != AstNodeKind::CallExpression ||
        expression->first() == nullptr)
        return {};
    const AstNode* callee = expression->first();
    if (callee->kind() == AstNodeKind::IdentifierExpression ||
        callee->kind() == AstNodeKind::MemberExpression)
        return callee->text();
    return {};
}

void collectReturns(AstNode* node, AstNode* owner, std::vector<AstNode*>& returns)
{
    if (node == nullptr) return;
    if (node != owner && isCallable(node->kind())) return;
    if (node->kind() == AstNodeKind::ReturnStatement)
    {
        returns.push_back(node);
        return;
    }
    collectReturns(node->first(), owner, returns);
    collectReturns(node->second(), owner, returns);
    collectReturns(node->third(), owner, returns);
    for (AstNode* child : node->children()) collectReturns(child, owner, returns);
}

void collectValueTypes(AstNode* node, std::unordered_map<std::string, const TypeSyntax*>& values)
{
    if (node == nullptr) return;
    if ((node->kind() == AstNodeKind::ParameterDeclaration ||
         node->kind() == AstNodeKind::VariableDeclarationStatement ||
         node->kind() == AstNodeKind::FieldDeclaration) &&
        node->typeSyntax() != nullptr &&
        node->typeSyntax()->baseTokenKind() != TokenKind::KwVar)
        values.emplace(node->text(), node->typeSyntax());
    collectValueTypes(node->first(), values);
    collectValueTypes(node->second(), values);
    collectValueTypes(node->third(), values);
    for (AstNode* child : node->children()) collectValueTypes(child, values);
}

const TypeSyntax* inferExpressionType(
    const AstNode* expression,
    const std::unordered_map<std::string, const TypeSyntax*>& values,
    const std::unordered_map<std::string, const TypeSyntax*>& callableReturns)
{
    if (expression == nullptr) return nullptr;
    switch (expression->kind())
    {
        case AstNodeKind::ObjectCreationExpression:
        case AstNodeKind::CastExpression:
        case AstNodeKind::SafeCastExpression:
        case AstNodeKind::DefaultValueExpression:
            return expression->typeSyntax();
        case AstNodeKind::IdentifierExpression:
        {
            const auto found = values.find(expression->text());
            return found == values.end() ? nullptr : found->second;
        }
        case AstNodeKind::MoveExpression:
        case AstNodeKind::AssignmentExpression:
            return inferExpressionType(
                expression->kind() == AstNodeKind::MoveExpression
                    ? expression->first() : expression->second(), values, callableReturns);
        case AstNodeKind::ConditionalExpression:
        {
            const TypeSyntax* left = inferExpressionType(
                expression->second(), values, callableReturns);
            const TypeSyntax* right = inferExpressionType(
                expression->third(), values, callableReturns);
            return left != nullptr && right != nullptr && typeSpelling(left) == typeSpelling(right)
                ? left : nullptr;
        }
        case AstNodeKind::CallExpression:
        {
            const auto found = callableReturns.find(calleeName(expression));
            return found == callableReturns.end() ? nullptr : found->second;
        }
        default: return nullptr;
    }
}

TypeSyntax* makeNamedType(
    ast::AstArena& arena,
    SourceRange range,
    std::string name,
    const AstNode* genericSource = nullptr)
{
    TypeSyntax* type = arena.make<TypeSyntax>(range, TokenKind::Identifier, std::move(name));
    if (genericSource != nullptr)
        for (const std::string& parameter : genericSource->genericParameters())
            type->addTypeArgument(arena.make<TypeSyntax>(
                range, TokenKind::Identifier, parameter));
    return type;
}

AstNode* makeFactory(
    ast::AstArena& arena,
    const LocatedDeclaration& declaration,
    std::string_view wrapperName,
    const TypeSyntax* concreteType)
{
    const SourceRange range = declaration.syntax->range();
    AstNode* factory = arena.make<AstNode>(AstNodeKind::MethodDeclaration, range);
    factory->setText("__nsc_from_representation");
    factory->setAuxiliaryText("opaque-result:from");
    factory->setTokenKind(TokenKind::KwInternal);
    factory->addModifier(TokenKind::KwInternal);
    factory->addModifier(TokenKind::KwStatic);
    factory->setTypeSyntax(makeNamedType(
        arena, range, std::string(wrapperName), declaration.syntax));

    AstNode* parameter = arena.make<AstNode>(AstNodeKind::ParameterDeclaration, range);
    parameter->setText("value");
    parameter->setTypeSyntax(cloneType(arena, concreteType));
    factory->addChild(parameter);

    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, range);
    AstNode* defaultValue = arena.make<AstNode>(AstNodeKind::DefaultValueExpression, range);
    defaultValue->setTypeSyntax(makeNamedType(
        arena, range, std::string(wrapperName), declaration.syntax));
    AstNode* local = arena.make<AstNode>(AstNodeKind::VariableDeclarationStatement, range);
    local->setText("__nsc_opaque_result");
    local->setTypeSyntax(makeNamedType(
        arena, range, std::string(wrapperName), declaration.syntax));
    local->setFirst(defaultValue);
    body->addChild(local);

    AstNode* assignment = arena.make<AstNode>(AstNodeKind::AssignmentExpression, range);
    assignment->setTokenKind(TokenKind::Equal);
    assignment->setFirst(member(
        arena, identifier(arena, range, "__nsc_opaque_result"), "__nsc_opaque_value"));
    assignment->setSecond(identifier(arena, range, "value"));
    AstNode* statement = arena.make<AstNode>(AstNodeKind::ExpressionStatement, range);
    statement->setFirst(assignment);
    body->addChild(statement);
    AstNode* result = identifier(arena, range, "__nsc_opaque_result");
    if (declaration.syntax->auxiliaryText().find("copy") == std::string::npos)
    {
        AstNode* movedResult = arena.make<AstNode>(AstNodeKind::MoveExpression, range);
        movedResult->setFirst(result);
        result = movedResult;
    }
    body->addChild(returnStatement(arena, range, result));
    factory->setFirst(body);
    return factory;
}

AstNode* makeForwarder(
    ast::AstArena& arena,
    const AstNode* requirement,
    const TypeSyntax* concreteType,
    const std::unordered_map<std::string, const TypeSyntax*>& substitutions)
{
    if (requirement == nullptr || requirement->kind() != AstNodeKind::MethodDeclaration ||
        requirement->first() != nullptr)
        return nullptr;

    const SourceRange range = requirement->range();
    AstNode* method = arena.make<AstNode>(AstNodeKind::MethodDeclaration, range);
    method->setText(requirement->text());
    method->setAuxiliaryText("opaque-result:forward:" + requirement->text());
    method->setTokenKind(TokenKind::KwPublic);
    method->addModifier(TokenKind::KwPublic);
    const bool isStatic = requirement->hasModifier(TokenKind::KwStatic);
    if (isStatic) method->addModifier(TokenKind::KwStatic);
    method->setTypeSyntax(cloneType(arena, requirement->typeSyntax(), substitutions));
    for (const ast::GenericParameterSyntax& parameter : requirement->genericParameterSyntaxes())
        method->addGenericParameter(parameter);
    for (const ast::GenericConstraintClause& clause : requirement->genericConstraints())
        method->addGenericConstraint(clause);

    std::vector<AstNode*> arguments;
    for (const AstNode* child : requirement->children())
    {
        if (child == nullptr || child->kind() != AstNodeKind::ParameterDeclaration) continue;
        AstNode* parameter = arena.make<AstNode>(AstNodeKind::ParameterDeclaration, child->range());
        parameter->setText(child->text());
        parameter->setAuxiliaryText(child->auxiliaryText());
        parameter->setTypeSyntax(cloneType(arena, child->typeSyntax(), substitutions));
        for (TokenKind modifier : child->modifiers()) parameter->addModifier(modifier);
        method->addChild(parameter);
        arguments.push_back(identifier(arena, child->range(), child->text()));
    }

    AstNode* receiver = identifier(
        arena, range, isStatic && concreteType != nullptr
            ? concreteType->baseName() : "__nsc_opaque_value");
    if (isStatic && concreteType != nullptr && !concreteType->typeArguments().empty())
        receiver->setTypeSyntax(cloneType(arena, concreteType));
    AstNode* invocation = call(
        arena, member(arena, receiver, requirement->text()), std::move(arguments));
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, range);
    if (method->typeSyntax() != nullptr && method->typeSyntax()->baseTokenKind() == TokenKind::KwVoid)
    {
        AstNode* statement = arena.make<AstNode>(AstNodeKind::ExpressionStatement, range);
        statement->setFirst(invocation);
        body->addChild(statement);
    }
    else body->addChild(returnStatement(arena, range, invocation));
    method->setFirst(body);
    return method;
}

bool isSelfRecursiveReturn(const AstNode* expression, const AstNode* declaration)
{
    return declaration != nullptr && calleeName(expression) == declaration->text();
}

// Reports opaque-result syntax outside a direct callable return while preserving traversal order.
void validateOpaqueResultPlacement(
    AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics,
    bool& ok)
{
    const bool callableReturn =
        (node->kind() == AstNodeKind::MethodDeclaration ||
         node->kind() == AstNodeKind::FunctionDeclaration) &&
        node->typeSyntax() != nullptr && node->typeSyntax()->isOpaqueResult();
    if (node->typeSyntax() != nullptr && containsOpaqueResult(node->typeSyntax()) &&
        (!callableReturn || !node->typeSyntax()->isOpaqueResult() ||
         std::ranges::any_of(node->typeSyntax()->typeArguments(), containsOpaqueResult)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            node->typeSyntax()->range(),
            "'some Trait' is permitted only as the direct return type of a function or method");
        ok = false;
    }
    for (const TypeSyntax* syntax : node->typeSyntaxes())
    {
        if (!containsOpaqueResult(syntax)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "'some Trait' is permitted only as the direct return type of a function or method");
        ok = false;
    }
}

// Records declarations and callable returns in the same depth-first order used by the original closure.
void collectOpaqueResultDeclarations(
    ProjectSyntaxUnit& unit,
    AstNode* node,
    AstNode* parent,
    const std::string& parentNamespace,
    const std::string& parentOwner,
    std::vector<LocatedDeclaration>& opaqueDeclarations,
    std::unordered_map<std::string, LocatedDeclaration>& traits,
    std::unordered_map<std::string, LocatedDeclaration>& concreteDeclarations,
    std::vector<LocatedDeclaration>& traitImplementations,
    std::unordered_map<std::string, const TypeSyntax*>& callableReturns,
    diagnostics::DiagnosticEngine& diagnostics,
    bool& ok)
{
    if (node == nullptr) return;
    std::string currentNamespace = parentNamespace;
    if (node->kind() == AstNodeKind::NamespaceDeclaration)
        currentNamespace = qualify(parentNamespace, node->text());
    std::string currentOwner = parentOwner;
    if (node->kind() == AstNodeKind::ClassDeclaration ||
        node->kind() == AstNodeKind::StructDeclaration ||
        node->kind() == AstNodeKind::InterfaceDeclaration)
        currentOwner = qualify(parentOwner, node->text());

    if (node->kind() == AstNodeKind::InterfaceDeclaration &&
        (node->auxiliaryText() == "ns3-trait" ||
         node->auxiliaryText().find("nsc-meta:ns3-trait") != std::string::npos))
    {
        const LocatedDeclaration located{&unit, parent, node, currentNamespace, currentOwner};
        traits.emplace(qualify(currentNamespace, node->text()), located);
        traits.emplace(node->text(), located);
    }
    if (node->kind() == AstNodeKind::ClassDeclaration ||
        node->kind() == AstNodeKind::StructDeclaration)
    {
        const LocatedDeclaration located{&unit, parent, node, currentNamespace, currentOwner};
        if (node->auxiliaryText() == "ns3-trait-impl") traitImplementations.push_back(located);
        else
        {
            concreteDeclarations.emplace(node->text(), located);
            concreteDeclarations.emplace(currentOwner, located);
            concreteDeclarations.emplace(qualify(currentNamespace, currentOwner), located);
        }
    }

    validateOpaqueResultPlacement(node, diagnostics, ok);
    if ((node->kind() == AstNodeKind::MethodDeclaration ||
         node->kind() == AstNodeKind::FunctionDeclaration) &&
        node->typeSyntax() != nullptr)
    {
        if (node->typeSyntax()->isOpaqueResult())
            opaqueDeclarations.push_back(
                LocatedDeclaration{&unit, parent, node, currentNamespace, currentOwner});
        else
        {
            const auto [position, inserted] = callableReturns.emplace(node->text(), node->typeSyntax());
            if (!inserted && typeSpelling(position->second) != typeSpelling(node->typeSyntax()))
                position->second = nullptr;
        }
    }

    const std::string childOwner = currentOwner;
    for (AstNode* child : node->children())
        collectOpaqueResultDeclarations(
            unit, child, node, currentNamespace, childOwner, opaqueDeclarations, traits,
            concreteDeclarations, traitImplementations, callableReturns, diagnostics, ok);
}

// Rejects opaque declarations whose source shape cannot produce a declaration-scoped wrapper.
bool validateOpaqueDeclaration(
    const LocatedDeclaration& declaration,
    diagnostics::DiagnosticEngine& diagnostics)
{
    AstNode* method = declaration.syntax;
    TypeSyntax* sourceReturn = method->typeSyntax();
    if (declaration.parent == nullptr || method->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            method->range(),
            "an opaque result declaration requires a body that determines one concrete type");
        return false;
    }
    if (sourceReturn->isDynamicTrait() || sourceReturn->isTuple() ||
        !sourceReturn->modifiers().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            sourceReturn->range(),
            "'some' must name one trait directly; pointer, array, tuple and 'dyn' forms are invalid");
        return false;
    }
    return true;
}

// Resolves the trait using the original unqualified-then-qualified precedence.
const LocatedDeclaration* findOpaqueTrait(
    const LocatedDeclaration& declaration,
    const TypeSyntax* sourceReturn,
    const std::unordered_map<std::string, LocatedDeclaration>& traits)
{
    const std::string traitName = sourceReturn->opaqueResultTraitName();
    const auto trait = traits.find(traitName);
    const auto qualifiedTrait = traits.find(qualify(declaration.namespaceName, traitName));
    return trait != traits.end()
        ? &trait->second
        : (qualifiedTrait != traits.end() ? &qualifiedTrait->second : nullptr);
}

// Infers the single concrete return type and emits the original aggregate failure diagnostic.
const TypeSyntax* inferOpaqueConcreteType(
    const LocatedDeclaration& declaration,
    const std::unordered_map<std::string, const TypeSyntax*>& callableReturns,
    std::vector<AstNode*>& returns,
    diagnostics::DiagnosticEngine& diagnostics)
{
    AstNode* method = declaration.syntax;
    std::unordered_map<std::string, const TypeSyntax*> values;
    collectValueTypes(method, values);
    collectReturns(method->first(), method, returns);
    const TypeSyntax* concreteType = nullptr;
    bool mismatch = false;
    bool unknown = false;
    for (AstNode* statement : returns)
    {
        if (statement->first() == nullptr) continue;
        if (isSelfRecursiveReturn(statement->first(), method)) continue;
        const TypeSyntax* inferred = inferExpressionType(statement->first(), values, callableReturns);
        if (inferred == nullptr)
        {
            unknown = true;
            continue;
        }
        if (concreteType == nullptr) concreteType = inferred;
        else if (typeSpelling(concreteType) != typeSpelling(inferred)) mismatch = true;
    }
    if (!returns.empty() && concreteType != nullptr && !unknown && !mismatch) return concreteType;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        method->range(),
        mismatch
            ? "opaque result paths resolve to different concrete types"
            : "opaque result concrete type cannot be inferred from every non-recursive return path");
    return nullptr;
}

// Builds the exposed trait spelling and verifies the inferred representation against it.
TypeSyntax* validateOpaqueConformance(
    const LocatedDeclaration& declaration,
    TypeSyntax* sourceReturn,
    const TypeSyntax* concreteType,
    const std::unordered_map<std::string, LocatedDeclaration>& concreteDeclarations,
    const std::vector<LocatedDeclaration>& traitImplementations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    TypeSyntax* exposedTrait = cloneType(declaration.unit->arena(), sourceReturn);
    exposedTrait->setOpaqueResult(false);
    exposedTrait->setOpaqueResultTraitName({});
    std::unordered_set<const AstNode*> visitedConformance;
    if (typeConformsToTrait(
            concreteType, typeSpelling(exposedTrait), declaration.namespaceName,
            concreteDeclarations, traitImplementations, visitedConformance))
        return exposedTrait;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        declaration.syntax->range(),
        "opaque result concrete type '" + typeSpelling(concreteType) +
            "' does not implement trait '" + typeSpelling(exposedTrait) + "'");
    return nullptr;
}

// Produces the stable declaration identity without changing parameter order or spelling.
std::string opaqueDeclarationIdentity(
    const LocatedDeclaration& declaration,
    const TypeSyntax* exposedTrait)
{
    AstNode* method = declaration.syntax;
    std::string identity = qualify(declaration.namespaceName, declaration.ownerName);
    identity += "::" + method->text() + "(";
    for (const AstNode* child : method->children())
        if (child != nullptr && child->kind() == AstNodeKind::ParameterDeclaration)
            identity += typeSpelling(child->typeSyntax()) + ";";
    identity += ")->some " + typeSpelling(exposedTrait);
    return identity;
}

// Creates the synthetic value wrapper, representation field and factory in their original order.
AstNode* makeOpaqueWrapper(
    ast::AstArena& arena,
    const LocatedDeclaration& declaration,
    const std::string& wrapperName,
    const std::string& identity,
    const std::string& traitName,
    TypeSyntax* exposedTrait,
    const TypeSyntax* concreteType)
{
    AstNode* method = declaration.syntax;
    AstNode* wrapper = arena.make<AstNode>(AstNodeKind::StructDeclaration, method->range());
    wrapper->setText(wrapperName);
    // The compiler-owned wrapper is a value just like a user-declared struct.
    // Keeping the marker on the synthetic declaration makes its generated factory return a
    // real move-only local instead of asking the ownership checker to move a copyable value.
    wrapper->setAuxiliaryText(
        "opaque-result|identity=" + identity + "|trait=" + traitName + "|move-only-value");
    const TokenKind accessibility = method->hasModifier(TokenKind::KwPublic)
        ? TokenKind::KwPublic : TokenKind::KwInternal;
    wrapper->setTokenKind(accessibility);
    wrapper->addModifier(accessibility);
    for (const ast::GenericParameterSyntax& parameter : method->genericParameterSyntaxes())
        wrapper->addGenericParameter(parameter);
    for (const ast::GenericConstraintClause& clause : method->genericConstraints())
        wrapper->addGenericConstraint(clause);
    TypeSyntax* wrapperTrait = exposedTrait;
    wrapper->setTypeSyntax(wrapperTrait);
    wrapper->addTypeSyntax(wrapperTrait);

    AstNode* field = arena.make<AstNode>(AstNodeKind::FieldDeclaration, method->range());
    field->setText("__nsc_opaque_value");
    field->setAuxiliaryText("opaque-result:representation");
    field->setTokenKind(TokenKind::KwPrivate);
    field->addModifier(TokenKind::KwPrivate);
    field->setTypeSyntax(cloneType(arena, concreteType));
    wrapper->addChild(field);
    wrapper->addChild(makeFactory(arena, declaration, wrapperName, concreteType));
    return wrapper;
}

// Finds the concrete associated item before falling back to a trait default.
const AstNode* findAssociatedDefinition(
    const LocatedDeclaration* concreteDeclaration,
    const std::vector<LocatedDeclaration>& traitImplementations,
    const TypeSyntax* concreteType,
    std::string_view name)
{
    if (concreteDeclaration != nullptr && concreteDeclaration->syntax != nullptr)
        for (const AstNode* child : concreteDeclaration->syntax->children())
            if (child != nullptr && child->text() == name &&
                (child->kind() == AstNodeKind::AssociatedTypeDeclaration ||
                 child->kind() == AstNodeKind::AssociatedConstantDeclaration))
                return child;
    for (const LocatedDeclaration& implementation : traitImplementations)
    {
        if (implementation.syntax == nullptr ||
            implementation.syntax->text() != concreteType->baseName())
            continue;
        for (const AstNode* child : implementation.syntax->children())
            if (child != nullptr && child->text() == name &&
                (child->kind() == AstNodeKind::AssociatedTypeDeclaration ||
                 child->kind() == AstNodeKind::AssociatedConstantDeclaration))
                return child;
    }
    return nullptr;
}

// Recursively appends inherited trait requirements using the original depth-first traversal.
void addOpaqueTraitSurface(
    ast::AstArena& arena,
    const LocatedDeclaration& currentTrait,
    const std::unordered_map<std::string, const TypeSyntax*>& substitutions,
    const std::unordered_map<std::string, LocatedDeclaration>& traits,
    const LocatedDeclaration* concreteDeclaration,
    const std::vector<LocatedDeclaration>& traitImplementations,
    const TypeSyntax* concreteType,
    AstNode* wrapper,
    std::unordered_set<std::string>& forwardedMethods,
    std::unordered_set<std::string>& forwardedAssociatedItems,
    std::unordered_set<const AstNode*>& visitedTraits)
{
    if (currentTrait.syntax == nullptr || !visitedTraits.insert(currentTrait.syntax).second) return;
    for (const TypeSyntax* base : currentTrait.syntax->typeSyntaxes())
    {
        if (base == nullptr) continue;
        std::string baseName = base->baseName();
        const auto directBase = traits.find(baseName);
        const auto qualifiedBase = traits.find(qualify(currentTrait.namespaceName, baseName));
        const LocatedDeclaration* baseTrait = directBase != traits.end()
            ? &directBase->second
            : (qualifiedBase != traits.end() ? &qualifiedBase->second : nullptr);
        if (baseTrait == nullptr) continue;
        std::unordered_map<std::string, const TypeSyntax*> baseSubstitutions;
        const auto& baseParameters = baseTrait->syntax->genericParameters();
        for (std::size_t index = 0;
             index < baseParameters.size() && index < base->typeArguments().size(); ++index)
        {
            const TypeSyntax* argument = base->typeArguments()[index];
            const auto replacement = argument == nullptr
                ? substitutions.end() : substitutions.find(argument->baseName());
            baseSubstitutions.emplace(
                baseParameters[index],
                replacement != substitutions.end() && argument->typeArguments().empty()
                    ? replacement->second : argument);
        }
        addOpaqueTraitSurface(
            arena, *baseTrait, baseSubstitutions, traits, concreteDeclaration,
            traitImplementations, concreteType, wrapper, forwardedMethods,
            forwardedAssociatedItems, visitedTraits);
    }
    for (const AstNode* requirement : currentTrait.syntax->children())
    {
        if (requirement == nullptr) continue;
        if (requirement->kind() == AstNodeKind::AssociatedTypeDeclaration ||
            requirement->kind() == AstNodeKind::AssociatedConstantDeclaration)
        {
            if (!forwardedAssociatedItems.insert(requirement->text()).second) continue;
            const AstNode* definition = findAssociatedDefinition(
                concreteDeclaration, traitImplementations, concreteType, requirement->text());
            const bool hasDefault =
                (requirement->kind() == AstNodeKind::AssociatedTypeDeclaration &&
                 requirement->typeSyntax() != nullptr) ||
                (requirement->kind() == AstNodeKind::AssociatedConstantDeclaration &&
                 requirement->first() != nullptr);
            if (definition != nullptr || hasDefault)
                wrapper->addChild(cloneNode(
                    arena, definition != nullptr ? definition : requirement, substitutions));
            continue;
        }
        if (requirement->kind() != AstNodeKind::MethodDeclaration) continue;
        std::string signature = requirement->text() + "(";
        for (const AstNode* parameter : requirement->children())
            if (parameter != nullptr && parameter->kind() == AstNodeKind::ParameterDeclaration)
                signature += typeSpelling(parameter->typeSyntax()) + ";";
        signature += ")";
        if (!forwardedMethods.insert(signature).second) continue;
        if (requirement->first() != nullptr)
            wrapper->addChild(cloneNode(arena, requirement, substitutions));
        else if (AstNode* forwarder = makeForwarder(arena, requirement, concreteType, substitutions))
            wrapper->addChild(forwarder);
    }
}

// Seeds trait substitutions and preserves the original visited and de-duplication scopes.
void populateOpaqueTraitSurface(
    ast::AstArena& arena,
    AstNode* wrapper,
    const LocatedDeclaration& traitDeclaration,
    const TypeSyntax* sourceReturn,
    const std::unordered_map<std::string, LocatedDeclaration>& traits,
    const LocatedDeclaration* concreteDeclaration,
    const std::vector<LocatedDeclaration>& traitImplementations,
    const TypeSyntax* concreteType)
{
    std::unordered_map<std::string, const TypeSyntax*> traitSubstitutions;
    const auto& traitParameters = traitDeclaration.syntax->genericParameters();
    for (std::size_t index = 0;
         index < traitParameters.size() && index < sourceReturn->typeArguments().size(); ++index)
        traitSubstitutions.emplace(traitParameters[index], sourceReturn->typeArguments()[index]);
    std::unordered_set<std::string> forwardedMethods;
    std::unordered_set<std::string> forwardedAssociatedItems;
    std::unordered_set<const AstNode*> visitedTraits;
    addOpaqueTraitSurface(
        arena, traitDeclaration, traitSubstitutions, traits, concreteDeclaration,
        traitImplementations, concreteType, wrapper, forwardedMethods,
        forwardedAssociatedItems, visitedTraits);
}

// Rewrites the callable return and each non-recursive return value, then publishes the wrapper.
void rewriteOpaqueDeclaration(
    ast::AstArena& arena,
    const LocatedDeclaration& declaration,
    TypeSyntax* sourceReturn,
    const std::string& wrapperName,
    const std::string& traitName,
    std::vector<AstNode*>& returns,
    std::unordered_map<std::string, const TypeSyntax*>& callableReturns,
    AstNode* wrapper)
{
    AstNode* method = declaration.syntax;
    TypeSyntax* rewrittenReturn = makeNamedType(
        arena, sourceReturn->range(), wrapperName, method);
    rewrittenReturn->setOwnershipQualifier(sourceReturn->ownershipQualifier());
    rewrittenReturn->setOpaqueResult();
    rewrittenReturn->setOpaqueResultTraitName(traitName);
    method->setTypeSyntax(rewrittenReturn);
    method->setAuxiliaryText(
        method->auxiliaryText() + (method->auxiliaryText().empty() ? "" : "|") +
        "opaque-result:" + wrapperName + ":" + traitName);

    const auto [callable, insertedCallable] = callableReturns.emplace(method->text(), rewrittenReturn);
    if (!insertedCallable && callable->second != nullptr &&
        typeSpelling(callable->second) != typeSpelling(rewrittenReturn))
        callable->second = nullptr;
    for (AstNode* statement : returns)
    {
        AstNode* value = statement->first();
        if (value == nullptr || isSelfRecursiveReturn(value, method)) continue;
        AstNode* typeExpression = identifier(arena, value->range(), wrapperName);
        if (!method->genericParameters().empty())
            typeExpression->setTypeSyntax(makeNamedType(
                arena, value->range(), wrapperName, method));
        AstNode* factory = member(arena, typeExpression, "__nsc_from_representation");
        statement->setFirst(call(arena, factory, {value}));
    }
    declaration.parent->addChild(wrapper);
}

// Processes one opaque declaration without changing the original validation and publication sequence.
bool processOpaqueDeclaration(
    const LocatedDeclaration& declaration,
    const std::unordered_map<std::string, LocatedDeclaration>& traits,
    const std::unordered_map<std::string, LocatedDeclaration>& concreteDeclarations,
    const std::vector<LocatedDeclaration>& traitImplementations,
    std::unordered_map<std::string, const TypeSyntax*>& callableReturns,
    std::unordered_set<std::string>& wrapperNames,
    diagnostics::DiagnosticEngine& diagnostics)
{
    AstNode* method = declaration.syntax;
    TypeSyntax* sourceReturn = method->typeSyntax();
    if (!validateOpaqueDeclaration(declaration, diagnostics)) return false;
    const std::string traitName = sourceReturn->opaqueResultTraitName();
    const LocatedDeclaration* traitDeclaration = findOpaqueTrait(declaration, sourceReturn, traits);
    if (traitDeclaration == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            sourceReturn->range(),
            "opaque result target '" + traitName + "' is not a visible trait");
        return false;
    }
    std::vector<AstNode*> returns;
    const TypeSyntax* concreteType = inferOpaqueConcreteType(
        declaration, callableReturns, returns, diagnostics);
    if (concreteType == nullptr) return false;
    TypeSyntax* exposedTrait = validateOpaqueConformance(
        declaration, sourceReturn, concreteType, concreteDeclarations,
        traitImplementations, diagnostics);
    if (exposedTrait == nullptr) return false;

    const std::string identity = opaqueDeclarationIdentity(declaration, exposedTrait);
    const std::string wrapperName = "__nsc_some_" + stableSuffix(identity);
    if (!wrapperNames.insert(wrapperName).second)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            method->range(),
            "duplicate opaque result declaration identity '" + identity + "'");
        return false;
    }
    ast::AstArena& arena = declaration.unit->arena();
    AstNode* wrapper = makeOpaqueWrapper(
        arena, declaration, wrapperName, identity, traitName, exposedTrait, concreteType);
    const LocatedDeclaration* concreteDeclaration = findDeclaration(
        concreteDeclarations, concreteType->baseName(), declaration.namespaceName);
    populateOpaqueTraitSurface(
        arena, wrapper, *traitDeclaration, sourceReturn, traits, concreteDeclaration,
        traitImplementations, concreteType);
    rewriteOpaqueDeclaration(
        arena, declaration, sourceReturn, wrapperName, traitName, returns, callableReturns, wrapper);
    return true;
}
}

bool ProjectOpaqueResultModel::build(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<LocatedDeclaration> opaqueDeclarations;
    std::unordered_map<std::string, LocatedDeclaration> traits;
    std::unordered_map<std::string, LocatedDeclaration> concreteDeclarations;
    std::vector<LocatedDeclaration> traitImplementations;
    std::unordered_map<std::string, const TypeSyntax*> callableReturns;
    bool ok = true;

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr && unit->syntaxTree() != nullptr)
            collectOpaqueResultDeclarations(
                *unit, unit->syntaxTree(), nullptr, "", "", opaqueDeclarations, traits,
                concreteDeclarations, traitImplementations, callableReturns, diagnostics, ok);

    std::unordered_set<std::string> wrapperNames;
    for (const LocatedDeclaration& declaration : opaqueDeclarations)
        if (!processOpaqueDeclaration(
                declaration, traits, concreteDeclarations, traitImplementations,
                callableReturns, wrapperNames, diagnostics))
            ok = false;

    return ok && !diagnostics.hasErrors();
}
}
