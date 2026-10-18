#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Language/GenericSemantics.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}

bool Parser::supportsConstGenericParameterType(const ast::TypeSyntax* type) noexcept
{
    if (type == nullptr || !type->modifiers().empty()) return false;
    const TokenKind kind = type->baseTokenKind();
    const bool scalarSupported = type->typeArguments().empty() &&
        (kind == TokenKind::KwBool || kind == TokenKind::KwByte ||
         kind == TokenKind::KwSByte || kind == TokenKind::KwShort ||
         kind == TokenKind::KwUShort || kind == TokenKind::KwInt ||
         kind == TokenKind::KwUInt || kind == TokenKind::KwLong ||
         kind == TokenKind::KwULong || kind == TokenKind::KwNInt ||
         kind == TokenKind::KwNUInt || kind == TokenKind::KwChar ||
         kind == TokenKind::Identifier);
    return scalarSupported || type->isTuple();
}

bool Parser::parseConstGenericParameterType(
    ast::GenericParameterSyntax& syntax,
    bool pack)
{
    syntax.constantType = parseType();
    if (syntax.constantType == nullptr) return false;
    if (!supportsConstGenericParameterType(syntax.constantType))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax.constantType->range(),
            pack
                ? "const generic packs require bool, char, an integral builtin type, "
                  "an enum, or an approved structural tuple type"
                : "const generic parameters require bool, char, an integral builtin type, "
                  "an enum, or an approved structural tuple type");
    }
    return true;
}

bool Parser::parseGenericParameterKind(
    ast::GenericParameterSyntax& syntax,
    bool& lifetimeParameter)
{
    lifetimeParameter = atContextualKeyword("lifetime");
    if (lifetimeParameter)
    {
        consume();
        return true;
    }
    if (consumeIf(TokenKind::KwConst))
    {
        syntax.kind = ast::GenericParameterSyntaxKind::Constant;
        return parseConstGenericParameterType(syntax, false);
    }
    if (!atContextualKeyword("pack")) return true;
    consume();
    if (consumeIf(TokenKind::KwConst))
    {
        syntax.kind = ast::GenericParameterSyntaxKind::ConstantPack;
        return parseConstGenericParameterType(syntax, true);
    }
    syntax.kind = ast::GenericParameterSyntaxKind::TypePack;
    return true;
}

void Parser::finishGenericParameter(
    ast::AstNode* declaration,
    ast::GenericParameterSyntax syntax,
    bool lifetimeParameter,
    std::size_t parameterBegin,
    std::unordered_set<std::string>& names)
{
    const lexer::Token& parameter = consume();
    const std::string name(parameter.lexeme());
    if (!names.insert(name).second)
    {
        const std::string parameterKind = lifetimeParameter
            ? "lifetime parameter"
            : syntax.kind == ast::GenericParameterSyntaxKind::Constant ||
                syntax.kind == ast::GenericParameterSyntaxKind::ConstantPack
            ? "const generic parameter" : "generic type parameter";
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range(),
            "duplicate " + parameterKind + " '" + name + "'");
    }
    syntax.name = name;
    syntax.range = tokenRangeFrom(parameterBegin);
    if (lifetimeParameter)
        declaration->addLifetimeParameter(ast::LifetimeParameterSyntax{name, syntax.range});
    else
        declaration->addGenericParameter(std::move(syntax));
}

bool Parser::parseGenericParameter(
    ast::AstNode* declaration,
    std::unordered_set<std::string>& names)
{
    ast::GenericParameterSyntax syntax;
    const std::size_t parameterBegin = index_;
    bool lifetimeParameter = false;
    if (!parseGenericParameterKind(syntax, lifetimeParameter)) return false;
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "generic parameter name");
        return false;
    }
    finishGenericParameter(
        declaration, std::move(syntax), lifetimeParameter, parameterBegin, names);
    return true;
}

void Parser::validateGenericPackPlacement(const ast::AstNode* declaration)
{
    const auto& parameters = declaration->genericParameterSyntaxes();
    for (std::size_t index = 0; index + 1U < parameters.size(); ++index)
    {
        if (parameters[index].kind != ast::GenericParameterSyntaxKind::TypePack &&
            parameters[index].kind != ast::GenericParameterSyntaxKind::ConstantPack)
            continue;
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            parameters[index].range,
            "a generic pack must be the final generic parameter");
    }
}

bool Parser::parseGenericParameterList(ast::AstNode* declaration)
{
    if (declaration == nullptr || !consumeIf(TokenKind::Less)) return true;
    std::unordered_set<std::string> names;
    while (true)
    {
        if (!parseGenericParameter(declaration, names)) return false;
        if (!consumeIf(TokenKind::Comma)) break;
    }
    validateGenericPackPlacement(declaration);
    return expect(TokenKind::Greater, "'>' after generic type parameter list");
}

bool Parser::beginGenericConstraintClause(
    ast::AstNode* declaration,
    std::unordered_set<std::string>& constrained,
    GenericConstraintParseState& state,
    bool& ok)
{
    state.clauseBegin = index_;
    consume();
    if (!at(TokenKind::Identifier))
        return expect(TokenKind::Identifier, "generic type parameter after 'where'");
    const lexer::Token& parameter = consume();
    state.parameterRange = parameter.range();
    state.rootParameter = std::string(parameter.lexeme());
    state.clause.parameterName = state.rootParameter;
    state.clause.parameterRange = parameter.range();
    while (consumeIf(TokenKind::Dot))
    {
        if (!at(TokenKind::Identifier))
            return expect(TokenKind::Identifier, "associated item name after '.'");
        const lexer::Token& item = consume();
        state.clause.parameterName += '.';
        state.clause.parameterName += item.lexeme();
        state.clause.parameterRange = join(state.clause.parameterRange, item.range());
    }
    state.associatedEquality = state.clause.parameterName != state.rootParameter;
    if (state.associatedEquality)
        state.clause.associatedProjection = arena_.make<ast::TypeSyntax>(
            state.clause.parameterRange, TokenKind::Identifier, state.clause.parameterName);
    const auto& declared = declaration->genericParameters();
    const bool knownGeneric = std::ranges::find(declared, state.rootParameter) != declared.end();
    state.knownLifetime = std::ranges::any_of(
        declaration->lifetimeParameters(), [&](const ast::LifetimeParameterSyntax& item)
        {
            return item.name == state.rootParameter;
        });
    state.known = knownGeneric || state.knownLifetime;
    const auto syntax = std::ranges::find_if(
        declaration->genericParameterSyntaxes(), [&](const ast::GenericParameterSyntax& item)
        {
            return item.name == state.rootParameter;
        });
    if (syntax != declaration->genericParameterSyntaxes().end())
        state.parameterSyntax = &*syntax;
    if (!state.known)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::UnknownIdentifier,
            diagnostics::DiagnosticSeverity::Error,
            state.parameterRange,
            "'" + state.rootParameter +
                "' is not a generic or lifetime parameter of this declaration");
        ok = false;
    }
    else if (!constrained.insert(state.clause.parameterName).second)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            state.parameterRange,
            "duplicate 'where' clause for generic type parameter '" +
                state.clause.parameterName + "'");
        ok = false;
    }
    return true;
}

bool Parser::parseLifetimeConstraintItem(
    ast::AstNode* declaration,
    const GenericConstraintParseState& state,
    std::unordered_set<std::string>& written,
    bool& ok)
{
    if (!at(TokenKind::Identifier) && !at(TokenKind::KwStatic))
        return expect(TokenKind::Identifier, "lifetime name after ':'");
    const lexer::Token& shorter = consume();
    const std::string shorterName(shorter.lexeme());
    const bool isStatic = shorterName == "static";
    const bool declaredLifetime = std::ranges::any_of(
        declaration->lifetimeParameters(), [&](const ast::LifetimeParameterSyntax& item)
        {
            return item.name == shorterName;
        });
    if (!isStatic && !declaredLifetime)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::UnknownIdentifier,
            diagnostics::DiagnosticSeverity::Error,
            shorter.range(),
            "'" + shorterName + "' is not a lifetime parameter of this declaration");
        ok = false;
    }
    if (shorterName == state.rootParameter)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            shorter.range(),
            "lifetime outlives constraint for '" + state.rootParameter + "' is tautological");
        ok = false;
    }
    if (!written.insert(shorterName).second)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            shorter.range(),
            "duplicate outlives constraint '" + state.rootParameter + ":" + shorterName + "'");
        ok = false;
    }
    if ((isStatic || declaredLifetime) && shorterName != state.rootParameter)
        declaration->addLifetimeOutlivesConstraint(ast::LifetimeOutlivesConstraint{
            state.rootParameter, shorterName, join(state.parameterRange, shorter.range())});
    return true;
}

bool Parser::parseLifetimeConstraintClause(
    ast::AstNode* declaration,
    const GenericConstraintParseState& state,
    bool& ok)
{
    if (state.associatedEquality)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.clause.parameterRange,
            "a lifetime parameter cannot name an associated item");
        synchronizeGlobal();
        return false;
    }
    if (!expect(TokenKind::Colon, "':' after the constrained lifetime parameter")) return false;
    std::unordered_set<std::string> written;
    while (true)
    {
        if (!parseLifetimeConstraintItem(declaration, state, written, ok)) return false;
        if (!consumeIf(TokenKind::Comma)) break;
    }
    return true;
}

ast::TypeSyntax* Parser::parseGenericEqualityTarget(
    const GenericConstraintParseState& state,
    bool parameterIsConstant)
{
    if (at(TokenKind::IntegerLiteral))
    {
        const lexer::Token& literal = consume();
        const auto* value = std::get_if<std::uint64_t>(&literal.value());
        const std::uint64_t normalized = value == nullptr ? 0 : *value;
        const std::string typeKey = parameterIsConstant &&
            state.parameterSyntax->constantType != nullptr
            ? "builtin:" + state.parameterSyntax->constantType->baseName()
            : std::string(frontend::builtins::integerLiteralTypeKey(
                  literal.lexeme(), normalized, false));
        return arena_.make<ast::TypeSyntax>(
            literal.range(), TokenKind::Invalid,
            language::constGenericArgumentMarker(typeKey, std::to_string(normalized)));
    }
    if (at(TokenKind::KwTrue) || at(TokenKind::KwFalse))
    {
        const lexer::Token& literal = consume();
        return arena_.make<ast::TypeSyntax>(
            literal.range(), TokenKind::Invalid,
            language::constGenericArgumentMarker(
                "builtin:bool", literal.kind() == TokenKind::KwTrue ? "true" : "false"));
    }
    return parseType();
}

void Parser::validateGenericEqualityTarget(
    ast::AstNode* declaration,
    const GenericConstraintParseState& state,
    bool parameterIsConstant,
    bool& ok)
{
    std::string equalityConstantType;
    std::string equalityConstantValue;
    const bool equalityIsConstant = language::parseConstGenericArgumentMarker(
        state.clause.equalityConstraint->baseName(),
        equalityConstantType,
        equalityConstantValue);
    if (parameterIsConstant != equalityIsConstant)
    {
        const auto otherParameter = std::ranges::find_if(
            declaration->genericParameterSyntaxes(), [&](const ast::GenericParameterSyntax& item)
            {
                return item.name == state.clause.equalityConstraint->baseName();
            });
        const bool otherIsMatchingParameter =
            otherParameter != declaration->genericParameterSyntaxes().end() &&
            ((parameterIsConstant &&
              otherParameter->kind == ast::GenericParameterSyntaxKind::Constant) ||
             (!parameterIsConstant &&
              otherParameter->kind == ast::GenericParameterSyntaxKind::Type));
        if (!otherIsMatchingParameter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                state.clause.equalityConstraint->range(),
                parameterIsConstant
                    ? "a const generic equality requires another const parameter or a compile-time value"
                    : "a type generic equality requires another type parameter or a type");
            ok = false;
        }
    }
    if (state.clause.equalityConstraint->baseName() == state.clause.parameterName)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            state.clause.equalityConstraint->range(),
            "generic equality constraint for '" + state.clause.parameterName +
                "' is tautological");
        ok = false;
    }
}

Parser::GenericConstraintBranchResult Parser::parseGenericEqualityConstraint(
    ast::AstNode* declaration,
    GenericConstraintParseState& state,
    bool& ok)
{
    if (!consumeIf(TokenKind::EqualEqual)) return GenericConstraintBranchResult::NotMatched;
    const bool parameterIsConstant = state.parameterSyntax != nullptr &&
        state.parameterSyntax->kind == ast::GenericParameterSyntaxKind::Constant;
    if (state.parameterSyntax != nullptr &&
        (state.parameterSyntax->kind == ast::GenericParameterSyntaxKind::TypePack ||
         state.parameterSyntax->kind == ast::GenericParameterSyntaxKind::ConstantPack))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.parameterRange,
            "a generic pack cannot be the left side of an equality constraint");
        ok = false;
    }
    state.clause.equalityConstraint = parseGenericEqualityTarget(state, parameterIsConstant);
    if (state.clause.equalityConstraint == nullptr)
        return GenericConstraintBranchResult::Failed;
    validateGenericEqualityTarget(declaration, state, parameterIsConstant, ok);
    state.clause.range = tokenRangeFrom(state.clauseBegin);
    if (state.known) declaration->addGenericConstraint(std::move(state.clause));
    return GenericConstraintBranchResult::Parsed;
}

std::optional<Parser::GenericCapabilityKind>
Parser::currentGenericCapability() const noexcept
{
    if (at(TokenKind::KwClass)) return GenericCapabilityKind::Class;
    if (at(TokenKind::KwStruct)) return GenericCapabilityKind::Struct;
    if (at(TokenKind::KwNew)) return GenericCapabilityKind::DefaultConstructor;
    if (atContextualKeyword("copy")) return GenericCapabilityKind::Copy;
    if (at(TokenKind::KwMove) || atContextualKeyword("move"))
        return GenericCapabilityKind::Move;
    if (atContextualKeyword("lifetime")) return GenericCapabilityKind::Lifetime;
    if (atContextualKeyword("unmanaged")) return GenericCapabilityKind::Unmanaged;
    if (atContextualKeyword("notnull")) return GenericCapabilityKind::NotNull;
    if (atContextualKeyword("ref") && peek().kind() == TokenKind::Minus &&
        peek(2).kind() == TokenKind::Identifier && peek(2).lexeme() == "like")
        return GenericCapabilityKind::RefLike;
    return std::nullopt;
}

bool Parser::parseGenericCapability(
    ast::GenericConstraintClause& clause,
    GenericCapabilityKind kind,
    bool firstConstraint,
    bool& ok)
{
    const lexer::Token& keyword = consume();
    if (kind == GenericCapabilityKind::RefLike)
    {
        consume();
        consume();
    }
    const bool conflictsWithEarlierPrimary =
        (kind == GenericCapabilityKind::Class &&
         (clause.requiresValueType || clause.requiresUnmanaged)) ||
        (kind == GenericCapabilityKind::Struct && clause.requiresClass);
    if ((kind == GenericCapabilityKind::Class || kind == GenericCapabilityKind::Struct) &&
        !firstConstraint && !conflictsWithEarlierPrimary)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            keyword.range(),
            kind == GenericCapabilityKind::Class
                ? "'class' must be the first constraint of its 'where' clause"
                : "primary generic constraint '" + std::string(keyword.lexeme()) +
                    "' must be first in its 'where' clause");
        ok = false;
    }
    bool* flag = nullptr;
    std::string_view spelling;
    switch (kind)
    {
        case GenericCapabilityKind::Class: flag = &clause.requiresClass; spelling = "class"; break;
        case GenericCapabilityKind::Struct: flag = &clause.requiresValueType; spelling = "struct"; break;
        case GenericCapabilityKind::DefaultConstructor:
            flag = &clause.requiresDefaultConstructor;
            spelling = "new()";
            if (!expect(TokenKind::LeftParen, "'(' after new in a generic constraint") ||
                !expect(TokenKind::RightParen, "')' after new( in a generic constraint"))
                return false;
            break;
        case GenericCapabilityKind::Copy: flag = &clause.requiresCopy; spelling = "copy"; break;
        case GenericCapabilityKind::Move: flag = &clause.requiresMove; spelling = "move"; break;
        case GenericCapabilityKind::Lifetime:
            flag = &clause.requiresTrackedLifetime; spelling = "lifetime"; break;
        case GenericCapabilityKind::Unmanaged:
            flag = &clause.requiresUnmanaged; spelling = "unmanaged"; break;
        case GenericCapabilityKind::NotNull: flag = &clause.requiresNotNull; spelling = "notnull"; break;
        case GenericCapabilityKind::RefLike: flag = &clause.requiresRefLike; spelling = "ref-like"; break;
    }
    if (*flag)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            keyword.range(),
            "duplicate '" + std::string(spelling) + "' constraint for '" +
                clause.parameterName + "'");
        ok = false;
    }
    *flag = true;
    return true;
}

bool Parser::parseGenericNominalConstraint(
    const ast::AstNode* declaration,
    ast::GenericConstraintClause& clause,
    std::unordered_set<std::string>& written,
    bool& ok)
{
    ast::TypeSyntax* constraint = parseType();
    if (constraint == nullptr) return false;
    const auto& declared = declaration->genericParameters();
    if (std::ranges::find(declared, std::string(constraint->baseName())) != declared.end())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            constraint->range(),
            "'" + std::string(constraint->baseName()) +
                "' is a generic type parameter of this declaration and cannot be used "
                "as a constraint; use 'where " + clause.parameterName + " == " +
                std::string(constraint->baseName()) +
                "' for equality instead of a trait/base constraint");
        ok = false;
    }
    if (!written.insert(constraint->baseName()).second)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            constraint->range(),
            "duplicate constraint '" + constraint->baseName() + "' for '" +
                clause.parameterName + "'");
        ok = false;
    }
    clause.typeConstraints.push_back(constraint);
    return true;
}

bool Parser::parseGenericCapabilityConstraints(
    ast::AstNode* declaration,
    GenericConstraintParseState& state,
    bool& ok)
{
    if (state.associatedEquality)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            state.clause.parameterRange,
            "an associated type projection constraint requires '=='");
        synchronizeGlobal();
        return false;
    }
    if (state.parameterSyntax != nullptr &&
        state.parameterSyntax->kind != ast::GenericParameterSyntaxKind::Type)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.parameterRange,
            "a capability or trait 'where' clause applies only to an ordinary generic type parameter; '" +
                state.clause.parameterName + "' is a const generic or type pack");
        ok = false;
    }
    if (!expect(TokenKind::Colon, "':' or '==' after the constrained generic parameter"))
        return false;
    std::unordered_set<std::string> written;
    bool firstConstraint = true;
    while (true)
    {
        const std::optional<GenericCapabilityKind> capability = currentGenericCapability();
        if (capability.has_value())
        {
            if (!parseGenericCapability(state.clause, *capability, firstConstraint, ok)) return false;
        }
        else if (!parseGenericNominalConstraint(declaration, state.clause, written, ok))
            return false;
        firstConstraint = false;
        if (!consumeIf(TokenKind::Comma)) break;
    }
    state.clause.range = tokenRangeFrom(state.clauseBegin);
    if (state.known) declaration->addGenericConstraint(std::move(state.clause));
    return true;
}

bool Parser::parseGenericConstraintClauses(ast::AstNode* declaration)
{
    if (declaration == nullptr) return true;
    bool ok = true;
    std::unordered_set<std::string> constrained;
    while (atContextualKeyword("where"))
    {
        GenericConstraintParseState state;
        if (!beginGenericConstraintClause(declaration, constrained, state, ok)) return false;
        if (state.knownLifetime)
        {
            if (!parseLifetimeConstraintClause(declaration, state, ok)) return false;
            continue;
        }
        const GenericConstraintBranchResult equality =
            parseGenericEqualityConstraint(declaration, state, ok);
        if (equality == GenericConstraintBranchResult::Failed) return false;
        if (equality == GenericConstraintBranchResult::Parsed) continue;
        if (!parseGenericCapabilityConstraints(declaration, state, ok)) return false;
    }
    return ok;
}

bool Parser::looksLikeTupleTypeArgument() const noexcept
{
    if (!at(TokenKind::LeftParen)) return false;
    std::size_t afterType = index_;
    if (!scanTypeAt(index_, afterType) || afterType >= tokens_.size()) return false;
    const TokenKind following = tokens_[afterType].kind();
    return following == TokenKind::Comma || following == TokenKind::Greater ||
        following == TokenKind::GreaterGreater;
}

ast::TypeSyntax* Parser::parseConstExpressionTypeArgument()
{
    const SourceRange begin = current().range();
    ast::AstNode* expression = at(TokenKind::LeftParen)
        ? parsePrimaryExpression() : parseUnaryExpression();
    ast::TypeSyntax* argument = arena_.make<ast::TypeSyntax>(
        expression != nullptr ? expression->range() : begin,
        TokenKind::Invalid,
        "$nsc_const_expression$");
    argument->setConstExpression(expression);
    return argument;
}

ast::TypeSyntax* Parser::parseConstLiteralTypeArgument()
{
    if (at(TokenKind::IntegerLiteral))
    {
        const lexer::Token& literal = consume();
        const auto* value = std::get_if<std::uint64_t>(&literal.value());
        const std::uint64_t normalized = value == nullptr ? 0 : *value;
        const std::string typeKey(frontend::builtins::integerLiteralTypeKey(
            literal.lexeme(), normalized, false));
        return arena_.make<ast::TypeSyntax>(
            literal.range(), TokenKind::Invalid,
            language::constGenericArgumentMarker(typeKey, std::to_string(normalized)));
    }
    if (!at(TokenKind::KwTrue) && !at(TokenKind::KwFalse)) return nullptr;
    const lexer::Token& literal = consume();
    return arena_.make<ast::TypeSyntax>(
        literal.range(), TokenKind::Invalid,
        language::constGenericArgumentMarker(
            "builtin:bool", literal.kind() == TokenKind::KwTrue ? "true" : "false"));
}

bool Parser::parseTypeArgument(std::vector<ast::TypeSyntax*>& arguments)
{
    if (looksLikeTupleTypeArgument())
    {
        if (ast::TypeSyntax* argument = parseType(); argument != nullptr)
            arguments.push_back(argument);
        return true;
    }
    if (at(TokenKind::LeftParen) || at(TokenKind::Plus) || at(TokenKind::Minus) ||
        at(TokenKind::Tilde) || at(TokenKind::Bang))
    {
        arguments.push_back(parseConstExpressionTypeArgument());
        return true;
    }
    if (ast::TypeSyntax* literal = parseConstLiteralTypeArgument(); literal != nullptr)
    {
        arguments.push_back(literal);
        return true;
    }
    if (!canStartTypeAt(index_) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "generic type argument list requires a type");
        return false;
    }
    if (ast::TypeSyntax* argument = parseType(); argument != nullptr)
        arguments.push_back(argument);
    return true;
}

bool Parser::parseTypeArgumentList(std::vector<ast::TypeSyntax*>& arguments)
{
    if (!consumeIf(TokenKind::Less)) return false;
    if (consumeGenericGreater()) return true;
    while (true)
    {
        if (!parseTypeArgument(arguments)) return false;
        if (!consumeIf(TokenKind::Comma)) break;
    }
    if (consumeGenericGreater()) return true;
    expect(TokenKind::Greater, "'>' after generic type argument list");
    return false;
}

bool Parser::consumeGenericGreater()
{
    if (pendingGenericGreater_ != 0)
    {
        --pendingGenericGreater_;
        return true;
    }
    if (consumeIf(TokenKind::Greater)) return true;
    if (at(TokenKind::GreaterGreater))
    {
        consume();
        ++pendingGenericGreater_;
        return true;
    }
    if (at(TokenKind::GreaterGreaterEqual))
    {
        const lexer::Token& combined = consume();
        const SourceRange combinedRange = combined.range();
        const SourceLocation equalBegin(
            combinedRange.begin().fileId(), combinedRange.end().offset() - 1U);
        pendingGenericEqualToken_.emplace(
            TokenKind::Equal,
            SourceRange(equalBegin, combinedRange.end()),
            combined.lexeme().substr(2U, 1U));
        pendingGenericEqual_ = true;
        ++pendingGenericGreater_;
        return true;
    }
    return false;
}

bool Parser::canAppearInTypeArgumentList(TokenKind kind) noexcept
{
    if (lexer::isBuiltinTypeKeyword(kind) || lexer::isOwnershipQualifierKeyword(kind)) return true;
    switch (kind)
    {
        case TokenKind::Identifier:
        case TokenKind::Dot:
        case TokenKind::Comma:
        case TokenKind::Star:
        case TokenKind::LeftBracket:
        case TokenKind::RightBracket:
        case TokenKind::LeftParen:
        case TokenKind::RightParen:
        case TokenKind::Question:
        case TokenKind::Arrow:
        case TokenKind::KwNoHeap:
        case TokenKind::KwVectorCall:
        case TokenKind::IntegerLiteral:
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::GreaterGreater:
        case TokenKind::Ellipsis:
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Slash:
        case TokenKind::Percent:
        case TokenKind::Bang:
        case TokenKind::AmpAmp:
        case TokenKind::PipePipe:
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::Tilde:
        case TokenKind::LessLess:
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
        case TokenKind::LessEqual:
        case TokenKind::GreaterEqual:
        case TokenKind::KwTrue:
        case TokenKind::KwFalse:
            return true;
        default:
            return false;
    }
}

bool Parser::genericLookaheadTokenIsAllowed(
    std::size_t cursor,
    int parenthesisDepth) const noexcept
{
    const TokenKind kind = tokens_[cursor].kind();
    if (!canAppearInTypeArgumentList(kind)) return false;
    const bool functionReferenceCarrier = kind == TokenKind::Amp && cursor > 0 &&
        tokens_[cursor - 1U].kind() == TokenKind::Identifier &&
        tokens_[cursor - 1U].lexeme() == "fn";
    if (parenthesisDepth == 0 &&
        (kind == TokenKind::AmpAmp || kind == TokenKind::PipePipe ||
         kind == TokenKind::EqualEqual || kind == TokenKind::BangEqual ||
         kind == TokenKind::LessEqual || kind == TokenKind::GreaterEqual ||
         kind == TokenKind::Slash || kind == TokenKind::Percent ||
         (kind == TokenKind::Amp && !functionReferenceCarrier) ||
         kind == TokenKind::Pipe || kind == TokenKind::Caret || kind == TokenKind::LessLess))
        return false;
    if (parenthesisDepth == 0 &&
        (kind == TokenKind::Plus || kind == TokenKind::Minus ||
         kind == TokenKind::Bang || kind == TokenKind::Tilde))
    {
        const TokenKind previous = tokens_[cursor - 1U].kind();
        if (previous != TokenKind::Less && previous != TokenKind::Comma) return false;
    }
    return true;
}

bool Parser::looksLikeGenericCallTypeArguments() const noexcept
{
    if (!at(TokenKind::Less)) return false;
    std::size_t cursor = index_ + 1U;
    int genericDepth = 1;
    int parenthesisDepth = 0;
    bool expectsArgument = true;
    bool sawArgument = false;
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!genericLookaheadTokenIsAllowed(cursor, parenthesisDepth)) return false;
        if (parenthesisDepth == 0 && genericDepth == 1 && expectsArgument &&
            kind != TokenKind::Greater && kind != TokenKind::GreaterGreater)
        {
            const bool constantArgument = kind == TokenKind::LeftParen ||
                kind == TokenKind::Plus || kind == TokenKind::Minus ||
                kind == TokenKind::Tilde || kind == TokenKind::Bang ||
                kind == TokenKind::IntegerLiteral || kind == TokenKind::KwTrue ||
                kind == TokenKind::KwFalse;
            if (!constantArgument && !canStartTypeAt(cursor)) return false;
            expectsArgument = false;
            sawArgument = true;
        }
        if (kind == TokenKind::LeftParen) ++parenthesisDepth;
        else if (kind == TokenKind::RightParen)
        {
            if (--parenthesisDepth < 0) return false;
        }
        else if (parenthesisDepth == 0 && kind == TokenKind::Less) ++genericDepth;
        else if (parenthesisDepth == 0 && genericDepth == 1 && kind == TokenKind::Comma)
            expectsArgument = true;
        else if (parenthesisDepth == 0 &&
                 (kind == TokenKind::Greater || kind == TokenKind::GreaterGreater))
        {
            genericDepth -= kind == TokenKind::GreaterGreater ? 2 : 1;
            if (genericDepth == 0)
            {
                if (expectsArgument && sawArgument) return false;
                return cursor + 1U < tokens_.size() &&
                    tokens_[cursor + 1U].kind() == TokenKind::LeftParen;
            }
            if (genericDepth < 0) return false;
        }
        ++cursor;
    }
    return false;
}

bool Parser::scanClosedGenericTypePrefix(std::size_t& cursor) const noexcept
{
    if (cursor + 2U < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Colon &&
        tokens_[cursor + 1U].kind() == TokenKind::Colon &&
        tokens_[cursor + 2U].kind() == TokenKind::Identifier)
        cursor += 3U;
    while (cursor + 1U < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Dot &&
           tokens_[cursor + 1U].kind() == TokenKind::Identifier)
        cursor += 2U;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Less) return false;
    ++cursor;
    return true;
}

bool Parser::looksLikeClosedGenericTypeReference() const noexcept
{
    if (!at(TokenKind::Identifier)) return false;
    std::size_t cursor = index_ + 1U;
    if (!scanClosedGenericTypePrefix(cursor)) return false;
    int genericDepth = 1;
    int parenthesisDepth = 0;
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!genericLookaheadTokenIsAllowed(cursor, parenthesisDepth)) return false;
        if (kind == TokenKind::LeftParen) ++parenthesisDepth;
        else if (kind == TokenKind::RightParen)
        {
            if (--parenthesisDepth < 0) return false;
        }
        else if (parenthesisDepth == 0 && kind == TokenKind::Less) ++genericDepth;
        else if (parenthesisDepth == 0 &&
                 (kind == TokenKind::Greater || kind == TokenKind::GreaterGreater))
        {
            genericDepth -= kind == TokenKind::GreaterGreater ? 2 : 1;
            if (genericDepth == 0)
                return cursor + 2U < tokens_.size() &&
                    tokens_[cursor + 1U].kind() == TokenKind::Dot &&
                    tokens_[cursor + 2U].kind() == TokenKind::Identifier;
            if (genericDepth < 0) return false;
        }
        ++cursor;
    }
    return false;
}
}
