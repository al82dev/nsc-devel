#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/IterationSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Target/TargetInfo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;

constexpr std::array<TokenKind, 1> kLogicalOr = {TokenKind::PipePipe};
constexpr std::array<TokenKind, 1> kLogicalAnd = {TokenKind::AmpAmp};
constexpr std::array<TokenKind, 1> kBitwiseOr = {TokenKind::Pipe};
constexpr std::array<TokenKind, 1> kBitwiseXor = {TokenKind::Caret};
constexpr std::array<TokenKind, 1> kBitwiseAnd = {TokenKind::Amp};
constexpr std::array<TokenKind, 2> kEquality = {TokenKind::EqualEqual, TokenKind::BangEqual};
constexpr std::array<TokenKind, 4> kRelational = {
    TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual};
constexpr std::array<TokenKind, 2> kShift = {TokenKind::LessLess, TokenKind::GreaterGreater};
constexpr std::array<TokenKind, 2> kAdditive = {TokenKind::Plus, TokenKind::Minus};
constexpr std::array<TokenKind, 3> kMultiplicative = {TokenKind::Star, TokenKind::Slash, TokenKind::Percent};

bool contains(std::span<const TokenKind> values, TokenKind kind) noexcept
{
    for (const TokenKind value : values)
    {
        if (value == kind)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::int64_t> signedIntegerLiteral(const ast::AstNode* expression) noexcept
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression)
    {
        if (expression->integerValue() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::nullopt;
        return static_cast<std::int64_t>(expression->integerValue());
    }
    if (expression->kind() != ast::AstNodeKind::UnaryExpression ||
        expression->tokenKind() != TokenKind::Minus || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IntegerLiteralExpression)
        return std::nullopt;
    const std::uint64_t magnitude = expression->first()->integerValue();
    if (magnitude > (std::uint64_t{1} << 63U)) return std::nullopt;
    if (magnitude == (std::uint64_t{1} << 63U))
        return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}

void applyFloatingPointModeToTree(
    ast::AstNode* node,
    ast::FloatingPointModeSyntaxKind mode)
{
    if (node == nullptr || node->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        return;
    node->setFloatingPointMode(mode);
    applyFloatingPointModeToTree(node->first(), mode);
    applyFloatingPointModeToTree(node->second(), mode);
    applyFloatingPointModeToTree(node->third(), mode);
    for (ast::AstNode* child : node->children())
        applyFloatingPointModeToTree(child, mode);
}

ast::AstNode* cloneParsedNode(ast::AstArena& arena, const ast::AstNode* source);

ast::TypeSyntax* cloneParsedType(ast::AstArena& arena, const ast::TypeSyntax* source)
{
    if (source == nullptr) return nullptr;
    ast::TypeSyntax* result = arena.make<ast::TypeSyntax>(
        source->range(), source->baseTokenKind(), source->baseName());
    result->setOwnershipQualifier(source->ownershipQualifier());
    result->setLifetimeName(source->lifetimeName());
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
    result->setFunctionReturnType(cloneParsedType(arena, source->functionReturnType()));
    result->setFunctionErrorType(cloneParsedType(arena, source->functionErrorType()));
    result->setConstExpression(cloneParsedNode(arena, source->constExpression()));
    result->setPackExpansion(source->isPackExpansion());
    for (std::size_t index = 0; index < source->typeArguments().size(); ++index)
    {
        ast::TypeSyntax* argument = cloneParsedType(arena, source->typeArguments()[index]);
        if (source->isTuple())
        {
            const std::string name = index < source->tupleElementNames().size()
                ? source->tupleElementNames()[index] : std::string{};
            result->addTupleElement(argument, name);
        }
        else result->addTypeArgument(argument);
    }
    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        ast::TypeModifierSyntax copy = modifier;
        copy.arrayCount = cloneParsedNode(arena, modifier.arrayCount);
        result->addModifier(std::move(copy));
    }
    if (source->originProjectOrdinal().has_value())
        result->setOriginProjectOrdinal(*source->originProjectOrdinal());
    if (source->originNamespace().has_value())
        result->setOriginNamespace(*source->originNamespace());
    return result;
}

ast::AstNode* cloneParsedNode(ast::AstArena& arena, const ast::AstNode* source)
{
    if (source == nullptr) return nullptr;
    ast::AstNode* result = arena.make<ast::AstNode>(source->kind(), source->range());
    result->setText(source->text());
    result->setAuxiliaryText(source->auxiliaryText());
    result->setArgumentName(source->argumentName());
    result->setTokenKind(source->tokenKind());
    result->setFlag(source->flag());
    result->setReference(source->isReference());
    result->setLifetimeName(source->lifetimeName());
    result->setVariadic(source->isVariadic());
    result->setUnsafe(source->isUnsafe());
    result->setFloatingPointMode(source->floatingPointMode());
    result->setFfiUnwindPolicy(source->ffiUnwindPolicy());
    result->setIntegerValue(source->integerValue());
    result->setFloatingValue(source->floatingValue());
    for (const TokenKind modifier : source->modifiers()) result->addModifier(modifier);
    result->setTypeSyntax(cloneParsedType(arena, source->typeSyntax()));
    result->setThrowsTypeSyntax(cloneParsedType(arena, source->throwsTypeSyntax()));
    for (const ast::TypeSyntax* syntax : source->typeSyntaxes())
        result->addTypeSyntax(cloneParsedType(arena, syntax));
    for (const ast::GenericParameterSyntax& parameter : source->genericParameterSyntaxes())
    {
        ast::GenericParameterSyntax copy = parameter;
        copy.constantType = cloneParsedType(arena, parameter.constantType);
        result->addGenericParameter(std::move(copy));
    }
    for (const ast::LifetimeParameterSyntax& parameter : source->lifetimeParameters())
        result->addLifetimeParameter(parameter);
    for (const ast::LifetimeOutlivesConstraint& constraint : source->lifetimeOutlivesConstraints())
        result->addLifetimeOutlivesConstraint(constraint);
    for (const ast::GenericConstraintClause& clause : source->genericConstraints())
    {
        ast::GenericConstraintClause copy = clause;
        copy.equalityConstraint = cloneParsedType(arena, clause.equalityConstraint);
        copy.associatedProjection = cloneParsedType(arena, clause.associatedProjection);
        copy.substitutedArgument = cloneParsedType(arena, clause.substitutedArgument);
        copy.typeConstraints.clear();
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            copy.typeConstraints.push_back(cloneParsedType(arena, constraint));
        result->addGenericConstraint(std::move(copy));
    }
    result->setFirst(cloneParsedNode(arena, source->first()));
    result->setSecond(cloneParsedNode(arena, source->second()));
    result->setThird(cloneParsedNode(arena, source->third()));
    for (const ast::AstNode* child : source->children())
        result->addChild(cloneParsedNode(arena, child));
    return result;
}

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

// Generic parameter names are types only inside the declaration that introduces them.  The
// statement lookahead intentionally consults knownTypeNames_ so it can distinguish `T value`
// from an expression; keeping T globally would make an unrelated declaration in a later type
// parse differently.  This guard installs only type parameters (never const parameters) and
// removes exactly the names it inserted, preserving an equally named outer generic parameter.
class KnownGenericTypeScope final
{
public:
    KnownGenericTypeScope(
        std::unordered_set<std::string>& knownTypeNames,
        const ast::AstNode* declaration)
        : knownTypeNames_(knownTypeNames)
    {
        if (declaration == nullptr) return;
        for (const ast::GenericParameterSyntax& parameter :
             declaration->genericParameterSyntaxes())
        {
            if (parameter.kind != ast::GenericParameterSyntaxKind::Type &&
                parameter.kind != ast::GenericParameterSyntaxKind::TypePack)
            {
                continue;
            }
            if (knownTypeNames_.insert(parameter.name).second)
            {
                inserted_.push_back(parameter.name);
            }
        }
    }

    ~KnownGenericTypeScope()
    {
        for (const std::string& name : inserted_) knownTypeNames_.erase(name);
    }

    KnownGenericTypeScope(const KnownGenericTypeScope&) = delete;
    KnownGenericTypeScope& operator=(const KnownGenericTypeScope&) = delete;

private:
    std::unordered_set<std::string>& knownTypeNames_;
    std::vector<std::string> inserted_;
};

bool isStaticInterpolationHole(const language::InterpolationPart& part) noexcept
{
    return language::builtinStaticInterpolationHoleSupported(part);
}

bool isInvalidVoidObjectType(const ast::TypeSyntax* type) noexcept
{
    if (type == nullptr || type->baseTokenKind() != TokenKind::KwVoid)
    {
        return false;
    }

    const auto& modifiers = type->modifiers();
    return modifiers.empty() ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::Array ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::DynamicArray ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::RangeArray ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::FlexibleArray;
}

bool isAccessibilityToken(TokenKind kind) noexcept
{
    return kind == TokenKind::KwPublic || kind == TokenKind::KwInternal ||
        kind == TokenKind::KwProtected || kind == TokenKind::KwPrivate;
}

bool isStage38OperatorToken(TokenKind kind) noexcept
{
    switch (kind)
    {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
        case TokenKind::Bang:
        case TokenKind::Tilde:
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
        case TokenKind::PlusPlus:
        case TokenKind::MinusMinus:
            return true;
        default:
            return false;
    }
}

std::string_view operatorSpelling(TokenKind kind) noexcept
{
    switch (kind)
    {
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::Bang: return "!";
        case TokenKind::Tilde: return "~";
        case TokenKind::Amp: return "&";
        case TokenKind::Pipe: return "|";
        case TokenKind::Caret: return "^";
        case TokenKind::LessLess: return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::EqualEqual: return "==";
        case TokenKind::BangEqual: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::Greater: return ">";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::AmpAmp: return "&&";
        case TokenKind::PipePipe: return "||";
        case TokenKind::QuestionQuestion: return "??";
        default: return {};
    }
}

std::string operatorMethodName(TokenKind kind, std::size_t arity)
{
    if (kind == TokenKind::PlusPlus)
        return arity == 1 ? "op_Increment" : "op_Invalid";
    if (kind == TokenKind::MinusMinus)
        return arity == 1 ? "op_Decrement" : "op_Invalid";
    const std::string_view method = language::operatorMethodName(operatorSpelling(kind), arity);
    return method.empty() ? "op_Invalid" : std::string(method);
}
}

Parser::Parser(
    std::span<const lexer::Token> tokens,
    ast::AstArena& arena,
    diagnostics::DiagnosticEngine& diagnostics,
    std::span<const std::string> knownTypeNames)
    : tokens_(tokens),
      arena_(arena),
      diagnostics_(diagnostics)
{
    static constexpr std::array<std::string_view, 16> builtinNames = {
        "void", "bool", "byte", "char", "short", "ushort", "int", "uint", "long", "ulong",
        "nint", "nuint", "float", "double", "string",
        // C8 "type root": `object` is a KNOWN TYPE NAME here so that `object x = ...;` parses as
        // a declaration, but it is deliberately NOT a lexer keyword and gets no builtin type key:
        // name lookup resolves it, as a last resort, to NativeSharp.Core.Object (see
        // ProjectNameLookup::bindTypeSyntax). A project without the root in view therefore gets
        // "unknown type name 'object'" rather than a parse that quietly means something else.
        "object"};

    for (const std::string_view name : builtinNames)
    {
        knownTypeNames_.insert(std::string(name));
    }
    for (const std::string& name : knownTypeNames)
    {
        if (!name.empty())
        {
            knownTypeNames_.insert(name);
        }
    }
}

ast::AstNode* Parser::parseCompilationUnit()
{
    const SourceRange start = current().range();
    ast::AstNode* unit = arena_.make<ast::AstNode>(ast::AstNodeKind::CompilationUnit, start);

    while (!at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* declaration = parseNamespaceMember();
        if (declaration != nullptr)
        {
            unit->addChild(declaration);
        }

        if (index_ == before)
        {
            consume();
        }
    }

    unit->setRange(join(start, current().range()));
    return unit;
}

const lexer::Token& Parser::current() const noexcept
{
    if (pendingGenericEqual_)
        return *pendingGenericEqualToken_;
    return tokens_[index_ < tokens_.size() ? index_ : tokens_.size() - 1];
}

const lexer::Token& Parser::peek(std::size_t distance) const noexcept
{
    if (pendingGenericEqual_ && distance == 0U)
        return *pendingGenericEqualToken_;
    const std::size_t target = pendingGenericEqual_
        ? index_ + distance - 1U
        : index_ + distance;
    return tokens_[target < tokens_.size() ? target : tokens_.size() - 1];
}

bool Parser::at(TokenKind kind) const noexcept
{
    return current().kind() == kind;
}

bool Parser::atContextualKeyword(std::string_view keyword) const noexcept
{
    return current().kind() == TokenKind::Identifier && current().lexeme() == keyword;
}

bool Parser::startsParenthesizedBlock(std::string_view keyword) const noexcept
{
    if (!atContextualKeyword(keyword) || peek().kind() != TokenKind::LeftParen)
        return false;

    std::size_t depth = 0;
    for (std::size_t cursor = index_ + 1U; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen)
        {
            ++depth;
            continue;
        }
        if (kind != TokenKind::RightParen || depth == 0)
            continue;

        --depth;
        if (depth == 0)
            return cursor + 1U < tokens_.size() &&
                tokens_[cursor + 1U].kind() == TokenKind::LeftBrace;
    }
    return false;
}

bool Parser::expectContextualKeyword(
    std::string_view keyword,
    std::string_view description)
{
    if (atContextualKeyword(keyword))
    {
        consume();
        return true;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected " + std::string(description) + ", found '" +
            std::string(current().lexeme()) + "'");
    return false;
}

bool Parser::consumeIf(TokenKind kind) noexcept
{
    if (!at(kind))
    {
        return false;
    }
    consume();
    return true;
}

const lexer::Token& Parser::consume() noexcept
{
    const lexer::Token& token = current();
    if (pendingGenericEqual_)
    {
        pendingGenericEqual_ = false;
        return token;
    }
    if (!at(TokenKind::EndOfFile))
    {
        ++index_;
    }
    return token;
}

bool Parser::expect(TokenKind kind, std::string_view description)
{
    if (consumeIf(kind))
    {
        return true;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected " + std::string(description) + ", found '" + std::string(current().lexeme()) + "'");
    return false;
}

SourceRange Parser::join(SourceRange begin, SourceRange end) const noexcept
{
    return SourceRange(begin.begin(), end.end());
}

SourceRange Parser::tokenRangeFrom(std::size_t beginIndex) const noexcept
{
    const std::size_t endIndex = index_ == 0 ? 0 : index_ - 1;
    return join(tokens_[beginIndex].range(), tokens_[endIndex].range());
}

bool Parser::canStartType(const lexer::Token& token) const noexcept
{
    if (lexer::isOwnershipQualifierKeyword(token.kind()))
    {
        return true;
    }

    if (lexer::isBuiltinTypeKeyword(token.kind()))
    {
        return true;
    }

    if (token.kind() == TokenKind::Identifier && token.lexeme() == "fn")
    {
        return true;
    }

    if (token.kind() == TokenKind::Identifier &&
        (token.lexeme() == "simd" || token.lexeme() == "mask"))
    {
        return true;
    }

    return token.kind() == TokenKind::Identifier && knownTypeNames_.contains(std::string(token.lexeme()));
}

bool Parser::canStartTypeAt(std::size_t tokenIndex) const noexcept
{
    std::size_t afterType = tokenIndex;
    return scanTypeAt(tokenIndex, afterType);
}

bool Parser::scanTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept
{
    if (tokenIndex >= tokens_.size())
    {
        return false;
    }

    const auto scanPointerQualifiers = [&](std::size_t cursor) noexcept
    {
        if (cursor < tokens_.size() &&
            (tokens_[cursor].kind() == TokenKind::Question ||
             tokens_[cursor].kind() == TokenKind::Bang))
            ++cursor;
        while (cursor + 1U < tokens_.size() &&
               tokens_[cursor].kind() == TokenKind::Identifier &&
               (tokens_[cursor].lexeme() == "align" ||
                tokens_[cursor].lexeme() == "addrspace" ||
                tokens_[cursor].lexeme() == "provenance") &&
               tokens_[cursor + 1U].kind() == TokenKind::LeftParen)
        {
            cursor += 2U;
            while (cursor < tokens_.size() &&
                   tokens_[cursor].kind() != TokenKind::RightParen &&
                   tokens_[cursor].kind() != TokenKind::EndOfFile)
                ++cursor;
            if (cursor >= tokens_.size() ||
                tokens_[cursor].kind() != TokenKind::RightParen)
                return tokens_.size();
            ++cursor;
        }
        return cursor;
    };

    if (lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
    {
        const TokenKind ownershipQualifier = tokens_[tokenIndex].kind();
        ++tokenIndex;
        if (tokenIndex >= tokens_.size() ||
            lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
        {
            return false;
        }
        if (ownershipQualifier == TokenKind::KwBorrowed &&
            tokens_[tokenIndex].kind() == TokenKind::LeftBracket)
        {
            if (tokenIndex + 2U >= tokens_.size() ||
                (tokens_[tokenIndex + 1U].kind() != TokenKind::Identifier &&
                 tokens_[tokenIndex + 1U].kind() != TokenKind::KwThis &&
                 tokens_[tokenIndex + 1U].kind() != TokenKind::KwStatic) ||
                tokens_[tokenIndex + 2U].kind() != TokenKind::RightBracket)
            {
                return false;
            }
            tokenIndex += 3U;
        }
    }

    if (tokenIndex < tokens_.size() &&
        tokens_[tokenIndex].kind() == TokenKind::Identifier &&
        (tokens_[tokenIndex].lexeme() == "dyn" || tokens_[tokenIndex].lexeme() == "some"))
    {
        std::size_t afterDynamicType = tokenIndex + 1U;
        if (scanTypeAt(tokenIndex + 1U, afterDynamicType))
        {
            afterType = afterDynamicType;
            return true;
        }
    }

    // T08-F01 structural function type. The carrier is mandatory so `fn * value` can never be
    // confused with multiplication and a callable reference remains visibly different from a
    // raw code pointer: `fn*(...) -> R` versus `fn&(...) -> R`.
    if (tokenIndex < tokens_.size() &&
        tokens_[tokenIndex].kind() == TokenKind::Identifier &&
        tokens_[tokenIndex].lexeme() == "fn")
    {
        std::size_t cursor = tokenIndex + 1U;
        if (cursor >= tokens_.size() ||
            (tokens_[cursor].kind() != TokenKind::Star &&
             tokens_[cursor].kind() != TokenKind::Amp))
            return false;
        ++cursor;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Question)
            ++cursor;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::LeftBracket)
        {
            if (cursor + 2U >= tokens_.size() ||
                (tokens_[cursor + 1U].kind() != TokenKind::Identifier &&
                 tokens_[cursor + 1U].kind() != TokenKind::KwThis &&
                 tokens_[cursor + 1U].kind() != TokenKind::KwStatic) ||
                tokens_[cursor + 2U].kind() != TokenKind::RightBracket)
                return false;
            cursor += 3U;
        }
        if (cursor < tokens_.size() &&
            ((tokens_[cursor].kind() == TokenKind::Identifier &&
              tokens_[cursor].lexeme() == "C") ||
             tokens_[cursor].kind() == TokenKind::KwVectorCall ||
             (tokens_[cursor].kind() == TokenKind::Identifier &&
              (tokens_[cursor].lexeme() == "native" ||
               tokens_[cursor].lexeme() == "cdecl" ||
               tokens_[cursor].lexeme() == "stdcall" ||
               tokens_[cursor].lexeme() == "fastcall" ||
               tokens_[cursor].lexeme() == "win64"))))
            ++cursor;
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::LeftParen)
            return false;
        ++cursor;
        bool sawParameter = false;
        while (cursor < tokens_.size() && tokens_[cursor].kind() != TokenKind::RightParen)
        {
            if (tokens_[cursor].kind() == TokenKind::Ellipsis)
            {
                if (!sawParameter || cursor + 1U >= tokens_.size() ||
                    tokens_[cursor + 1U].kind() != TokenKind::RightParen)
                    return false;
                ++cursor;
                break;
            }
            std::size_t afterParameter = cursor;
            if (!scanTypeAt(cursor, afterParameter)) return false;
            cursor = afterParameter;
            sawParameter = true;
            if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Comma)
            {
                ++cursor;
                continue;
            }
            break;
        }
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::RightParen)
            return false;
        ++cursor;
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Arrow)
            return false;
        ++cursor;
        std::size_t afterResult = cursor;
        if (!scanTypeAt(cursor, afterResult)) return false;
        cursor = afterResult;
        bool sawUnsafe = false;
        bool sawNoHeap = false;
        bool sawAsync = false;
        while (cursor < tokens_.size())
        {
            const bool unsafeEffect = tokens_[cursor].kind() == TokenKind::Identifier &&
                tokens_[cursor].lexeme() == "unsafe";
            const bool noHeapEffect = tokens_[cursor].kind() == TokenKind::KwNoHeap;
            const bool asyncEffect = tokens_[cursor].kind() == TokenKind::Identifier &&
                tokens_[cursor].lexeme() == "async";
            if ((!unsafeEffect && !noHeapEffect && !asyncEffect) ||
                (unsafeEffect && sawUnsafe) || (noHeapEffect && sawNoHeap) ||
                (asyncEffect && sawAsync))
                break;
            sawUnsafe = sawUnsafe || unsafeEffect;
            sawNoHeap = sawNoHeap || noHeapEffect;
            sawAsync = sawAsync || asyncEffect;
            ++cursor;
        }
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
            tokens_[cursor].lexeme() == "throws")
        {
            ++cursor;
            std::size_t afterError = cursor;
            if (!scanTypeAt(cursor, afterError)) return false;
            cursor = afterError;
        }
        afterType = cursor;
        return true;
    }

    if (tokens_[tokenIndex].kind() == TokenKind::LeftParen)
    {
        std::size_t cursor = tokenIndex + 1;
        std::size_t elementCount = 0;
        bool sawComma = false;
        while (cursor < tokens_.size() && tokens_[cursor].kind() != TokenKind::RightParen)
        {
            std::size_t afterElement = cursor;
            if (!scanTypeAt(cursor, afterElement)) return false;
            cursor = afterElement;
            if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
                cursor + 1 < tokens_.size() &&
                (tokens_[cursor + 1].kind() == TokenKind::Comma ||
                 tokens_[cursor + 1].kind() == TokenKind::RightParen))
            {
                ++cursor;
            }
            ++elementCount;
            if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Comma)
            {
                sawComma = true;
                ++cursor;
                if (cursor < tokens_.size() &&
                    tokens_[cursor].kind() == TokenKind::RightParen) break;
                continue;
            }
            break;
        }
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::RightParen)
            return false;
        // `(T)` remains ordinary grouping and is not a type spelling. `(T,)` is the unary tuple;
        // `()` is the unit tuple. Arity >= 2 necessarily encountered a separator comma.
        if (elementCount == 1 && !sawComma) return false;
        ++cursor;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::KwConst)
            ++cursor;
        while (cursor < tokens_.size())
        {
            if (tokens_[cursor].kind() == TokenKind::Star)
            {
                cursor = scanPointerQualifiers(cursor + 1U);
                continue;
            }
            if (tokens_[cursor].kind() != TokenKind::LeftBracket) break;
            int depth = 1;
            ++cursor;
            while (cursor < tokens_.size() && depth != 0)
            {
                if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
                else if (tokens_[cursor].kind() == TokenKind::RightBracket) --depth;
                else if (tokens_[cursor].kind() == TokenKind::EndOfFile) return false;
                ++cursor;
            }
            if (depth != 0) return false;
        }
        afterType = cursor;
        return true;
    }
    const lexer::Token& token = tokens_[tokenIndex];
    const bool builtin = lexer::isBuiltinTypeKeyword(token.kind());
    const bool portableSimd = token.kind() == TokenKind::Identifier &&
        (token.lexeme() == "simd" || token.lexeme() == "mask");
    if (token.kind() != TokenKind::Identifier)
    {
        if (!builtin) return false;
    }

    std::string candidate(token.lexeme());
    std::size_t cursor = tokenIndex + 1;
    bool qualified = false;
    bool moduleQualified = false;
    if (cursor + 2 < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Colon &&
        tokens_[cursor + 1].kind() == TokenKind::Colon &&
        tokens_[cursor + 2].kind() == TokenKind::Identifier)
    {
        candidate += "::";
        candidate += tokens_[cursor + 2].lexeme();
        cursor += 3;
        qualified = true;
        moduleQualified = true;
    }
    while (cursor + 1 < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Dot &&
           tokens_[cursor + 1].kind() == TokenKind::Identifier)
    {
        qualified = true;
        candidate.push_back('.');
        candidate += tokens_[cursor + 1].lexeme();
        cursor += 2;
    }

    if (!builtin && qualified)
    {
        if (!moduleQualified && !knownTypeNames_.contains(candidate)) return false;
    }
    else if (!builtin && !portableSimd && !knownTypeNames_.contains(candidate)) return false;

    if (!builtin && cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Less)
    {
        int depth = 0;
        do
        {
            const TokenKind kind = tokens_[cursor].kind();
            if (kind == TokenKind::Less) ++depth;
            else if (kind == TokenKind::Greater) --depth;
            else if (kind == TokenKind::GreaterGreater ||
                     kind == TokenKind::GreaterGreaterEqual) depth -= 2;
            else if (kind == TokenKind::EndOfFile) return false;
            ++cursor;
        }
        while (cursor < tokens_.size() && depth > 0);
        if (depth != 0) return false;
    }
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::KwConst)
        ++cursor;
    while (cursor < tokens_.size())
    {
        if (tokens_[cursor].kind() == TokenKind::Star)
        {
            cursor = scanPointerQualifiers(cursor + 1U);
            continue;
        }
        if (tokens_[cursor].kind() != TokenKind::LeftBracket) break;
        int depth = 1;
        ++cursor;
        while (cursor < tokens_.size() && depth != 0)
        {
            if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
            else if (tokens_[cursor].kind() == TokenKind::RightBracket) --depth;
            else if (tokens_[cursor].kind() == TokenKind::EndOfFile) return false;
            ++cursor;
        }
        if (depth != 0) return false;
    }
    afterType = cursor;
    return true;
}

// NSC-LIMIT-013. Un nombre que TAMBIEN es tipo no convierte por si solo una
// sentencia en declaracion. La forma sintactica minima de una declaracion es
// `tipo identificador`; si tras el tipo viene `=`, `.`, `(`, un operador
// compuesto, etc., es una expresion. Esto importa cuando un grafo importado
// contiene `Raw.Gl` y la clase actual declara el campo C#-idiomatico `GL Gl`:
// dentro del constructor, `Gl = gl;` empieza por un nombre que existe como tipo
// EN OTRO namespace, pero el `=` demuestra que no puede ser una declaracion.
bool Parser::looksLikeVariableDeclaration() const noexcept
{
    std::size_t cursor = index_;
    // NativeSharp 2.0 item 312: `ref` is contextual and introduces a reference-local
    // declaration only when a real type follows it. Skip it for the ordinary type/name lookahead.
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
        tokens_[cursor].lexeme() == "ref")
    {
        ++cursor;
    }
    std::size_t afterType = cursor;
    if (!scanTypeAt(cursor, afterType))
    {
        // Preserve a type-shaped declaration even when its leading identifier is unknown.  The
        // name-lookup phase can then issue the causal "unknown type name 'U'" diagnostic instead
        // of the parser treating `U value;` as two adjacent expressions and reporting a stray
        // semicolon.  This deliberately accepts only the unambiguous declaration prefix needed
        // here: identifier, optional pointer/empty-array suffixes, then identifier.
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Identifier)
            return false;
        ++cursor;
        // C-style declaration/expression disambiguation needs a typedef-like known name for
        // pointer declarators. If the leading identifier is unknown, `x * x;` is multiplication,
        // not a declaration of the second x. Known types already take the scanTypeAt path above.
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Star) return false;
        while (cursor + 1U < tokens_.size() &&
               tokens_[cursor].kind() == TokenKind::LeftBracket &&
               tokens_[cursor + 1U].kind() == TokenKind::RightBracket)
        {
            cursor += 2U;
        }
        return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier;
    }
    cursor = afterType;

    return cursor < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Identifier;
}

// NS304-T04-F06: `type name (...)` al principio de una sentencia es una funcion local. Se
// distingue de una declaracion de variable (`type name ;`/`=`) por el `(` tras el nombre.
bool Parser::looksLikeLocalFunction() const noexcept
{
    std::size_t cursor = index_;
    std::size_t afterType = cursor;
    if (!scanTypeAt(cursor, afterType)) return false;
    cursor = afterType;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Identifier) return false;
    ++cursor;
    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::LeftParen;
}

ast::AstNode* Parser::parseLocalFunctionAsLambda()
{
    // NS304-T04-F06: baja `int name(params) { body }` a `var name = (params) => body;` para
    // reutilizar la inferencia de tipo callable natural de lambda (T04-F07). La recursion
    // (autoreferencia) es follow-up.
    const std::size_t beginIndex = index_;
    ast::AstNode* function = parseFunctionOrGlobalVariable();
    if (function == nullptr || function->kind() != ast::AstNodeKind::FunctionDeclaration)
    {
        return function;
    }
    ast::AstNode* lambda = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LambdaExpression, function->range());
    for (ast::AstNode* parameter : function->children())
    {
        lambda->addChild(cloneParsedNode(arena_, parameter));
    }
    // Conserva el tipo de retorno explicito para que el Sema construya el fn* (recursion).
    lambda->setTypeSyntax(cloneParsedType(arena_, function->typeSyntax()));
    // NS304-T04-F06: el lambda lleva su nombre para que el Sema registre la auto-referencia en el
    // scope del lambda (no en el enclosing) y el cuerpo pueda llamarse a si mismo sin capturar.
    lambda->setAuxiliaryText("local-function:" + function->text());
    // Un lambda natural no-convertido exige cuerpo de EXPRESION (el retorno se infiere del
    // cuerpo). Si el cuerpo es `{ return expr; }` (un solo return), se usa `expr` como cuerpo.
    ast::AstNode* body = function->first();
    if (body != nullptr && body->kind() == ast::AstNodeKind::BlockStatement &&
        body->children().size() == 1U && body->children()[0] != nullptr &&
        body->children()[0]->kind() == ast::AstNodeKind::ReturnStatement)
    {
        lambda->setFirst(cloneParsedNode(arena_, body->children()[0]->first()));
        lambda->setFlag(false);
    }
    else
    {
        lambda->setFirst(cloneParsedNode(arena_, body));
        lambda->setFlag(body != nullptr &&
            body->kind() == ast::AstNodeKind::BlockStatement);
    }

    ast::TypeSyntax* varType = arena_.make<ast::TypeSyntax>(
        function->range(), TokenKind::KwVar, "var");
    ast::AstNode* declaration = makeVariableDeclaration(
        varType, function->text(), lambda, false, false);
    declaration->setAuxiliaryText("local-function");
    declaration->setRange(tokenRangeFrom(beginIndex));
    return declaration;
}

// A declaration used as a condition must initialize the declared name.  Reusing the broader
// statement lookahead here made `if (left * right >= limit)` look like `Type* name` whenever the
// first identifier happened to be type-shaped.  Requiring the initializer token both models the
// deferred construct we diagnose (`T value = expression`) and leaves ordinary multiplication to
// the expression parser. Keep this lookahead narrower than the ordinary local-declaration probe.
bool Parser::looksLikeConditionDeclaration() const noexcept
{
    std::size_t cursor = index_;
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
        tokens_[cursor].lexeme() == "ref")
    {
        ++cursor;
    }
    std::size_t afterType = cursor;
    if (!scanTypeAt(cursor, afterType)) return false;
    cursor = afterType;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Identifier) return false;
    ++cursor;
    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Equal;
}

bool Parser::looksLikeParenthesizedType() const noexcept
{
    if (!at(TokenKind::LeftParen))
    {
        return false;
    }

    std::size_t afterType = index_ + 1U;
    if (!scanTypeAt(index_ + 1U, afterType)) return false;
    return afterType < tokens_.size() && tokens_[afterType].kind() == TokenKind::RightParen;
}

bool Parser::parseQualifiedName(std::string& name, SourceRange& range)
{
    name.clear();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "identifier");
        return false;
    }

    const SourceRange start = current().range();
    SourceRange end = start;
    name = std::string(consume().lexeme());
    if (at(TokenKind::Colon) && peek().kind() == TokenKind::Colon)
    {
        consume();
        consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "type name after module qualifier '::'");
            range = join(start, end);
            return false;
        }
        const lexer::Token& segment = consume();
        name += "::";
        name += segment.lexeme();
        end = segment.range();
    }
    while (consumeIf(TokenKind::Dot))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "identifier after '.'");
            range = join(start, end);
            return false;
        }
        const lexer::Token& segment = consume();
        name.push_back('.');
        name += segment.lexeme();
        end = segment.range();
    }
    range = join(start, end);
    return true;
}

ast::TypeSyntax* Parser::parseType()
{
    ast::OwnershipQualifierSyntaxKind ownershipQualifier = ast::OwnershipQualifierSyntaxKind::None;
    std::string lifetimeName;
    SourceRange qualifierRange = current().range();
    if (lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        const lexer::Token& qualifier = consume();
        qualifierRange = qualifier.range();
        ownershipQualifier = qualifier.kind() == TokenKind::KwOwned
            ? ast::OwnershipQualifierSyntaxKind::Owned
            : ast::OwnershipQualifierSyntaxKind::Borrowed;

        if (lexer::isOwnershipQualifierKeyword(current().kind()))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedType,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "an ownership-qualified type accepts exactly one qualifier");
            return nullptr;
        }
        if (ownershipQualifier == ast::OwnershipQualifierSyntaxKind::Borrowed)
            lifetimeName = parseOptionalLifetimeAnnotation("borrowed type");
        else if (at(TokenKind::LeftBracket))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "only a borrowed type may carry a named lifetime annotation");
            parseOptionalLifetimeAnnotation("owned type");
        }
    }

    bool dynamicTrait = false;
    SourceRange dynamicRange = current().range();
    if (atContextualKeyword("dyn") && canStartTypeAt(index_ + 1U))
    {
        const lexer::Token& qualifier = consume();
        dynamicRange = qualifier.range();
        dynamicTrait = true;
    }

    bool opaqueResult = false;
    SourceRange opaqueRange = current().range();
    if (atContextualKeyword("some") && canStartTypeAt(index_ + 1U))
    {
        const lexer::Token& qualifier = consume();
        opaqueRange = qualifier.range();
        opaqueResult = true;
        if (dynamicTrait)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                join(dynamicRange, opaqueRange),
                "'dyn' and 'some' are mutually exclusive trait representations");
        }
    }

    SourceRange longDoubleRange{};
    bool recoverLongDoubleAsDouble = false;
    if (at(TokenKind::KwLong) && peek().kind() == TokenKind::KwDouble)
    {
        longDoubleRange = join(current().range(), peek().range());
        reportDeferredLanguage3(
            diagnostics_, longDoubleRange, language::Language3Feature::LongDouble);
        consume();
        recoverLongDoubleAsDouble = true;
    }
    if (atContextualKeyword("decimal"))
    {
        reportDeferredLanguage3(
            diagnostics_, current().range(), language::Language3Feature::Decimal);
    }

    if (atContextualKeyword("fn"))
    {
        const SourceRange functionStart = current().range();
        consume();
        if (dynamicTrait || opaqueResult)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                dynamicTrait ? dynamicRange : opaqueRange,
                "a function type cannot use 'dyn' or 'some'");
        }

        ast::FunctionTypeCarrierSyntaxKind carrier;
        if (consumeIf(TokenKind::Star))
            carrier = ast::FunctionTypeCarrierSyntaxKind::Pointer;
        else if (consumeIf(TokenKind::Amp))
            carrier = ast::FunctionTypeCarrierSyntaxKind::Reference;
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected '*' (function pointer) or '&' (function reference) after 'fn'");
            return nullptr;
        }

        const bool nullable = consumeIf(TokenKind::Question);
        std::string functionLifetime;
        if (at(TokenKind::LeftBracket))
            functionLifetime = parseOptionalLifetimeAnnotation("function reference");
        if (!functionLifetime.empty() && carrier != ast::FunctionTypeCarrierSyntaxKind::Reference)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                functionStart,
                "only a function reference may carry a named lifetime");
        }

        ast::FunctionCallingConventionSyntaxKind convention =
            ast::FunctionCallingConventionSyntaxKind::NativeSharp;
        if (atContextualKeyword("C"))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::C;
            consume();
        }
        else if (at(TokenKind::KwVectorCall))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::VectorCall;
            consume();
        }
        else if (atContextualKeyword("native"))
        {
            consume();
        }
        else if (atContextualKeyword("cdecl"))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::C;
            consume();
        }
        else if (atContextualKeyword("stdcall"))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::StdCall;
            consume();
        }
        else if (atContextualKeyword("fastcall"))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::FastCall;
            consume();
        }
        else if (atContextualKeyword("win64"))
        {
            convention = ast::FunctionCallingConventionSyntaxKind::Win64;
            consume();
        }
        if (carrier == ast::FunctionTypeCarrierSyntaxKind::Reference &&
            convention != ast::FunctionCallingConventionSyntaxKind::NativeSharp)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                functionStart,
                "a function reference uses the NativeSharp calling convention; foreign conventions require fn*");
        }

        if (!expect(TokenKind::LeftParen, "'(' after function type carrier")) return nullptr;
        ast::TypeSyntax* function = arena_.make<ast::TypeSyntax>(
            functionStart, TokenKind::Invalid, "$function");
        function->setFunction();
        function->setOwnershipQualifier(ownershipQualifier);
        function->setLifetimeName(std::move(lifetimeName));
        function->setFunctionCarrier(carrier);
        function->setFunctionCallingConvention(convention);
        function->setFunctionNullable(nullable);
        function->setFunctionLifetimeName(std::move(functionLifetime));

        bool sawParameter = false;
        while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
        {
            if (consumeIf(TokenKind::Ellipsis))
            {
                function->setFunctionVariadic();
                if (!sawParameter)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        tokens_[index_ - 1U].range(),
                        "a variadic function type requires at least one fixed parameter");
                }
                if (!at(TokenKind::RightParen))
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "'...' must be the final item in a function type parameter list");
                }
                break;
            }
            ast::TypeSyntax* parameter = parseType();
            if (parameter == nullptr) return nullptr;
            if (parameter->baseTokenKind() == TokenKind::KwVoid &&
                parameter->modifiers().empty())
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter->range(),
                    "a function type parameter cannot have type void");
            }
            function->addTypeArgument(parameter);
            sawParameter = true;
            if (!consumeIf(TokenKind::Comma)) break;
            if (at(TokenKind::RightParen))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "a function type parameter list cannot end with ','; use ', ...' for variadicity");
                break;
            }
        }
        if (!expect(TokenKind::RightParen, "')' after function type parameters")) return nullptr;
        if (!expect(TokenKind::Arrow, "'->' after function type parameters")) return nullptr;
        ast::TypeSyntax* returnType = parseType();
        if (returnType == nullptr) return nullptr;
        function->setFunctionReturnType(returnType);

        bool sawUnsafe = false;
        bool sawNoHeap = false;
        bool sawAsync = false;
        while (true)
        {
            const bool unsafeEffect = atContextualKeyword("unsafe");
            const bool noHeapEffect = at(TokenKind::KwNoHeap);
            const bool asyncEffect = atContextualKeyword("async");
            if (!unsafeEffect && !noHeapEffect && !asyncEffect) break;
            const bool duplicate = (unsafeEffect && sawUnsafe) ||
                (noHeapEffect && sawNoHeap) || (asyncEffect && sawAsync);
            const SourceRange effectRange = consume().range();
            if (duplicate)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    effectRange,
                    "duplicate function type effect");
                continue;
            }
            sawUnsafe = sawUnsafe || unsafeEffect;
            sawNoHeap = sawNoHeap || noHeapEffect;
            sawAsync = sawAsync || asyncEffect;
        }
        function->setFunctionUnsafe(sawUnsafe);
        function->setFunctionNoHeap(sawNoHeap);
        function->setFunctionAsync(sawAsync);

        if (atContextualKeyword("throws"))
        {
            const SourceRange throwsRange = consume().range();
            if (carrier == ast::FunctionTypeCarrierSyntaxKind::Pointer &&
                convention != ast::FunctionCallingConventionSyntaxKind::NativeSharp)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    throwsRange,
                    "a foreign function pointer cannot propagate a NativeSharp typed error");
            }
            ast::TypeSyntax* errorType = parseType();
            if (errorType == nullptr) return nullptr;
            function->setFunctionErrorType(errorType);
        }
        if (function->functionIsVariadic() &&
            (carrier != ast::FunctionTypeCarrierSyntaxKind::Pointer ||
             convention == ast::FunctionCallingConventionSyntaxKind::NativeSharp))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                functionStart,
                "variadicity requires a foreign function pointer calling convention");
        }
        if (function->functionIsVariadic() &&
            convention != ast::FunctionCallingConventionSyntaxKind::C &&
            convention != ast::FunctionCallingConventionSyntaxKind::Win64)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                functionStart,
                "a variadic function pointer requires the cdecl/C or win64 calling convention on Windows x64");
        }

        const SourceRange complete = index_ == 0
            ? functionStart
            : join(
                ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None
                    ? functionStart : qualifierRange,
                tokens_[index_ - 1U].range());
        function->setRange(complete);
        return function;
    }

    if (at(TokenKind::LeftParen))
    {
        const SourceRange open = consume().range();
        ast::TypeSyntax* tuple = arena_.make<ast::TypeSyntax>(open, TokenKind::Invalid, "$tuple");
        tuple->setTuple();
        tuple->setOwnershipQualifier(ownershipQualifier);
        tuple->setLifetimeName(lifetimeName);
        if (dynamicTrait)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                dynamicRange,
                "'dyn' requires a named trait or interface type");
        }
        if (opaqueResult)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                opaqueRange,
                "'some' requires a named trait type");
        }
        if (ownershipQualifier != ast::OwnershipQualifierSyntaxKind::None)
            qualifierRange = join(qualifierRange, open);
        while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
        {
            ast::TypeSyntax* elementType = parseType();
            if (elementType == nullptr) return nullptr;
            std::string elementName;
            if (at(TokenKind::Identifier) &&
                (peek().kind() == TokenKind::Comma || peek().kind() == TokenKind::RightParen))
                elementName = std::string(consume().lexeme());
            tuple->addTupleElement(elementType, std::move(elementName));
            if (!consumeIf(TokenKind::Comma)) break;
            if (at(TokenKind::RightParen)) break;
        }
        const SourceRange close = current().range();
        if (!expect(TokenKind::RightParen, "')' after tuple type")) return nullptr;
        tuple->setRange(join(
            ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None ? open : qualifierRange,
            close));
        return tuple;
    }

    if (!lexer::isBuiltinTypeKeyword(current().kind()) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None
                ? "expected a NativeSharp type"
                : "expected a NativeSharp type after ownership qualifier");
        return nullptr;
    }

    const lexer::TokenKind baseKind = current().kind();
    SourceRange completeRange = current().range();
    std::string baseName;
    if (at(TokenKind::Identifier))
    {
        if (!parseQualifiedName(baseName, completeRange))
        {
            return nullptr;
        }
    }
    else
    {
        const lexer::Token& base = consume();
        baseName = std::string(base.lexeme());
        if (baseName == "i8") baseName = "sbyte";
        else if (baseName == "u8") baseName = "byte";
        else if (baseName == "i16") baseName = "short";
        else if (baseName == "u16") baseName = "ushort";
        else if (baseName == "i32") baseName = "int";
        else if (baseName == "u32") baseName = "uint";
        else if (baseName == "i64") baseName = "long";
        else if (baseName == "u64") baseName = "ulong";
        else if (baseName == "isize") baseName = "nint";
        else if (baseName == "usize") baseName = "nuint";
        completeRange = base.range();
    }
    if (recoverLongDoubleAsDouble) completeRange = join(longDoubleRange, completeRange);

    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        completeRange, baseKind, std::move(baseName));
    type->setOwnershipQualifier(ownershipQualifier);
    type->setLifetimeName(std::move(lifetimeName));
    type->setDynamicTrait(dynamicTrait);
    type->setOpaqueResult(opaqueResult);
    if (opaqueResult) type->setOpaqueResultTraitName(type->baseName());
    if (ownershipQualifier != ast::OwnershipQualifierSyntaxKind::None)
    {
        completeRange = join(qualifierRange, completeRange);
    }
    if (dynamicTrait) completeRange = join(dynamicRange, completeRange);
    if (opaqueResult) completeRange = join(opaqueRange, completeRange);

    if (baseKind == TokenKind::Identifier && at(TokenKind::Less))
    {
        std::vector<ast::TypeSyntax*> arguments;
        if (parseTypeArgumentList(arguments))
        {
            for (ast::TypeSyntax* argument : arguments) type->addTypeArgument(argument);
            if (index_ > 0) completeRange = join(completeRange, tokens_[index_ - 1].range());
        }
    }

    if (at(TokenKind::Ellipsis))
    {
        const SourceRange expansion = consume().range();
        type->setPackExpansion();
        completeRange = join(completeRange, expansion);
    }

    // T42 uses the unambiguous C-compatible postfix spelling for pointee constness:
    // `int const* p`. A leading `const int* p` remains the existing declaration-level const
    // and therefore makes `p`, not `*p`, immutable.
    SourceRange pointeeConstRange{};
    const bool pointeeConst = at(TokenKind::KwConst);
    if (pointeeConst)
    {
        pointeeConstRange = consume().range();
        type->setPointeeConst();
        completeRange = join(completeRange, pointeeConstRange);
    }

    while (true)
    {
        if (at(TokenKind::Star))
        {
            const lexer::Token& star = consume();
            ast::TypeModifierSyntax pointer{
                ast::TypeModifierSyntaxKind::Pointer,
                nullptr,
                star.range()};
            if (consumeIf(TokenKind::Question))
            {
                pointer.pointerNullability = ast::PointerNullabilitySyntaxKind::Nullable;
                pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
            }
            else if (consumeIf(TokenKind::Bang))
            {
                pointer.pointerNullability = ast::PointerNullabilitySyntaxKind::NonNull;
                pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
            }
            while ((atContextualKeyword("align") || atContextualKeyword("addrspace") ||
                    atContextualKeyword("provenance")) &&
                   peek().kind() == TokenKind::LeftParen)
            {
                const std::string qualifier(current().lexeme());
                const SourceRange qualifierStart = consume().range();
                consume();
                if (qualifier == "provenance")
                {
                    if (atContextualKeyword("none"))
                    {
                        pointer.pointerHasProvenance = false;
                        consume();
                    }
                    else if (atContextualKeyword("tracked"))
                    {
                        pointer.pointerHasProvenance = true;
                        consume();
                    }
                    else
                    {
                        expectContextualKeyword("tracked", "'tracked' or 'none' provenance mode");
                        if (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
                        {
                            consume();
                        }
                    }
                }
                else
                {
                    std::optional<std::uint64_t> value;
                    if (at(TokenKind::IntegerLiteral) &&
                        std::holds_alternative<std::uint64_t>(current().value()))
                    {
                        value = std::get<std::uint64_t>(current().value());
                        consume();
                    }
                    else
                    {
                        expect(TokenKind::IntegerLiteral, "pointer qualifier value");
                    }
                    if (qualifier == "align" && value.has_value())
                    {
                        if (*value == 0 || (*value & (*value - 1U)) != 0 || *value > 4096U)
                        {
                            diagnostics_.report(
                                diagnostics::DiagnosticId::InvalidType,
                                diagnostics::DiagnosticSeverity::Error,
                                qualifierStart,
                                "pointer alignment knowledge must be a power of two no larger than 4096");
                        }
                        else pointer.pointerKnownAlignment = static_cast<std::size_t>(*value);
                    }
                    else if (qualifier == "addrspace" && value.has_value())
                    {
                        if (*value > 255U)
                        {
                            diagnostics_.report(
                                diagnostics::DiagnosticId::InvalidType,
                                diagnostics::DiagnosticSeverity::Error,
                                qualifierStart,
                                "reserved pointer address space must be in range 0..255");
                        }
                        else pointer.pointerAddressSpace = static_cast<unsigned>(*value);
                    }
                }
                expect(TokenKind::RightParen, "')' after pointer qualifier");
                pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
            }
            type->addModifier(pointer);
            completeRange = join(completeRange, pointer.range);
            continue;
        }

        if (at(TokenKind::LeftBracket))
        {
            const SourceRange left = consume().range();
            // Item 273: `T[*]` is an unsized inline tail, deliberately distinct from `T[]`
            // (a native heap-array reference) and `T[N]` (a complete inline value).
            if (at(TokenKind::Star) && peek().kind() == TokenKind::RightBracket)
            {
                consume();
                const SourceRange right = consume().range();
                const SourceRange modifierRange = join(left, right);
                type->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::FlexibleArray,
                    nullptr,
                    modifierRange});
                completeRange = join(completeRange, modifierRange);
                continue;
            }
            if (at(TokenKind::RightBracket))
            {
                const SourceRange right = consume().range();
                const SourceRange modifierRange = join(left, right);
                type->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::DynamicArray,
                    nullptr,
                    modifierRange});
                completeRange = join(completeRange, modifierRange);
                continue;
            }

            // NativeSharp 3.0 T32: comma-only brackets are one rectangular heap-array modifier.
            // Rank stays in the type instead of degrading `T[,]` into jagged `T[][]`.
            if (at(TokenKind::Comma))
            {
                std::size_t rank = 1;
                while (consumeIf(TokenKind::Comma)) ++rank;
                const SourceRange beforeRight = current().range();
                expect(TokenKind::RightBracket, "']'");
                SourceRange modifierRange = join(left, beforeRight);
                if (index_ > 0 && tokens_[index_ - 1].kind() == TokenKind::RightBracket)
                    modifierRange = join(left, tokens_[index_ - 1].range());
                type->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::RangeArray, nullptr, modifierRange, rank});
                completeRange = join(completeRange, modifierRange);
                continue;
            }

            // Item 267: a multidimensional fixed array keeps the type-first NativeSharp
            // spelling (`T[rows, columns]`) while lowering to the already canonical nested-array
            // representation.  The first written dimension is the outermost one, so modifiers
            // are appended in reverse: `int[3, 4]` becomes the canonical `int[4][3]`, i.e.
            // `[3 x [4 x i32]]` in LLVM.  Keeping one representation means layout, sizeof,
            // generic substitution and the C ABI do not need a parallel "matrix" type.
            std::vector<ast::AstNode*> dimensions;
            dimensions.push_back(parseExpression());
            while (consumeIf(TokenKind::Comma))
            {
                if (at(TokenKind::RightBracket))
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedExpression,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "fixed multidimensional array requires an expression after ','");
                    break;
                }
                dimensions.push_back(parseExpression());
            }
            const SourceRange beforeRight = current().range();
            expect(TokenKind::RightBracket, "']'");
            SourceRange modifierRange = join(left, beforeRight);
            if (index_ > 0 && tokens_[index_ - 1].kind() == TokenKind::RightBracket)
            {
                modifierRange = join(left, tokens_[index_ - 1].range());
            }
            std::vector<ast::TypeModifierSyntax> arrayModifiers;
            arrayModifiers.reserve(dimensions.size());
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
            {
                arrayModifiers.push_back(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Array,
                    *dimension,
                    modifierRange});
            }
            type->addArrayModifierGroup(arrayModifiers);
            completeRange = join(completeRange, modifierRange);
            continue;
        }

        break;
    }

    if (pointeeConst && std::ranges::none_of(
            type->modifiers(), [](const ast::TypeModifierSyntax& modifier)
            {
                return modifier.kind == ast::TypeModifierSyntaxKind::Pointer;
            }))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            pointeeConstRange,
            "postfix const qualifies a pointer pointee and therefore requires '*' in the type");
    }

    type->setRange(completeRange);
    return type;
}

std::string Parser::parseOptionalLifetimeAnnotation(std::string_view context)
{
    if (!consumeIf(TokenKind::LeftBracket)) return {};
    std::string result;
    if (at(TokenKind::Identifier) || at(TokenKind::KwThis) || at(TokenKind::KwStatic))
        result = std::string(consume().lexeme());
    else
        expect(TokenKind::Identifier, ("lifetime name in " + std::string(context)).c_str());
    expect(TokenKind::RightBracket, "']' after lifetime name");
    return result;
}

ast::TypeSyntax* Parser::parseAllocationElementType()
{
    if (!lexer::isBuiltinTypeKeyword(current().kind()) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected allocation element type");
        return nullptr;
    }

    const lexer::TokenKind baseKind = current().kind();
    SourceRange completeRange = current().range();
    std::string baseName;
    if (at(TokenKind::Identifier))
    {
        if (!parseQualifiedName(baseName, completeRange))
        {
            return nullptr;
        }
    }
    else
    {
        const lexer::Token& base = consume();
        baseName = std::string(base.lexeme());
        completeRange = base.range();
    }

    if (baseKind == TokenKind::KwVoid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            completeRange,
            "void cannot be used as an allocation element type");
    }

    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        completeRange, baseKind, std::move(baseName));

    if (baseKind == TokenKind::Identifier && at(TokenKind::Less))
    {
        std::vector<ast::TypeSyntax*> arguments;
        if (parseTypeArgumentList(arguments))
        {
            for (ast::TypeSyntax* argument : arguments) type->addTypeArgument(argument);
            if (index_ > 0) completeRange = join(completeRange, tokens_[index_ - 1].range());
        }
    }

    while (at(TokenKind::Star))
    {
        const lexer::Token& star = consume();
        type->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Pointer,
            nullptr,
            star.range()});
        completeRange = join(completeRange, star.range());
    }

    type->setRange(completeRange);
    return type;
}

bool Parser::parseGenericParameterList(ast::AstNode* declaration)
{
    if (declaration == nullptr || !consumeIf(TokenKind::Less)) return true;
    std::unordered_set<std::string> names;
    while (true)
    {
        ast::GenericParameterSyntax syntax;
        const std::size_t parameterBegin = index_;
        const bool lifetimeParameter = atContextualKeyword("lifetime");
        if (lifetimeParameter)
        {
            consume();
        }
        else if (at(TokenKind::KwConst))
        {
            consume();
            syntax.kind = ast::GenericParameterSyntaxKind::Constant;
            syntax.constantType = parseType();
            if (syntax.constantType == nullptr) return false;
            const TokenKind kind = syntax.constantType->baseTokenKind();
            const bool scalarSupported = syntax.constantType->typeArguments().empty() &&
                (kind == TokenKind::KwBool || kind == TokenKind::KwByte ||
                 kind == TokenKind::KwSByte || kind == TokenKind::KwShort ||
                 kind == TokenKind::KwUShort || kind == TokenKind::KwInt ||
                 kind == TokenKind::KwUInt || kind == TokenKind::KwLong ||
                 kind == TokenKind::KwULong || kind == TokenKind::KwNInt ||
                 kind == TokenKind::KwNUInt || kind == TokenKind::KwChar ||
                 kind == TokenKind::Identifier);
            const bool supported = syntax.constantType->modifiers().empty() &&
                (scalarSupported || syntax.constantType->isTuple());
            if (!supported)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax.constantType->range(),
                    "const generic parameters require bool, char, an integral builtin type, "
                    "an enum, or an approved structural tuple type");
            }
        }
        else if (atContextualKeyword("pack"))
        {
            consume();
            if (consumeIf(TokenKind::KwConst))
            {
                syntax.kind = ast::GenericParameterSyntaxKind::ConstantPack;
                syntax.constantType = parseType();
                if (syntax.constantType == nullptr) return false;
                const TokenKind kind = syntax.constantType->baseTokenKind();
                const bool scalarSupported = syntax.constantType->typeArguments().empty() &&
                    (kind == TokenKind::KwBool || kind == TokenKind::KwByte ||
                     kind == TokenKind::KwSByte || kind == TokenKind::KwShort ||
                     kind == TokenKind::KwUShort || kind == TokenKind::KwInt ||
                     kind == TokenKind::KwUInt || kind == TokenKind::KwLong ||
                     kind == TokenKind::KwULong || kind == TokenKind::KwNInt ||
                     kind == TokenKind::KwNUInt || kind == TokenKind::KwChar ||
                     kind == TokenKind::Identifier);
                const bool supported = syntax.constantType->modifiers().empty() &&
                    (scalarSupported || syntax.constantType->isTuple());
                if (!supported)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        syntax.constantType->range(),
                        "const generic packs require bool, char, an integral builtin type, "
                        "an enum, or an approved structural tuple type");
                }
            }
            else
            {
                syntax.kind = ast::GenericParameterSyntaxKind::TypePack;
            }
        }
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "generic parameter name");
            return false;
        }
        const lexer::Token& parameter = consume();
        const std::string name(parameter.lexeme());
        if (!names.insert(name).second)
        {
            const std::string parameterKind =
                lifetimeParameter
                ? "lifetime parameter"
                : syntax.kind == ast::GenericParameterSyntaxKind::Constant ||
                    syntax.kind == ast::GenericParameterSyntaxKind::ConstantPack
                ? "const generic parameter"
                : "generic type parameter";
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
        if (!consumeIf(TokenKind::Comma)) break;
    }
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
    return expect(TokenKind::Greater, "'>' after generic type parameter list");
}

// NSC-LIMIT-007. `where T : class, Base, IFoo`.
//
// QUE SE ADMITE EN ESTA ENTREGA: `class`, una clase base y una o mas interfaces, en cualquier
// combinacion, con `class` obligatoriamente el PRIMERO cuando aparece -- que es la regla de C#.
// Cual de los tipos escritos es la base y cual la interfaz NO se decide aqui: eso exige saber que
// declara cada nombre, y el name lookup no existe todavia cuando el parser corre.
//
// QUE NO: `struct`, `new()`, `unmanaged` y `notnull`. Se RECHAZAN CON UN DIAGNOSTICO PROPIO, no
// con un parseo parcial que los ignore. Un constraint aceptado y no verificado es peor que uno
// rechazado: promete algo que nadie comprueba, y el error acaba saliendo dentro del cuerpo
// sustituido, que es el muro de errores de plantillas que este ID existe para quitar.
//
// LO QUE SE COMPRUEBA AQUI Y NO MAS TARDE es todo lo que se puede saber sin resolver tipos: que
// el parametro exista, que no haya dos clausulas para el mismo, que no se repita un constraint, y
// que `class` no venga en segunda posicion. Son errores de la DECLARACION, y dejarlos para la
// validacion semantica los convertiria en errores del sitio de instanciacion -- lejos de la causa.
bool Parser::parseGenericConstraintClauses(ast::AstNode* declaration)
{
    if (declaration == nullptr) return true;
    bool ok = true;
    std::unordered_set<std::string> constrained;
    while (atContextualKeyword("where"))
    {
        const std::size_t clauseBegin = index_;
        consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "generic type parameter after 'where'");
            return false;
        }
        const lexer::Token& parameter = consume();
        ast::GenericConstraintClause clause;
        const std::string rootParameter(parameter.lexeme());
        clause.parameterName = rootParameter;
        clause.parameterRange = parameter.range();
        while (consumeIf(TokenKind::Dot))
        {
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "associated item name after '.'");
                return false;
            }
            const lexer::Token& item = consume();
            clause.parameterName += '.';
            clause.parameterName += item.lexeme();
            clause.parameterRange = join(clause.parameterRange, item.range());
        }
        const bool associatedEquality = clause.parameterName != rootParameter;
        if (associatedEquality)
        {
            clause.associatedProjection = arena_.make<ast::TypeSyntax>(
                clause.parameterRange, TokenKind::Identifier, clause.parameterName);
        }

        const std::vector<std::string>& declared = declaration->genericParameters();
        const bool knownGeneric =
            std::find(declared.begin(), declared.end(), rootParameter) != declared.end();
        const auto lifetimeSyntax = std::ranges::find_if(
            declaration->lifetimeParameters(),
            [&](const ast::LifetimeParameterSyntax& item)
            {
                return item.name == rootParameter;
            });
        const bool knownLifetime = lifetimeSyntax != declaration->lifetimeParameters().end();
        const bool known = knownGeneric || knownLifetime;
        const auto syntax = std::ranges::find_if(
            declaration->genericParameterSyntaxes(),
            [&](const ast::GenericParameterSyntax& item)
            {
                return item.name == rootParameter;
            });
        if (!known)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::UnknownIdentifier,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range(),
                    "'" + rootParameter +
                    "' is not a generic or lifetime parameter of this declaration");
            ok = false;
        }
        else if (!constrained.insert(clause.parameterName).second)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range(),
                "duplicate 'where' clause for generic type parameter '" +
                    clause.parameterName + "'");
            ok = false;
        }

        if (knownLifetime)
        {
            if (associatedEquality)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    clause.parameterRange,
                    "a lifetime parameter cannot name an associated item");
                synchronizeGlobal();
                return false;
            }
            if (!expect(TokenKind::Colon, "':' after the constrained lifetime parameter"))
                return false;
            std::unordered_set<std::string> written;
            while (true)
            {
                if (!at(TokenKind::Identifier) && !at(TokenKind::KwStatic))
                {
                    expect(TokenKind::Identifier, "lifetime name after ':'");
                    return false;
                }
                const lexer::Token& shorter = consume();
                const std::string shorterName(shorter.lexeme());
                const bool isStatic = shorterName == "static";
                const bool declaredLifetime = std::ranges::any_of(
                    declaration->lifetimeParameters(),
                    [&](const ast::LifetimeParameterSyntax& item)
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
                if (shorterName == rootParameter)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        shorter.range(),
                        "lifetime outlives constraint for '" + rootParameter + "' is tautological");
                    ok = false;
                }
                if (!written.insert(shorterName).second)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        shorter.range(),
                        "duplicate outlives constraint '" + rootParameter + ":" + shorterName + "'");
                    ok = false;
                }
                if ((isStatic || declaredLifetime) && shorterName != rootParameter)
                {
                    declaration->addLifetimeOutlivesConstraint(ast::LifetimeOutlivesConstraint{
                        rootParameter, shorterName, join(parameter.range(), shorter.range())});
                }
                if (!consumeIf(TokenKind::Comma)) break;
            }
            continue;
        }

        if (consumeIf(TokenKind::EqualEqual))
        {
            const bool parameterIsConstant =
                syntax != declaration->genericParameterSyntaxes().end() &&
                syntax->kind == ast::GenericParameterSyntaxKind::Constant;
            if (syntax != declaration->genericParameterSyntaxes().end() &&
                (syntax->kind == ast::GenericParameterSyntaxKind::TypePack ||
                 syntax->kind == ast::GenericParameterSyntaxKind::ConstantPack))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter.range(),
                    "a generic pack cannot be the left side of an equality constraint");
                ok = false;
            }

            if (at(TokenKind::IntegerLiteral))
            {
                const lexer::Token& literal = consume();
                const auto* value = std::get_if<std::uint64_t>(&literal.value());
                const std::uint64_t normalized = value == nullptr ? 0 : *value;
                const std::string typeKey = parameterIsConstant && syntax->constantType != nullptr
                    ? "builtin:" + syntax->constantType->baseName()
                    : std::string(frontend::builtins::integerLiteralTypeKey(
                          literal.lexeme(), normalized, false));
                clause.equalityConstraint = arena_.make<ast::TypeSyntax>(
                    literal.range(), TokenKind::Invalid,
                    language::constGenericArgumentMarker(typeKey, std::to_string(normalized)));
            }
            else if (at(TokenKind::KwTrue) || at(TokenKind::KwFalse))
            {
                const lexer::Token& literal = consume();
                clause.equalityConstraint = arena_.make<ast::TypeSyntax>(
                    literal.range(), TokenKind::Invalid,
                    language::constGenericArgumentMarker(
                        "builtin:bool", literal.kind() == TokenKind::KwTrue ? "true" : "false"));
            }
            else
            {
                clause.equalityConstraint = parseType();
                if (clause.equalityConstraint == nullptr) return false;
            }

            std::string equalityConstantType;
            std::string equalityConstantValue;
            const bool equalityIsConstant = language::parseConstGenericArgumentMarker(
                clause.equalityConstraint->baseName(), equalityConstantType, equalityConstantValue);
            if (parameterIsConstant != equalityIsConstant)
            {
                const auto otherParameter = std::ranges::find_if(
                    declaration->genericParameterSyntaxes(),
                    [&](const ast::GenericParameterSyntax& item)
                    {
                        return item.name == clause.equalityConstraint->baseName();
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
                        clause.equalityConstraint->range(),
                        parameterIsConstant
                            ? "a const generic equality requires another const parameter or a compile-time value"
                            : "a type generic equality requires another type parameter or a type");
                    ok = false;
                }
            }
            if (clause.equalityConstraint->baseName() == clause.parameterName)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    clause.equalityConstraint->range(),
                    "generic equality constraint for '" + clause.parameterName +
                        "' is tautological");
                ok = false;
            }
            clause.range = tokenRangeFrom(clauseBegin);
            if (known) declaration->addGenericConstraint(std::move(clause));
            continue;
        }

        if (associatedEquality)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                clause.parameterRange,
                "an associated type projection constraint requires '=='");
            synchronizeGlobal();
            return false;
        }
        if (syntax != declaration->genericParameterSyntaxes().end() &&
            syntax->kind != ast::GenericParameterSyntaxKind::Type)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range(),
                "a capability or trait 'where' clause applies only to an ordinary generic type parameter; '" +
                    clause.parameterName + "' is a const generic or type pack");
            ok = false;
        }
        if (!expect(TokenKind::Colon, "':' or '==' after the constrained generic parameter"))
            return false;

        std::unordered_set<std::string> written;
        bool firstConstraint = true;
        while (true)
        {
            const bool isKeywordClass = at(TokenKind::KwClass);
            const bool isKeywordStruct = at(TokenKind::KwStruct);
            const bool isKeywordNew = at(TokenKind::KwNew);
            const bool isCopy = atContextualKeyword("copy");
            const bool isMove = at(TokenKind::KwMove) || atContextualKeyword("move");
            const bool isLifetime = atContextualKeyword("lifetime");
            const bool isUnmanaged = atContextualKeyword("unmanaged");
            const bool isNotNull = atContextualKeyword("notnull");
            if (isKeywordClass || isKeywordStruct || isKeywordNew || isCopy || isMove ||
                isLifetime || isUnmanaged || isNotNull)
            {
                const lexer::Token& keyword = consume();
                const bool conflictsWithEarlierPrimary =
                    (isKeywordClass && (clause.requiresValueType || clause.requiresUnmanaged)) ||
                    (isKeywordStruct && clause.requiresClass);
                if ((isKeywordClass || isKeywordStruct) && !firstConstraint &&
                    !conflictsWithEarlierPrimary)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        keyword.range(),
                        isKeywordClass
                            ? "'class' must be the first constraint of its 'where' clause"
                            : "primary generic constraint '" + std::string(keyword.lexeme()) +
                                  "' must be first in its 'where' clause");
                    ok = false;
                }
                bool* flag = nullptr;
                std::string_view spelling;
                if (isKeywordClass) { flag = &clause.requiresClass; spelling = "class"; }
                else if (isKeywordStruct) { flag = &clause.requiresValueType; spelling = "struct"; }
                else if (isKeywordNew)
                {
                    flag = &clause.requiresDefaultConstructor;
                    spelling = "new()";
                    if (!expect(TokenKind::LeftParen, "'(' after new in a generic constraint") ||
                        !expect(TokenKind::RightParen, "')' after new( in a generic constraint"))
                        return false;
                }
                else if (isCopy) { flag = &clause.requiresCopy; spelling = "copy"; }
                else if (isMove) { flag = &clause.requiresMove; spelling = "move"; }
                else if (isLifetime) { flag = &clause.requiresTrackedLifetime; spelling = "lifetime"; }
                else if (isUnmanaged) { flag = &clause.requiresUnmanaged; spelling = "unmanaged"; }
                else { flag = &clause.requiresNotNull; spelling = "notnull"; }
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
            }
            else
            {
                ast::TypeSyntax* constraint = parseType();
                if (constraint == nullptr) return false;
                if (std::find(declared.begin(), declared.end(), std::string(constraint->baseName())) !=
                    declared.end())
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        constraint->range(),
                        "'" + std::string(constraint->baseName()) +
                            "' is a generic type parameter of this declaration and cannot be used "
                            "as a constraint; use 'where " + clause.parameterName +
                            " == " + std::string(constraint->baseName()) +
                            "' for equality instead of a trait/base constraint");
                    ok = false;
                }
                // El texto escrito es lo unico comparable sin resolver nombres. Dos escrituras
                // distintas del mismo tipo se le escapan a esta comprobacion, y esa la coge la
                // validacion semantica -- que si sabe a que resuelve cada una.
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
            }
            firstConstraint = false;
            if (!consumeIf(TokenKind::Comma)) break;
        }

        clause.range = tokenRangeFrom(clauseBegin);
        if (known) declaration->addGenericConstraint(std::move(clause));
    }
    return ok;
}

bool Parser::parseTypeArgumentList(std::vector<ast::TypeSyntax*>& arguments)
{
    if (!consumeIf(TokenKind::Less)) return false;
    if (consumeGenericGreater()) return true;
    while (true)
    {
        bool tupleTypeArgument = false;
        if (at(TokenKind::LeftParen))
        {
            std::size_t afterType = index_;
            if (scanTypeAt(index_, afterType) && afterType < tokens_.size())
            {
                const TokenKind following = tokens_[afterType].kind();
                tupleTypeArgument = following == TokenKind::Comma ||
                    following == TokenKind::Greater || following == TokenKind::GreaterGreater;
            }
        }
        if (tupleTypeArgument)
        {
            ast::TypeSyntax* argument = parseType();
            if (argument != nullptr) arguments.push_back(argument);
            if (!consumeIf(TokenKind::Comma)) break;
            continue;
        }
        if (at(TokenKind::LeftParen) || at(TokenKind::Plus) || at(TokenKind::Minus) ||
            at(TokenKind::Tilde) || at(TokenKind::Bang))
        {
            const SourceRange begin = current().range();
            ast::AstNode* expression = at(TokenKind::LeftParen)
                ? parsePrimaryExpression() : parseUnaryExpression();
            ast::TypeSyntax* argument = arena_.make<ast::TypeSyntax>(
                expression != nullptr ? expression->range() : begin,
                TokenKind::Invalid,
                "$nsc_const_expression$");
            argument->setConstExpression(expression);
            arguments.push_back(argument);
            if (!consumeIf(TokenKind::Comma)) break;
            continue;
        }
        if (at(TokenKind::IntegerLiteral))
        {
            const lexer::Token& literal = consume();
            const auto* value = std::get_if<std::uint64_t>(&literal.value());
            const std::uint64_t normalized = value == nullptr ? 0 : *value;
            const std::string typeKey(frontend::builtins::integerLiteralTypeKey(
                literal.lexeme(), normalized, false));
            arguments.push_back(arena_.make<ast::TypeSyntax>(
                literal.range(), TokenKind::Invalid,
                language::constGenericArgumentMarker(typeKey, std::to_string(normalized))));
            if (!consumeIf(TokenKind::Comma)) break;
            continue;
        }
        if (at(TokenKind::KwTrue) || at(TokenKind::KwFalse))
        {
            const lexer::Token& literal = consume();
            arguments.push_back(arena_.make<ast::TypeSyntax>(
                literal.range(), TokenKind::Invalid,
                language::constGenericArgumentMarker(
                    "builtin:bool", literal.kind() == TokenKind::KwTrue ? "true" : "false")));
            if (!consumeIf(TokenKind::Comma)) break;
            continue;
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
        ast::TypeSyntax* argument = parseType();
        if (argument != nullptr) arguments.push_back(argument);
        if (!consumeIf(TokenKind::Comma)) break;
    }
    if (!consumeGenericGreater())
    {
        expect(TokenKind::Greater, "'>' after generic type argument list");
        return false;
    }
    return true;
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

// Un token que puede aparecer DENTRO de una lista de argumentos de tipo, y nada mas.
//
// EL DEFECTO QUE CIERRA, medido con un arnes que compila el lexer y el parser y les da la linea:
//
//     if (wide < (long)Min() || wide > (long)Max())
//
// no parseaba. `looksLikeGenericCallTypeArguments` avanzaba buscando el `>` que cierra, y lo
// encontraba... pero era el `>` de la SEGUNDA comparacion. Con la profundidad en cero miraba el
// token siguiente, que era el `(` de `(long)`, y contestaba "si, son argumentos genericos". A
// partir de ahi `parseTypeArgumentList` exigia un tipo, encontraba `(` y reportaba
// "generic type argument list requires a type" sin retroceder.
//
// La parada anterior era `;`, `=` y fin de fichero: tres tokens. Cualquier expresion con dos
// comparaciones y un parentesis entremedias entraba en la trampa.
//
// LA REGLA CORRECTA ES UNA LISTA BLANCA, no una lista de paradas. Lo que `parseType` puede
// consumir es un conjunto CERRADO: nombres, tipos primitivos, cualificadores de propiedad, el
// punto de un nombre calificado, la coma que separa argumentos, el `*` de un puntero, los
// corchetes de un arreglo con su contador, y los `<`/`>` del anidamiento. NativeSharp 3 agrega
// valores const: sus expresiones compuestas se escriben entre parentesis, de modo que el
// lookahead puede admitir operadores dentro de esos parentesis sin volver a confundir una
// comparacion ordinaria con una llamada generica.
//
// Enumerar lo que SI puede aparecer es lo que hace que el caso siguiente que nadie previo salga
// bien por omision. Una lista de paradas hay que ampliarla cada vez que aparece un operador nuevo;
// esta no.
bool Parser::canAppearInTypeArgumentList(TokenKind kind) noexcept
{
    if (lexer::isBuiltinTypeKeyword(kind) || lexer::isOwnershipQualifierKeyword(kind))
    {
        return true;
    }
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
        // El contador de un arreglo fijo: `Foo<float[4]>(x)`.
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

bool Parser::looksLikeGenericCallTypeArguments() const noexcept
{
    if (!at(TokenKind::Less)) return false;
    std::size_t cursor = index_ + 1;
    int genericDepth = 1;
    int parenthesisDepth = 0;
    bool expectsArgument = true;
    bool sawArgument = false;
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!canAppearInTypeArgumentList(kind))
        {
            return false;
        }
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
        const bool functionReferenceCarrier = kind == TokenKind::Amp && cursor > 0 &&
            tokens_[cursor - 1U].kind() == TokenKind::Identifier &&
            tokens_[cursor - 1U].lexeme() == "fn";
        if (parenthesisDepth == 0 &&
            (kind == TokenKind::AmpAmp || kind == TokenKind::PipePipe ||
             kind == TokenKind::EqualEqual || kind == TokenKind::BangEqual ||
             kind == TokenKind::LessEqual || kind == TokenKind::GreaterEqual ||
             kind == TokenKind::Slash || kind == TokenKind::Percent ||
             (kind == TokenKind::Amp && !functionReferenceCarrier) || kind == TokenKind::Pipe ||
             kind == TokenKind::Caret || kind == TokenKind::LessLess))
            return false;
        if (parenthesisDepth == 0 &&
            (kind == TokenKind::Plus || kind == TokenKind::Minus ||
             kind == TokenKind::Bang || kind == TokenKind::Tilde))
        {
            const TokenKind previous = tokens_[cursor - 1U].kind();
            if (previous != TokenKind::Less && previous != TokenKind::Comma) return false;
        }
        if (kind == TokenKind::LeftParen) ++parenthesisDepth;
        else if (kind == TokenKind::RightParen)
        {
            if (--parenthesisDepth < 0) return false;
        }
        else if (parenthesisDepth == 0 && kind == TokenKind::Less) ++genericDepth;
        else if (parenthesisDepth == 0 && genericDepth == 1 && kind == TokenKind::Comma)
        {
            expectsArgument = true;
        }
        else if (parenthesisDepth == 0 &&
                 (kind == TokenKind::Greater || kind == TokenKind::GreaterGreater))
        {
            genericDepth -= kind == TokenKind::GreaterGreater ? 2 : 1;
            if (genericDepth == 0)
            {
                if (expectsArgument && sawArgument) return false;
                return cursor + 1 < tokens_.size() && tokens_[cursor + 1].kind() == TokenKind::LeftParen;
            }
            if (genericDepth < 0) return false;
        }
        ++cursor;
    }
    return false;
}

// ID 131. `Caja<int>.Make(7)` -- un miembro estatico de un tipo generico CERRADO -- no parseaba.
//
// EL DEFECTO. `looksLikeGenericCallTypeArguments` exige que tras el `>` que cierra venga un `(`.
// Con esa regla `Helper.Wrap<int>(1)` --argumentos de tipo de un METODO-- entra, y
// `Caja<int>.Make(7)` --argumentos de tipo de un TIPO-- no: detras del `>` viene un `.`. El
// parser lo leia entonces como dos comparaciones y moria con "expected expression" dos veces
// seguidas de "expected ';', found 'Make'", ninguna de las cuales menciona genericos.
//
// POR QUE UN PREDICADO APARTE Y NO UN TOKEN MAS EN EL OTRO. Porque no producen lo mismo. Tras
// `Wrap<int>` viene una LLAMADA y los argumentos de tipo se cuelgan de la CallExpression; tras
// `Caja<int>` viene un TIPO CERRADO del que se accede a un miembro, y lo que hace falta no es una
// lista de argumentos suelta sino un TypeSyntax completo -- el mismo que `new Caja<int>()` y
// `Caja<int> c` ya construyen. Mezclar los dos casos en un predicado habria obligado a que el
// bucle de sufijos adivinara cual de las dos formas tenia delante.
//
// LA REGLA DE DESAMBIGUACION es la de C#: se intenta leer una lista de argumentos de tipo y solo
// se acepta si lo que sigue al `>` de cierre encaja. Aqui el conjunto es UNO -- `.` seguido de un
// identificador -- porque es la unica forma nueva que se admite. Se reutiliza
// `canAppearInTypeArgumentList`, que es la lista blanca que ya evita comprometerse con una
// expresion que solo tenia comparaciones dentro.
bool Parser::looksLikeClosedGenericTypeReference() const noexcept
{
    if (!at(TokenKind::Identifier)) return false;
    std::size_t cursor = index_ + 1;

    // Artifact metadata is allowed to preserve the owning module as
    // `Module::Namespace.Type<T>.Member`.  The ordinary type parser already accepts that
    // spelling; the expression disambiguator must skip the same prefix before looking for the
    // generic argument list.  Without this, metadata written by NSC itself was rejected at the
    // first ':' when a closed generic type headed a static call.
    if (cursor + 2 < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Colon &&
        tokens_[cursor + 1].kind() == TokenKind::Colon &&
        tokens_[cursor + 2].kind() == TokenKind::Identifier)
    {
        cursor += 3;
    }

    // Nombre calificado: `Espacio.Caja<int>.Make(...)`.
    while (cursor + 1 < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Dot &&
           tokens_[cursor + 1].kind() == TokenKind::Identifier)
    {
        cursor += 2;
    }
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Less) return false;

    ++cursor;
    int genericDepth = 1;
    int parenthesisDepth = 0;
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!canAppearInTypeArgumentList(kind))
        {
            return false;
        }
        const bool functionReferenceCarrier = kind == TokenKind::Amp && cursor > 0 &&
            tokens_[cursor - 1U].kind() == TokenKind::Identifier &&
            tokens_[cursor - 1U].lexeme() == "fn";
        if (parenthesisDepth == 0 &&
            (kind == TokenKind::AmpAmp || kind == TokenKind::PipePipe ||
             kind == TokenKind::EqualEqual || kind == TokenKind::BangEqual ||
             kind == TokenKind::LessEqual || kind == TokenKind::GreaterEqual ||
             kind == TokenKind::Slash || kind == TokenKind::Percent ||
             (kind == TokenKind::Amp && !functionReferenceCarrier) || kind == TokenKind::Pipe ||
             kind == TokenKind::Caret || kind == TokenKind::LessLess))
            return false;
        if (parenthesisDepth == 0 &&
            (kind == TokenKind::Plus || kind == TokenKind::Minus ||
             kind == TokenKind::Bang || kind == TokenKind::Tilde))
        {
            const TokenKind previous = tokens_[cursor - 1U].kind();
            if (previous != TokenKind::Less && previous != TokenKind::Comma) return false;
        }
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
            {
                return cursor + 2 < tokens_.size() &&
                    tokens_[cursor + 1].kind() == TokenKind::Dot &&
                    tokens_[cursor + 2].kind() == TokenKind::Identifier;
            }
            if (genericDepth < 0) return false;
        }
        ++cursor;
    }
    return false;
}

ast::AstNode* Parser::parseNamespaceMember()
{
    // NS304-T05-F01: `async` se consume como modifier (ya no se rechaza con NS3049) y se guarda
    // como marcador "async" en el auxiliaryText del callable global; el lowering es T05-F03.
    bool isAsync = false;
    if (atContextualKeyword("async"))
    {
        isAsync = true;
        consume();
    }
    if (atContextualKeyword("extension"))
    {
        return parseExtensionDeclaration();
    }
    if (atContextualKeyword("import"))
    {
        return parseImportDirective();
    }
    if (at(TokenKind::KwExport) &&
        peek().kind() == TokenKind::Identifier && peek().lexeme() == "import")
    {
        consume();
        return parseImportDirective(true);
    }
    if (at(TokenKind::KwUsing))
    {
        return parseUsingDirective();
    }
    if (at(TokenKind::KwNamespace))
    {
        return parseNamespaceDeclaration();
    }
    ast::AstNode* declaration = parseGlobalDeclaration();
    if (isAsync && declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::FunctionDeclaration)
    {
        std::string marker = declaration->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "async";
        declaration->setAuxiliaryText(std::move(marker));
    }
    return declaration;
}

ast::AstNode* Parser::parseImportDirective(bool reexport)
{
    const std::size_t beginIndex = index_;
    consume(); // contextual `import`

    enum class ExplicitImportKind : std::uint64_t
    {
        Module = 1,
        Symbol = 2,
        Trait = 3,
        Extensions = 4,
        RestrictedGlob = 5
    };

    ExplicitImportKind kind = ExplicitImportKind::Module;
    if (atContextualKeyword("module"))
    {
        consume();
    }
    else if (atContextualKeyword("symbol"))
    {
        kind = ExplicitImportKind::Symbol;
        consume();
    }
    else if (atContextualKeyword("trait"))
    {
        kind = ExplicitImportKind::Trait;
        consume();
    }
    else if (atContextualKeyword("extensions"))
    {
        kind = ExplicitImportKind::Extensions;
        consume();
    }

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "module, symbol or trait import target");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token first = current();
    lexer::Token last = first;
    std::string target(first.lexeme());
    consume();
    while (at(TokenKind::Dot) && peek().kind() == TokenKind::Identifier)
    {
        consume();
        last = current();
        target.push_back('.');
        target.append(current().lexeme());
        consume();
    }

    if (at(TokenKind::Dot) && peek().kind() == TokenKind::Star)
    {
        consume();
        last = consume();
        kind = ExplicitImportKind::RestrictedGlob;
    }

    std::string alias;
    if (at(TokenKind::KwAs) || atContextualKeyword("as"))
    {
        consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "local import alias");
            synchronizeGlobal();
            return nullptr;
        }
        alias = std::string(current().lexeme());
        last = consume();
    }

    if ((kind == ExplicitImportKind::Extensions || kind == ExplicitImportKind::RestrictedGlob) &&
        !alias.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidUsingTarget,
            diagnostics::DiagnosticSeverity::Error,
            tokenRangeFrom(beginIndex),
            "extension and restricted-glob imports cannot declare an alias");
    }

    expect(TokenKind::Semicolon, "';'");
    const SourceRange range = tokenRangeFrom(beginIndex);
    const bool importsSingleName = kind == ExplicitImportKind::Symbol ||
        kind == ExplicitImportKind::Trait || !alias.empty();
    if (!importsSingleName)
    {
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UsingNamespaceDirective, range);
        node->setText(std::move(target));
        node->setIntegerValue(static_cast<std::uint64_t>(kind));
        node->setFlag(reexport);
        return node;
    }

    if (alias.empty())
    {
        const std::size_t dot = target.rfind('.');
        alias = dot == std::string::npos ? target : target.substr(dot + 1);
    }
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingAliasDirective, range);
    node->setText(std::move(alias));
    node->setAuxiliaryText(target);
    node->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        join(first.range(), last.range()), TokenKind::Identifier, std::move(target)));
    node->setIntegerValue(static_cast<std::uint64_t>(kind));
    node->setFlag(reexport);
    return node;
}

ast::AstNode* Parser::parseExtensionDeclaration()
{
    const std::size_t beginIndex = index_;
    const SourceRange keywordRange = consume().range();

    TokenKind receiverPass = TokenKind::Invalid;
    if (atContextualKeyword("ref"))
    {
        receiverPass = consume().kind();
    }
    else if (at(TokenKind::KwIn))
    {
        receiverPass = consume().kind();
    }

    ast::TypeSyntax* receiverType = parseType();
    if (receiverType == nullptr)
    {
        synchronizeGlobal();
        return nullptr;
    }

    ast::AstNode* extension = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExtensionDeclaration, keywordRange);
    extension->setTypeSyntax(receiverType);
    extension->setAuxiliaryText(
        receiverPass == TokenKind::Invalid ? "value" :
        receiverPass == TokenKind::KwIn ? "in" : "ref");

    if (!expect(TokenKind::LeftBrace, "'{' after extension receiver type"))
    {
        synchronizeGlobal();
        extension->setRange(tokenRangeFrom(beginIndex));
        return extension;
    }

    const bool previousExtensionContext = parsingExtensionBody_;
    parsingExtensionBody_ = true;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member = parseGlobalDeclaration();
        const auto makeReceiver = [&]() -> ast::AstNode*
        {
            ast::AstNode* receiver = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, receiverType->range());
            receiver->setText("__nsc_extension_this");
            ast::TypeSyntax* parameterType = cloneParsedType(arena_, receiverType);
            if (receiverPass != TokenKind::Invalid)
            {
                parameterType->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, parameterType->range()});
                receiver->setAuxiliaryText(
                    receiverPass == TokenKind::KwIn ? "pass:in" : "pass:ref");
            }
            receiver->setTypeSyntax(parameterType);
            return receiver;
        };
        if (member != nullptr && member->kind() == ast::AstNodeKind::FunctionDeclaration)
        {
            member->prependChild(makeReceiver());
            member->setAuxiliaryText("ns3-extension");
            extension->addChild(member);
        }
        else if (member != nullptr &&
                 member->kind() == ast::AstNodeKind::PropertyDeclaration)
        {
            // NS304-T08-F02: baja la property a sus accessors (functions top-level). El getter
            // toma el nombre de la property; el setter conserva el prefijo `set_`.
            const std::string propertyName = member->text();
            for (ast::AstNode* accessor : member->children())
            {
                if (accessor == nullptr ||
                    accessor->kind() != ast::AstNodeKind::MethodDeclaration)
                {
                    continue;
                }
                const bool isGetter = accessor->text() == "get_" + propertyName;
                const bool isSetter = accessor->text() == "set_" + propertyName;
                if (!isGetter && !isSetter) continue;
                ast::AstNode* function = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::FunctionDeclaration, accessor->range());
                function->setText(isGetter ? propertyName : accessor->text());
                function->setTypeSyntax(cloneParsedType(arena_, accessor->typeSyntax()));
                function->setFirst(accessor->first());
                function->setTokenKind(accessor->tokenKind());
                function->prependChild(makeReceiver());
                for (ast::AstNode* parameter : accessor->children())
                {
                    function->addChild(cloneParsedNode(arena_, parameter));
                }
                function->setAuxiliaryText("ns3-extension");
                extension->addChild(function);
            }
        }
        else if (member != nullptr)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                member->range(),
                "an extension block may contain only namespace-function declarations");
        }
        if (index_ == before) consume();
    }
    parsingExtensionBody_ = previousExtensionContext;
    expect(TokenKind::RightBrace, "'}' after extension block");
    extension->setRange(tokenRangeFrom(beginIndex));
    return extension;
}

ast::AstNode* Parser::parseNamespaceDeclaration()
{
    const std::size_t beginIndex = index_;
    consume();

    std::string namespaceName;
    SourceRange nameRange = current().range();
    if (!parseQualifiedName(namespaceName, nameRange))
    {
        synchronizeGlobal();
        return nullptr;
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NamespaceDeclaration,
        join(tokens_[beginIndex].range(), nameRange));
    node->setText(std::move(namespaceName));

    if (consumeIf(TokenKind::Semicolon))
    {
        node->setFlag(true);
        while (!at(TokenKind::EndOfFile))
        {
            const std::size_t before = index_;
            ast::AstNode* member = parseNamespaceMember();
            if (member != nullptr)
            {
                node->addChild(member);
            }
            if (index_ == before)
            {
                consume();
            }
        }
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    if (!expect(TokenKind::LeftBrace, "'{' or ';' after namespace name"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member = parseNamespaceMember();
        if (member != nullptr)
        {
            node->addChild(member);
        }
        if (index_ == before)
        {
            consume();
        }
    }
    expect(TokenKind::RightBrace, "'}'");

    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseUsingDirective()
{
    const std::size_t beginIndex = index_;
    consume();

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "namespace name or alias");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& first = current();
    if (peek().kind() == TokenKind::Equal || peek().kind() == TokenKind::Less)
    {
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UsingAliasDirective,
            first.range());
        node->setText(std::string(first.lexeme()));
        consume();
        // Item 276. An alias may bind type parameters exactly where a generic declaration does:
        // `using Buffer<T> = T*;`.  Keeping the parameters on the alias AST node lets the early
        // generic pass erase the alias before it computes specialization identities.
        if (!parseGenericParameterList(node))
        {
            synchronizeGlobal();
            return nullptr;
        }
        KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);
        if (!expect(TokenKind::Equal, "'=' after using alias"))
        {
            synchronizeGlobal();
            return nullptr;
        }
        // C3 "FFI aliases". The target used to be a qualified NAME and nothing else, which made
        // `using DWORD = uint;`, `using HANDLE = void*;` and `using Rgba = float[4];` unwritable
        // -- and those three are most of what a C header's typedefs actually say. It is now a
        // full TYPE, parsed by the same routine every other type in the language goes through, so
        // builtins, pointers, fixed arrays and named types (delegates included) all work with no
        // special cases. A plain dotted name still parses as a type with no modifiers, which is
        // what keeps NAMESPACE aliases (`using IO = NativeSharp.Core.IO;`) working unchanged --
        // the name lookup decides which of the two a given target is.
        ast::TypeSyntax* target = parseType();
        if (target == nullptr)
        {
            synchronizeGlobal();
            return nullptr;
        }
        expect(TokenKind::Semicolon, "';'");
        node->setRange(tokenRangeFrom(beginIndex));
        node->setTypeSyntax(target);
        // The statement parser decides "is `X y = ...;` a declaration?" from this set, so an
        // alias name has to be in it or `DWORD d = 42u;` parses as an expression and fails. Same
        // registration a struct/class/enum name gets when it is declared.
        knownTypeNames_.insert(std::string(first.lexeme()));
        if (node->genericParameters().empty() && target->modifiers().empty() &&
            target->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None &&
            target->typeArguments().empty() && !lexer::isBuiltinTypeKeyword(target->baseTokenKind()))
        {
            // Only a bare dotted name can name a namespace, so only that shape keeps the textual
            // target the namespace-alias path reads.
            node->setAuxiliaryText(target->baseName());
        }
        return node;
    }

    std::string target;
    SourceRange targetRange = first.range();
    if (!parseQualifiedName(target, targetRange))
    {
        synchronizeGlobal();
        return nullptr;
    }
    expect(TokenKind::Semicolon, "';'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingNamespaceDirective,
        tokenRangeFrom(beginIndex));
    node->setText(std::move(target));
    return node;
}

ast::AstNode* Parser::parseGlobalDeclaration()
{
    // Item 287 keeps `asm` contextual: an existing type/local/member called `asm` is not broken.
    // The spelling only opens an assembly statement when followed by `{` or its explicit header. At
    // namespace scope that same unambiguous shape gets a precise diagnostic and is consumed as a
    // unit so recovery can continue at the next declaration.
    if (atContextualKeyword("asm") &&
        (peek().kind() == TokenKind::LeftBrace || peek().kind() == TokenKind::LeftParen))
    {
        const SourceRange keywordRange = current().range();
        parseInlineAssemblyStatement();
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            keywordRange,
            "asm blocks are only valid inside function or method bodies");
        return nullptr;
    }

    const std::size_t declarationBeginIndex = index_;
    TokenKind accessibility = TokenKind::Invalid;
    std::string granularVisibility;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    bool isExportedFunction = false;
    bool isInlineFunction = false;
    bool isForceInlineFunction = false;
    bool isNoInlineFunction = false;
    bool isReadonlyType = false;
    bool isRecordStruct = false;
    bool isCopyType = false;
    bool isUnsafeTraitContract = false;
    // C2.2 "union and explicit layout": `pack(N)` and `align(N)` are contextual-keyword layout
    // modifiers on a struct/union declaration -- the NativeSharp spellings of C's
    // `#pragma pack(N)` and `__declspec(align(N))`. They are contextual (an ordinary identifier
    // everywhere else), exactly like `partial`, so no existing program that used `pack` or
    // `align` as a name stops compiling.
    std::optional<std::uint64_t> layoutPack;
    std::optional<std::uint64_t> layoutAlign;
    // SHR-013 "vistas y tiempo de vida". `borrowed struct T` declara un TIPO VISTA: un agregado
    // por valor cuyos valores no poseen nada y viven prestados de otro. Es la misma palabra que
    // ya califica a un local o a un parametro -- `borrowed` significa "esto no es mio" en los dos
    // sitios -- solo que aqui la lleva el TIPO, no cada uso, porque una vista no deja de serlo
    // segun donde se guarde.
    //
    // POR QUE UN MODIFICADOR EXPLICITO Y NO INFERENCIA. La alternativa medida era deducirlo:
    // "un struct con un campo puntero es una vista". Habria convertido en vista cada struct que
    // hoy lleva un puntero sin pedirlo, cambiando el significado de codigo ya escrito y ya verde.
    // Un modificador que el autor escribe no rompe nada existente y dice la intencion en la
    // declaracion, que es donde se lee.
    bool isBorrowedView = false;
    // `size(N)`: el tamano que el autor AFIRMA que mide la struct. Ver el comentario del bloque
    // que lo parsea.
    std::optional<std::uint64_t> layoutSize;
    std::optional<std::string> representation;

    while (true)
    {
        if (atContextualKeyword("repr") && peek().kind() == TokenKind::LeftParen)
        {
            const SourceRange feature = consume().range();
            consume();
            std::string value;
            if (atContextualKeyword("native") || atContextualKeyword("C") ||
                atContextualKeyword("transparent"))
            {
                value = std::string(consume().lexeme());
            }
            else
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "repr requires exactly one of native, C, or transparent");
                if (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile)) consume();
            }
            expect(TokenKind::RightParen, "')' after representation attribute");
            if (representation.has_value())
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    feature,
                    "an aggregate may declare exactly one representation attribute");
            }
            else if (!value.empty()) representation = std::move(value);
            continue;
        }
        if (atContextualKeyword("unsafe") && peek().kind() == TokenKind::Identifier &&
            (peek().lexeme() == "trait" || peek().lexeme() == "impl"))
        {
            const lexer::Token& modifier = consume();
            if (isUnsafeTraitContract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate unsafe modifier on trait contract");
            }
            isUnsafeTraitContract = true;
            continue;
        }
        if ((atContextualKeyword("pack") || atContextualKeyword("align")) &&
            peek().kind() == TokenKind::LeftParen)
        {
            const bool isPack = atContextualKeyword("pack");
            const lexer::Token& keyword = consume();
            consume();
            std::optional<std::uint64_t> value;
            if (at(TokenKind::IntegerLiteral) &&
                std::holds_alternative<std::uint64_t>(current().value()))
            {
                value = std::get<std::uint64_t>(current().value());
                consume();
            }
            else
            {
                expect(TokenKind::IntegerLiteral, isPack ? "pack value" : "alignment value");
            }
            expect(TokenKind::RightParen, "')'");
            if (value.has_value() && (*value == 0 || (*value & (*value - 1)) != 0))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    keyword.range(),
                    std::string(isPack ? "pack" : "align") +
                        " requires a power-of-two byte value");
                value.reset();
            }
            if (isPack) layoutPack = value; else layoutAlign = value;
            continue;
        }
        // `size(N)` -- LA ASERCION DE TAMANO, para bindings.
        //
        // POR QUE EXISTE, y por que va en esta misma ranura que `pack` y `align`. El peor fallo que
        // puede producir un binding no es perder una distincion de tipos: es una struct cuyos BYTES
        // no coinciden con los de C. Eso no da un error -- da datos corruptos, y se manifiesta como
        // un campo que vale basura a doscientas llamadas de distancia.
        //
        // El lenguaje ya podia FIJAR el layout: `pack(N)`, `align(N)` y `at(N)` por campo. Lo que
        // no podia era COMPROBARLO de punta a punta, porque el relleno final de una struct no lo
        // escribe nadie: sale de la regla de alineacion. `size(N)` cierra eso.
        //
        // VERIFICA, NO FUERZA, y la diferencia es la razon de ser del modificador. Si forzara,
        // anadiria relleno hasta cuadrar y el desajuste quedaria TAPADO -- que es exactamente el
        // fallo que se quiere cazar. Un binding que rellena en silencio es un binding que miente.
        //
        // Y NO ES UN static_assert GENERAL. El lenguaje no tiene evaluacion de constantes en
        // tiempo de compilacion sobre layouts --el modelo semantico corre ANTES que el de layout,
        // y por eso `sizeof` se resuelve tarde-- asi que un assert general seria una promesa que
        // habria que sostener con maquinaria que no existe. Esto es un contrato de layout, escrito
        // donde se lee el layout, y no pretende ser otra cosa.
        if (atContextualKeyword("size") && peek().kind() == TokenKind::LeftParen)
        {
            const lexer::Token& keyword = consume();
            consume();
            std::optional<std::uint64_t> value;
            if (at(TokenKind::IntegerLiteral) &&
                std::holds_alternative<std::uint64_t>(current().value()))
            {
                value = std::get<std::uint64_t>(current().value());
                consume();
            }
            else
            {
                expect(TokenKind::IntegerLiteral, "size value");
            }
            expect(TokenKind::RightParen, "')'");
            // El cero se rechaza aqui y no en el modelo: una struct de cero bytes no existe en el
            // modelo de objetos de Windows x64, y `size(0)` casi siempre es un generador que no
            // supo leer el tamano y escribio su valor por omision.
            if (value.has_value() && *value == 0)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    keyword.range(),
                    "size requires a positive byte count");
                value.reset();
            }
            layoutSize = value;
            continue;
        }
        if (at(TokenKind::KwExport))
        {
            const lexer::Token& modifier = consume();
            if (isExportedFunction)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate export extern C modifier on namespace function");
            }
            isExportedFunction = true;
            const bool sawExtern = consumeIf(TokenKind::KwExtern);
            const bool sawC = sawExtern && atContextualKeyword("C");
            if (sawC) consume();
            if (!sawExtern || !sawC)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "namespace exports require the exact modifier sequence 'export extern C'");
            }
            continue;
        }
        if (at(TokenKind::KwInline) || at(TokenKind::KwForceInline) ||
            at(TokenKind::KwNoInline))
        {
            const lexer::Token& modifier = consume();
            bool* seen = modifier.kind() == TokenKind::KwInline ? &isInlineFunction :
                modifier.kind() == TokenKind::KwForceInline ? &isForceInlineFunction :
                &isNoInlineFunction;
            if (*seen)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate namespace-function inlining modifier");
            }
            *seen = true;
            continue;
        }
        if (at(TokenKind::KwPublic) || at(TokenKind::KwInternal) ||
            atContextualKeyword("module") || atContextualKeyword("package"))
        {
            const lexer::Token& modifier = consume();
            const bool contextualVisibility = modifier.kind() == TokenKind::Identifier;
            const TokenKind effective = contextualVisibility ? TokenKind::KwInternal : modifier.kind();
            if (accessibility != TokenKind::Invalid)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    accessibility == effective
                        ? "duplicate accessibility modifier on top-level type declaration"
                        : "conflicting accessibility modifiers on top-level type declaration");
            }
            else
            {
                accessibility = effective;
                if (contextualVisibility) granularVisibility = std::string(modifier.lexeme());
            }
            continue;
        }

        // NativeSharp 3.0 keeps `record` contextual. Only the unambiguous `record struct`
        // declaration shape consumes it, so a 2.x declaration named record remains compatible.
        if (atContextualKeyword("record") && peek().kind() == TokenKind::KwStruct)
        {
            const lexer::Token& modifier = consume();
            if (isRecordStruct)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate record modifier on struct declaration");
            }
            isRecordStruct = true;
            continue;
        }

        // NativeSharp 3.0 T06-F01. `copy` is contextual and is consumed only while it is
        // unambiguously decorating a value-type declaration. Other uses keep the identifier
        // available, while a declaration can make copyability an explicit source contract.
        const bool copyTypeModifier = atContextualKeyword("copy") &&
            (peek().kind() == TokenKind::KwStruct || peek().kind() == TokenKind::KwClass ||
             peek().kind() == TokenKind::KwInterface || peek().kind() == TokenKind::KwEnum ||
             peek().kind() == TokenKind::KwDelegate || peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal || peek().kind() == TokenKind::KwReadonly ||
             peek().kind() == TokenKind::KwBorrowed ||
             (peek().kind() == TokenKind::Identifier &&
              (peek().lexeme() == "pack" || peek().lexeme() == "align" ||
               peek().lexeme() == "size" || peek().lexeme() == "repr" || peek().lexeme() == "record" ||
               peek().lexeme() == "variant" || peek().lexeme() == "opaque" ||
               peek().lexeme() == "union")));
        if (copyTypeModifier)
        {
            const lexer::Token& modifier = consume();
            if (isCopyType)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate copy modifier on value-type declaration");
            }
            isCopyType = true;
            continue;
        }

        const bool readonlyTypeModifier = at(TokenKind::KwReadonly) &&
            (peek().kind() == TokenKind::KwStruct || peek().kind() == TokenKind::KwClass ||
             peek().kind() == TokenKind::KwInterface || peek().kind() == TokenKind::KwEnum ||
             peek().kind() == TokenKind::KwDelegate || peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal || peek().kind() == TokenKind::KwBorrowed ||
             (peek().kind() == TokenKind::Identifier &&
              (peek().lexeme() == "pack" || peek().lexeme() == "align" || peek().lexeme() == "repr" ||
               peek().lexeme() == "size" || peek().lexeme() == "union" ||
                peek().lexeme() == "record" || peek().lexeme() == "variant" ||
                peek().lexeme() == "copy")));
        if (readonlyTypeModifier)
        {
            const lexer::Token& modifier = consume();
            if (isReadonlyType)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate readonly modifier on type declaration");
            }
            isReadonlyType = true;
            continue;
        }

        const bool partialModifier = atContextualKeyword("partial") &&
            (peek().kind() == TokenKind::KwClass ||
             peek().kind() == TokenKind::KwInterface ||
             peek().kind() == TokenKind::KwStruct ||
             peek().kind() == TokenKind::KwEnum ||
             peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal ||
             peek().kind() == TokenKind::KwAbstract ||
             peek().kind() == TokenKind::KwSealed ||
             peek().kind() == TokenKind::KwStatic ||
             (peek().kind() == TokenKind::Identifier &&
              (peek().lexeme() == "partial" || peek().lexeme() == "repr")));
        if (partialModifier)
        {
            const lexer::Token& modifier = consume();
            if (isPartial)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate 'partial' modifier on type declaration");
            }
            isPartial = true;
            continue;
        }
        // LA GUARDA NO ES OPCIONAL, y costo una regresion descubrirlo:
        // `borrowed` YA existe como calificativo de tipo de retorno de una FUNCION GLOBAL --
        // `borrowed Resource Observe(owned Resource source)` es codigo valido desde Stage 50 y
        // esta en Stage50_1OwnershipSyntaxTests. Sin la guarda, este bucle se comia ese
        // `borrowed` como modificador de declaracion de tipo y la funcion perdia su calificativo
        // en el AST, silenciosamente.
        //
        // Es la misma forma de guarda que `partial` usa tres bloques mas abajo, y por la misma
        // razon: una palabra que significa dos cosas se desambigua mirando lo que la sigue.
        //
        // La lista incluye `class`, `enum`, `union` y `delegate` A PROPOSITO aunque `borrowed` no
        // sea legal en ninguno: consumirlo ahi es lo que permite dar el diagnostico que explica
        // POR QUE no lo es, en vez de un "expected a global declaration" que no ayuda a nadie.
        const bool borrowedTypeModifier = at(TokenKind::KwBorrowed) &&
            (peek().kind() == TokenKind::KwStruct ||
             peek().kind() == TokenKind::KwClass ||
             peek().kind() == TokenKind::KwInterface ||
             peek().kind() == TokenKind::KwEnum ||
             peek().kind() == TokenKind::KwDelegate ||
             peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal ||
             peek().kind() == TokenKind::KwAbstract ||
             peek().kind() == TokenKind::KwSealed ||
             peek().kind() == TokenKind::KwStatic ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "union") ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "variant") ||
              (peek().kind() == TokenKind::Identifier && peek().lexeme() == "copy") ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "partial"));
        if (borrowedTypeModifier)
        {
            const lexer::Token& modifier = consume();
            if (isBorrowedView)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate 'borrowed' modifier on type declaration");
            }
            isBorrowedView = true;
            continue;
        }
        if (at(TokenKind::KwAbstract))
        {
            const lexer::Token& modifier = consume();
            if (isAbstract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate abstract modifier on type declaration");
            }
            isAbstract = true;
            continue;
        }
        if (at(TokenKind::KwSealed))
        {
            const lexer::Token& modifier = consume();
            if (isSealed)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate sealed modifier on type declaration");
            }
            isSealed = true;
            continue;
        }
        if (at(TokenKind::KwStatic))
        {
            const lexer::Token& modifier = consume();
            if (isStatic)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate static modifier on type declaration");
            }
            isStatic = true;
            continue;
        }
        break;
    }

    bool isAsyncMethod = false;
    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador se baja al callable.
        isAsyncMethod = true;
        consume();
    }

    ast::AstNode* declaration = nullptr;
    if (at(TokenKind::KwDelegate))
    {
        if (isPartial || isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "delegate declarations cannot be partial, abstract, sealed, or static");
        }
        declaration = parseDelegateDeclaration();
    }
    else if (at(TokenKind::KwClass))
    {
        declaration = parseClassDeclaration(isPartial, isAbstract, isSealed, isStatic);
    }
    else if (at(TokenKind::KwInterface))
    {
        if (isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "interfaces cannot use abstract, sealed, or static type modifiers");
        }
        declaration = parseInterfaceDeclaration(isPartial);
    }
    else if (atContextualKeyword("trait") && peek().kind() == TokenKind::Identifier)
    {
        if (isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "traits cannot use abstract, sealed, or static type modifiers");
        }
        declaration = parseInterfaceDeclaration(isPartial, true);
    }
    else if (atContextualKeyword("impl"))
    {
        if (accessibility != TokenKind::Invalid || isPartial || isAbstract || isSealed ||
            isStatic || isReadonlyType || isRecordStruct || isBorrowedView ||
            isCopyType || representation.has_value() || layoutPack.has_value() ||
            layoutAlign.has_value() || layoutSize.has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "an impl block cannot declare accessibility, layout, or type modifiers");
        }
        declaration = parseTraitImplementationDeclaration();
    }
    else if (atContextualKeyword("variant") && peek().kind() == TokenKind::Identifier)
    {
        if (isPartial || isAbstract || isSealed || isStatic || isRecordStruct)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "variant declarations cannot be partial, abstract, sealed, static, or record");
        }
        declaration = parseVariantDeclaration();
    }
    else if (atContextualKeyword("opaque") &&
             peek().kind() == TokenKind::Identifier && peek().lexeme() == "type")
    {
        if (isPartial || isAbstract || isSealed || isStatic || isRecordStruct ||
            isBorrowedView || isReadonlyType)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "opaque type declarations cannot be partial, abstract, sealed, static, record, "
                "borrowed, or readonly");
        }
        if (layoutPack.has_value() || layoutAlign.has_value() || layoutSize.has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "an opaque type inherits the size and alignment of its representation and cannot "
                "override pack, align, or size");
            layoutPack.reset();
            layoutAlign.reset();
            layoutSize.reset();
        }
        declaration = parseOpaqueTypeDeclaration(isCopyType);
    }
    else if (at(TokenKind::KwStruct) ||
             (atContextualKeyword("union") && peek().kind() == TokenKind::Identifier))
    {
        // SHR-013: `borrowed union` queda fuera a proposito. Una union no tiene un miembro vivo
        // conocido, asi que no hay forma de decir de que presta -- y una vista cuyo origen no se
        // puede nombrar es exactamente lo que la comprobacion de escape existe para impedir.
        if (isBorrowedView && !at(TokenKind::KwStruct))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "'borrowed' declares a view type and applies only to struct declarations");
        }
        if (isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "abstract, sealed, and static modifiers are supported only for class declarations");
        }
        if (isPartial)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "'partial' is supported only for class declarations");
        }
        declaration = parseStructDeclaration(
            at(TokenKind::KwStruct) ? StructLayoutForm::Struct : StructLayoutForm::Union,
            isBorrowedView && at(TokenKind::KwStruct),
            isRecordStruct,
            isCopyType);
    }
    else if (at(TokenKind::KwEnum))
    {
        if (isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "abstract, sealed, and static modifiers are supported only for class declarations");
        }
        if (isPartial)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "'partial' is not valid on enum declarations");
        }
        declaration = parseEnumDeclaration();
    }
    else if (at(TokenKind::KwExtern))
    {
        // `extern C delegate T Name(...);` is a TYPE declaration like any other, so it has to be
        // able to carry an accessibility modifier. Before this branch existed, the `extern` case
        // sat further down the chain -- AFTER the "top-level accessibility supports only ..."
        // error -- so `public extern C delegate` was rejected outright and an extern C delegate
        // could only ever be internal.
        //
        // That is the real defect behind the report that "an extern C delegate does not cross a
        // ProjectReference". Only public types cross a module boundary, which is the same rule as
        // C# and is correct; the delegate simply had no way to say `public`. The classes and
        // structs beside it in the generated bindings could, which is why they crossed and it did
        // not, and why the whole bindings tree got restructured into one project per library to
        // work around something that was one missing branch here.
        //
        // An `extern C { ... }` BLOCK is not a type and has no accessibility of its own -- its
        // members carry their own -- so a modifier on it is still an error, with its own reason.
        if (isPartial || isAbstract || isSealed || isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "extern declarations cannot be partial, abstract, sealed, or static");
        }
        ast::AstNode* externDeclaration = parseExternCDeclaration();
        if (externDeclaration != nullptr &&
            externDeclaration->kind() == ast::AstNodeKind::DelegateDeclaration)
        {
            // Fall through to the shared tail, which is what stamps the accessibility.
            declaration = externDeclaration;
        }
        else
        {
            if (accessibility != TokenKind::Invalid)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[declarationBeginIndex].range(),
                    "an 'extern C' block is not a type and has no accessibility of its own; put "
                    "public/internal on its members instead");
            }
            return externDeclaration;
        }
    }
    else if (isPartial)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "'partial' must modify a class declaration");
        synchronizeGlobal();
        return nullptr;
    }
    else if (isAbstract || isSealed || isStatic)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            isStatic
                ? "static namespace declarations/file-local linkage are not supported in "
                  "NativeSharp 3.0; use an internal uniquely named namespace state"
                : "abstract and sealed modifiers are supported only for class declarations");
        synchronizeGlobal();
        return nullptr;
    }
    else if (isReadonlyType)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "readonly must modify a non-union struct declaration");
        synchronizeGlobal();
        return nullptr;
    }
    else if (at(TokenKind::KwConst) || lexer::isOwnershipQualifierKeyword(current().kind()) ||
             lexer::isBuiltinTypeKeyword(current().kind()) || at(TokenKind::Identifier))
    {
        declaration = parseFunctionOrGlobalVariable();
        if (declaration != nullptr && accessibility != TokenKind::Invalid)
        {
            declaration->setTokenKind(accessibility);
            declaration->addModifier(accessibility);
        }
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected a global declaration");
        synchronizeGlobal();
        return nullptr;
    }

    if (declaration != nullptr && isExportedFunction)
    {
        if (declaration->kind() != ast::AstNodeKind::FunctionDeclaration)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "export extern C is valid only on a namespace function");
        }
        else
        {
            declaration->addModifier(TokenKind::KwExport);
            declaration->addModifier(TokenKind::KwExtern);
            if (declaration->first() == nullptr)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "an exported namespace function requires a body");
            }
        }
    }
    if (declaration != nullptr &&
        (isInlineFunction || isForceInlineFunction || isNoInlineFunction))
    {
        const unsigned policyCount = static_cast<unsigned>(isInlineFunction) +
            static_cast<unsigned>(isForceInlineFunction) +
            static_cast<unsigned>(isNoInlineFunction);
        if (declaration->kind() != ast::AstNodeKind::FunctionDeclaration)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "inline, forceinline, and noinline apply only to namespace functions");
        }
        else if (policyCount != 1U)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "inline, forceinline, and noinline are contradictory inlining policies");
        }
        else if (declaration->first() == nullptr)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "a namespace-function inlining policy requires a function body");
        }
        else
        {
            declaration->addModifier(isInlineFunction ? TokenKind::KwInline :
                isForceInlineFunction ? TokenKind::KwForceInline : TokenKind::KwNoInline);
        }
    }
    if (declaration != nullptr)
    {
        if (isUnsafeTraitContract)
            declaration->setUnsafe();
        if (!granularVisibility.empty())
        {
            std::string markers = declaration->auxiliaryText();
            markers += markers.empty() ? "" : ",";
            markers += "visibility=" + granularVisibility;
            declaration->setAuxiliaryText(std::move(markers));
        }
        if (isCopyType)
        {
            if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "copy applies only to struct, record, variant, and opaque value-type declarations");
            }
            else if (declaration->auxiliaryText().find("copy") == std::string::npos)
            {
                std::string markers = declaration->auxiliaryText();
                markers += (markers.empty() ? "" : ",");
                markers += "copy";
                declaration->setAuxiliaryText(std::move(markers));
            }
        }
        // SHR-013: la misma forma que pack/align de abajo. `borrowed` describe el TIEMPO DE VIDA
        // de un valor, y solo un agregado por valor lo tiene sin dueno: una clase se posee por
        // referencia (`owned`/`borrowed` en cada uso ya lo dicen), un enum y un delegate no
        // prestan nada de nadie.
        if (isBorrowedView && declaration->kind() != ast::AstNodeKind::StructDeclaration)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "'borrowed' declares a view type and applies only to struct declarations");
        }
        if (isBorrowedView && declaration->kind() == ast::AstNodeKind::StructDeclaration &&
            declaration->auxiliaryText().find("variant") != std::string::npos)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "'borrowed' applies only to struct view declarations; variants own their active storage");
        }
        if (isReadonlyType)
        {
            if (declaration->kind() != ast::AstNodeKind::StructDeclaration ||
                declaration->auxiliaryText().find("union") != std::string::npos ||
                declaration->auxiliaryText().find("variant") != std::string::npos)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "readonly applies only to non-union struct declarations");
            }
            else
            {
                declaration->addModifier(TokenKind::KwReadonly);
                for (ast::AstNode* member : declaration->children())
                {
                    if (member != nullptr &&
                        member->kind() == ast::AstNodeKind::FieldDeclaration &&
                        member->auxiliaryText() == "promoted-primary-field" &&
                        !member->hasModifier(TokenKind::KwReadonly))
                    {
                        member->addModifier(TokenKind::KwReadonly);
                    }
                }
            }
        }
        // C2.2: pack/align describe how an aggregate's FIELDS are placed, so they mean nothing on
        // a class (heap object with its own header), an interface or an enum. Refusing them there
        // is what keeps the modifier from becoming a decoration nobody honours. The values ride
        // on the declaration's auxiliary text, joined to the union marker parseStructDeclaration
        // already wrote, which is where ProjectObjectLayoutModel reads them back.
        if ((layoutPack.has_value() || layoutAlign.has_value() || layoutSize.has_value()))
        {
            if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "pack/align/size layout modifiers apply only to struct and union declarations");
            }
            else if (declaration->auxiliaryText().find("variant") != std::string::npos)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "pack/align/size cannot override the canonical tagged-variant layout");
            }
            else
            {
                std::string layout = declaration->auxiliaryText();
                if (layoutPack.has_value())
                {
                    layout += (layout.empty() ? "" : ",");
                    layout += "pack=" + std::to_string(*layoutPack);
                }
                if (layoutAlign.has_value())
                {
                    layout += (layout.empty() ? "" : ",");
                    layout += "align=" + std::to_string(*layoutAlign);
                }
                if (layoutSize.has_value())
                {
                    layout += (layout.empty() ? "" : ",");
                    layout += "size=" + std::to_string(*layoutSize);
                }
                declaration->setAuxiliaryText(std::move(layout));
            }
        }
        if (representation.has_value())
        {
            if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    declaration->range(),
                    "repr applies only to struct and union declarations");
            }
            else
            {
                const bool isUnion = declaration->auxiliaryText().find("union") != std::string::npos;
                if (*representation == "transparent" &&
                    (isUnion || layoutPack.has_value() || layoutAlign.has_value() ||
                     layoutSize.has_value()))
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        declaration->range(),
                        "repr(transparent) cannot be a union or override pack, align, or size");
                }
                std::string markers = declaration->auxiliaryText();
                markers += (markers.empty() ? "" : ",");
                markers += "repr=" + *representation;
                declaration->setAuxiliaryText(std::move(markers));
            }
        }
        if (accessibility != TokenKind::Invalid &&
            declaration->tokenKind() == TokenKind::Invalid)
        {
            declaration->setTokenKind(accessibility);
        }
        if (declarationBeginIndex < tokens_.size())
        {
            declaration->setRange(join(tokens_[declarationBeginIndex].range(), declaration->range()));
        }
    }
    if (isAsyncMethod && declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::FunctionDeclaration)
    {
        std::string marker = declaration->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "async";
        declaration->setAuxiliaryText(std::move(marker));
    }
    return declaration;
}

ast::AstNode* Parser::parseDelegateDeclaration()
{
    const std::size_t beginIndex = index_;
    consume(); // delegate

    const bool isReferenceReturn = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1U) || peek().kind() == TokenKind::LeftBracket);
    std::string returnLifetime;
    if (isReferenceReturn)
    {
        consume();
        returnLifetime = parseOptionalLifetimeAnnotation("delegate ref return");
    }
    if (!canStartTypeAt(index_))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "delegate declaration requires a return type");
        synchronizeGlobal();
        return nullptr;
    }
    ast::TypeSyntax* returnType = parseType();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "delegate type name");
        synchronizeGlobal();
        return nullptr;
    }
    const lexer::Token& name = consume();
    knownTypeNames_.insert(std::string(name.lexeme()));
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::DelegateDeclaration, name.range());
    node->setText(std::string(name.lexeme()));
    node->setTypeSyntax(returnType);
    node->setReference(isReferenceReturn);
    node->setLifetimeName(std::move(returnLifetime));
    node->setAuxiliaryText("delegate");
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parseParameter(); parameter != nullptr)
            {
                node->addChild(parameter);
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    parseThrowsClause(node);
    parseFfiUnwindClause(node);
    // NSC-LIMIT-007: `delegate void D<T>(T x) where T : class;` -- despues de la lista de
    // parametros y antes del punto y coma, que es donde el lenguaje la pone.
    parseGenericConstraintClauses(node);
    expect(TokenKind::Semicolon, "';'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseStructDeclaration(
    StructLayoutForm form,
    bool isBorrowedView,
    bool isRecordStruct,
    bool isCopyType)
{
    const std::size_t beginIndex = index_;
    consume();

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, form == StructLayoutForm::Union ? "union name" : "struct name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    const std::string structName(name.lexeme());
    knownTypeNames_.insert(structName);
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::StructDeclaration, name.range());
    node->setText(structName);
    // C2.2: a union IS a struct in every respect this compiler already models -- same members,
    // same by-value semantics, same C ABI classification, same LLVM byte-array carrier -- except
    // that its fields all start at the same place. Rather than a parallel declaration kind
    // rippling through every switch over NamedTypeDeclarationKind, the difference is recorded
    // here as a layout marker and honoured in exactly one place: ProjectObjectLayoutModel.
    if (form == StructLayoutForm::Union)
    {
        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "union";
        node->setAuxiliaryText(std::move(markers));
    }
    // SHR-013: la marca de vista viaja por el MISMO canal que "union"/"pack="/"align=" -- el
    // texto auxiliar de la declaracion, separado por comas -- porque es exactamente la misma
    // clase de dato: una propiedad del agregado que un modelo aguas abajo vuelve a leer, sin
    // anadir un AstNodeKind nuevo que obligue a tocar cada switch sobre declaraciones.
    if (isBorrowedView)
    {
        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "borrowedview";
        node->setAuxiliaryText(std::move(markers));
    }
    if (isRecordStruct)
    {
        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "record";
        node->setAuxiliaryText(std::move(markers));
    }
    if (isCopyType)
    {
        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "copy";
        node->setAuxiliaryText(std::move(markers));
    }
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);

    // NativeSharp 3.0 T02-F05. A primary struct/record parameter list is deliberately
    // desugared here, before name binding, into the ordinary fields and constructor that every
    // later compiler stage already understands. There is no hidden capture and no alternate ABI:
    // the generated fields are the physical layout, and the generated constructor is an ordinary
    // constructor whose body assigns those fields in textual order.
    if (form == StructLayoutForm::Struct && at(TokenKind::LeftParen))
    {
        consume();
        std::vector<ast::AstNode*> parameters;
        std::unordered_set<std::string> names;
        if (!at(TokenKind::RightParen))
        {
            while (true)
            {
                TokenKind accessibility = TokenKind::Invalid;
                bool readonlyField = false;
                while (isAccessibilityToken(current().kind()) || at(TokenKind::KwReadonly))
                {
                    const lexer::Token& modifier = consume();
                    if (modifier.kind() == TokenKind::KwReadonly)
                    {
                        if (readonlyField)
                        {
                            diagnostics_.report(
                                diagnostics::DiagnosticId::ExpectedDeclaration,
                                diagnostics::DiagnosticSeverity::Error,
                                modifier.range(),
                                "duplicate readonly modifier on promoted field");
                        }
                        readonlyField = true;
                    }
                    else if (accessibility != TokenKind::Invalid)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            modifier.range(),
                            "conflicting accessibility modifiers on promoted field");
                    }
                    else
                    {
                        accessibility = modifier.kind();
                    }
                }

                ast::AstNode* parameter = parseParameter();
                if (parameter != nullptr)
                {
                    if (!names.insert(parameter->text()).second)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::DuplicateSymbol,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "duplicate promoted field '" + parameter->text() + "'");
                    }
                    if (!parameter->auxiliaryText().empty())
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "a promoted parameter cannot be params, ref, out, or in");
                    }
                    if (parameter->typeSyntax() != nullptr &&
                        parameter->typeSyntax()->ownershipQualifier() !=
                            ast::OwnershipQualifierSyntaxKind::None)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "owned and borrowed promoted parameters require the T13 ownership "
                            "and partial-drop contract; declare the field and constructor explicitly");
                    }

                    const TokenKind fieldAccessibility = accessibility == TokenKind::Invalid
                        ? (isRecordStruct ? TokenKind::KwPublic : TokenKind::KwPrivate)
                        : accessibility;
                    if (fieldAccessibility == TokenKind::KwProtected)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "a promoted struct field cannot be protected because structs are not inheritable");
                    }
                    ast::AstNode* field = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::FieldDeclaration, parameter->range());
                    field->setText(parameter->text());
                    field->setTypeSyntax(parameter->typeSyntax());
                    field->setAuxiliaryText("promoted-primary-field");
                    field->setTokenKind(fieldAccessibility);
                    field->addModifier(fieldAccessibility);
                    if (readonlyField) field->addModifier(TokenKind::KwReadonly);
                    node->addChild(field);
                    parameters.push_back(parameter);
                }
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')'");

        ast::AstNode* constructor = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ConstructorDeclaration,
            SourceRange(name.range().begin(), tokens_[index_ - 1].range().end()));
        constructor->setText(structName);
        constructor->setAuxiliaryText("promoted-primary-constructor");
        constructor->setTokenKind(TokenKind::KwPublic);
        constructor->addModifier(TokenKind::KwPublic);
        ast::AstNode* body = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, constructor->range());
        for (ast::AstNode* parameter : parameters)
        {
            constructor->addChild(parameter);
            ast::AstNode* receiver = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ThisExpression, parameter->range());
            ast::AstNode* field = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MemberExpression, parameter->range());
            field->setText(parameter->text());
            field->setFirst(receiver);
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IdentifierExpression, parameter->range());
            value->setText(parameter->text());
            ast::AstNode* assignment = makeAssignmentExpression(field, value);
            body->addChild(makeExpressionStatement(assignment));
        }
        constructor->setFirst(body);
        node->addChild(constructor);

        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "primary-fields=" + std::to_string(parameters.size());
        node->setAuxiliaryText(std::move(markers));
    }

    if (consumeIf(TokenKind::Colon))
    {
        bool first = true;
        do
        {
            if (!canStartTypeAt(index_))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedType,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "expected trait type after ':'/',' on struct declaration");
                break;
            }
            ast::TypeSyntax* specification = parseType();
            if (first)
            {
                node->setTypeSyntax(specification);
                first = false;
            }
            node->addTypeSyntax(specification);
        }
        while (consumeIf(TokenKind::Comma));
    }

    bool sawRecordDeriveClause = false;
    const auto parseRecordDerives = [&]()
    {
        if (!atContextualKeyword("derive") || peek().kind() != TokenKind::LeftParen) return;
        const lexer::Token& deriveKeyword = consume();
        const bool duplicateClause = sawRecordDeriveClause;
        if (duplicateClause)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                deriveKeyword.range(),
                "duplicate derive(...) clause on record struct");
        }
        sawRecordDeriveClause = true;
        if (!isRecordStruct)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                deriveKeyword.range(),
                "derive(...) is valid only on a record struct");
        }
        consume();
        std::unordered_set<std::string> derives;
        if (!at(TokenKind::RightParen))
        {
            while (true)
            {
                if (!at(TokenKind::Identifier))
                {
                    expect(TokenKind::Identifier, "record derive name");
                    break;
                }
                const lexer::Token& derive = consume();
                const std::string deriveName(derive.lexeme());
                if (deriveName != "Equality" && deriveName != "Hash" &&
                    deriveName != "Print" && deriveName != "Clone")
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        derive.range(),
                        "unknown record derive '" + deriveName +
                            "'; expected Equality, Hash, Print, or Clone");
                }
                else if (!derives.insert(deriveName).second)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        derive.range(),
                        "duplicate record derive '" + deriveName + "'");
                }
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')'");
        if (isRecordStruct && !duplicateClause && !derives.empty())
        {
            static constexpr std::array<std::string_view, 4> order = {
                "Equality", "Hash", "Print", "Clone"};
            std::string marker = "derive=";
            bool first = true;
            for (const std::string_view derive : order)
            {
                if (!derives.contains(std::string(derive))) continue;
                if (!first) marker += '|';
                marker += derive;
                first = false;
            }
            std::string markers = node->auxiliaryText();
            markers += (markers.empty() ? "" : ",");
            markers += marker;
            node->setAuxiliaryText(std::move(markers));
        }
    };
    parseRecordDerives();
    // A struct never has a class base. NativeSharp 3 permits the preceding list only for trait
    // conformance; generic constraints still occupy the ordinary position before the body.
    parseGenericConstraintClauses(node);
    parseRecordDerives();

    return parseStructBody(node, structName, beginIndex);
}

ast::AstNode* Parser::parseVariantDeclaration()
{
    struct ParsedCase final
    {
        std::string name;
        SourceRange range;
        ast::TypeSyntax* payloadType = nullptr;
    };

    const std::size_t beginIndex = index_;
    consume();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "variant name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    const std::string variantName(name.lexeme());
    knownTypeNames_.insert(variantName);
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StructDeclaration, name.range());
    node->setText(variantName);
    node->setAuxiliaryText("variant,move-only-value");
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);
    parseGenericConstraintClauses(node);

    if (!expect(TokenKind::LeftBrace, "'{' after variant declaration"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    std::vector<ParsedCase> cases;
    std::unordered_set<std::string> caseNames;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "variant case name");
            consume();
            continue;
        }
        const lexer::Token& caseName = consume();
        ParsedCase item{std::string(caseName.lexeme()), caseName.range(), nullptr};
        const bool uniqueCase = caseNames.insert(item.name).second;
        if (!uniqueCase)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                caseName.range(),
                "duplicate variant case '" + item.name + "'");
        }

        if (consumeIf(TokenKind::LeftParen))
        {
            item.payloadType = parseType();
            if (item.payloadType == nullptr)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedType,
                    diagnostics::DiagnosticSeverity::Error,
                    caseName.range(),
                    "variant payload requires a type");
            }
            else
            {
                if (item.payloadType->baseTokenKind() == TokenKind::KwVoid &&
                    item.payloadType->modifiers().empty())
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        item.payloadType->range(),
                        "a variant payload cannot have type void; omit the payload instead");
                }
                if (item.payloadType->ownershipQualifier() ==
                    ast::OwnershipQualifierSyntaxKind::Borrowed)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        item.payloadType->range(),
                        "borrowed variant payloads are outside the NativeSharp 3.0 storage contract; "
                        "wrap the borrow in a borrowed struct view");
                }
            }
            if (at(TokenKind::Comma))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "a variant case has one payload type; use a tuple type for multiple values");
                while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile)) consume();
            }
            expect(TokenKind::RightParen, "')' after variant payload type");
        }
        if (uniqueCase) cases.push_back(std::move(item));
        if (!consumeIf(TokenKind::Comma) && !at(TokenKind::RightBrace))
        {
            expect(TokenKind::Comma, "',' between variant cases");
        }
    }
    expect(TokenKind::RightBrace, "'}' to close variant declaration");

    if (cases.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "a variant must declare at least one case");
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    // Every generated public member must have a unique source-level spelling. Catch collisions
    // here so a case named `CaseTag` or an `IsX`/`X` pair cannot create an ambiguous API later.
    std::unordered_set<std::string> generatedNames = {"CaseTag"};
    for (const ParsedCase& item : cases)
    {
        const std::array<std::string, 2> names = {item.name, "Is" + item.name};
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            if (!generatedNames.insert(names[index]).second)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    item.range,
                    "variant case '" + item.name +
                        "' collides with generated member '" + names[index] + "'");
            }
        }
    }

    const TokenKind tagKind = cases.size() <= 0x100U
        ? TokenKind::KwByte
        : cases.size() <= 0x10000U ? TokenKind::KwUShort : TokenKind::KwUInt;
    const std::string tagTypeName = tagKind == TokenKind::KwByte
        ? "byte" : tagKind == TokenKind::KwUShort ? "ushort" : "uint";
    const auto makeTagType = [&]()
    {
        return arena_.make<ast::TypeSyntax>(name.range(), tagKind, tagTypeName);
    };
    const auto makeSelfType = [&]()
    {
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            name.range(), TokenKind::Identifier, variantName);
        for (const std::string& parameter : node->genericParameters())
        {
            type->addTypeArgument(arena_.make<ast::TypeSyntax>(
                name.range(), TokenKind::Identifier, parameter));
        }
        return type;
    };
    const auto markPublic = [](ast::AstNode* member)
    {
        member->setTokenKind(TokenKind::KwPublic);
        member->addModifier(TokenKind::KwPublic);
    };
    const auto markPrivate = [](ast::AstNode* member)
    {
        member->setTokenKind(TokenKind::KwPrivate);
        member->addModifier(TokenKind::KwPrivate);
    };
    const auto makeBlock = [&](SourceRange range)
    {
        return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    };
    const auto makeReturn = [&](ast::AstNode* expression, SourceRange range)
    {
        ast::AstNode* statement = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ReturnStatement, range);
        statement->setFirst(expression);
        return statement;
    };
    const auto makeMove = [&](ast::AstNode* expression, SourceRange range)
    {
        ast::AstNode* move = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, range);
        move->setFirst(expression);
        return move;
    };

    ast::AstNode* tagField = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, name.range());
    tagField->setText("__nsc_variant_tag");
    tagField->setTypeSyntax(makeTagType());
    tagField->setAuxiliaryText("variant-tag");
    markPrivate(tagField);
    node->addChild(tagField);

    const bool hasPayload = std::ranges::any_of(
        cases, [](const ParsedCase& item) { return item.payloadType != nullptr; });
    // The nested payload identity is derived from the declared type name, not from file id or
    // byte offset. Moving a declaration or adding an earlier source file must not rename an ABI
    // carrier in NSMETA/host headers.
    const std::string payloadTypeName = "__nsc_variant_payload_" + variantName;
    const std::string payloadFieldName = "__nsc_variant_payload";
    if (hasPayload)
    {
        ast::AstNode* payloadDeclaration = arena_.make<ast::AstNode>(
            ast::AstNodeKind::StructDeclaration, name.range());
        payloadDeclaration->setText(payloadTypeName);
        payloadDeclaration->setAuxiliaryText("union,variant-payload");
        payloadDeclaration->setTokenKind(TokenKind::KwPrivate);
        for (const ParsedCase& item : cases)
        {
            if (item.payloadType == nullptr) continue;
            ast::AstNode* field = arena_.make<ast::AstNode>(
                ast::AstNodeKind::FieldDeclaration, item.range);
            field->setText("__nsc_case_" + item.name);
            field->setTypeSyntax(item.payloadType);
            field->setAuxiliaryText("variant-case-payload:" + item.name);
            markPublic(field);
            payloadDeclaration->addChild(field);
        }
        node->addChild(payloadDeclaration);

        ast::AstNode* payloadField = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration, name.range());
        payloadField->setText(payloadFieldName);
        payloadField->setTypeSyntax(arena_.make<ast::TypeSyntax>(
            name.range(), TokenKind::Identifier, payloadTypeName));
        payloadField->setAuxiliaryText("variant-payload-carrier");
        markPrivate(payloadField);
        node->addChild(payloadField);
    }

    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
    {
        const ParsedCase& item = cases[caseIndex];
        ast::AstNode* factory = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MethodDeclaration, item.range);
        factory->setText(item.name);
        factory->setTypeSyntax(makeSelfType());
        factory->setAuxiliaryText(
            "variant-case-factory:" + item.name + ":" + std::to_string(caseIndex));
        markPublic(factory);
        factory->addModifier(TokenKind::KwStatic);
        if (item.payloadType != nullptr)
        {
            ast::AstNode* parameter = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, item.range);
            parameter->setText("value");
            parameter->setTypeSyntax(item.payloadType);
            factory->addChild(parameter);
        }

        ast::AstNode* factoryBody = makeBlock(item.range);
        ast::AstNode* defaultValue = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DefaultValueExpression, item.range);
        defaultValue->setTypeSyntax(makeSelfType());
        factoryBody->addChild(makeVariableDeclaration(
            makeSelfType(), "__nsc_variant_result", defaultValue, false));
        factoryBody->addChild(makeExpressionStatement(makeAssignmentExpression(
            makeMemberExpression(
                makeIdentifierExpression(item.range, "__nsc_variant_result"),
                "__nsc_variant_tag"),
            makeIntegerLiteralExpression(item.range, caseIndex))));
        if (item.payloadType != nullptr)
        {
            ast::AstNode* payload = makeMemberExpression(
                makeIdentifierExpression(item.range, "__nsc_variant_result"),
                payloadFieldName);
            payload = makeMemberExpression(payload, "__nsc_case_" + item.name);
            ast::AstNode* payloadValue = makeMove(
                makeIdentifierExpression(item.range, "value"), item.range);
            payloadValue->setAuxiliaryText("variant-payload-transfer");
            factoryBody->addChild(makeExpressionStatement(makeAssignmentExpression(
                payload, payloadValue)));
        }
        ast::AstNode* resultMove = makeMove(
            makeIdentifierExpression(item.range, "__nsc_variant_result"), item.range);
        resultMove->setAuxiliaryText("variant-result-transfer");
        factoryBody->addChild(makeReturn(resultMove, item.range));
        factory->setFirst(factoryBody);
        node->addChild(factory);

        ast::AstNode* testMethod = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MethodDeclaration, item.range);
        testMethod->setText("Is" + item.name);
        testMethod->setTypeSyntax(arena_.make<ast::TypeSyntax>(
            item.range, TokenKind::KwBool, "bool"));
        testMethod->setAuxiliaryText(
            "variant-case-test:" + item.name + ":" + std::to_string(caseIndex));
        markPublic(testMethod);
        testMethod->addModifier(TokenKind::KwReadonly);
        ast::AstNode* testBody = makeBlock(item.range);
        testBody->addChild(makeReturn(makeBinaryExpression(
            TokenKind::EqualEqual,
            makeIdentifierExpression(item.range, "__nsc_variant_tag"),
            makeIntegerLiteralExpression(item.range, caseIndex)), item.range));
        testMethod->setFirst(testBody);
        node->addChild(testMethod);

        if (item.payloadType != nullptr)
        {
            ast::AstNode* payloadMethod = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MethodDeclaration, item.range);
            // This member is public for cross-module pattern lowering. ProjectMethodBodyModel
            // rejects calls to the reserved name unless this CallExpression carries the marker
            // emitted by parseMatchStatement, so user source cannot bypass the tag test and read
            // an inactive union arm. A source-valid spelling is required because NSMETA verifies
            // its generated public API by reparsing it.
            payloadMethod->setText("__nsc_variant_extract_" + item.name);
            payloadMethod->setTypeSyntax(item.payloadType);
            payloadMethod->setAuxiliaryText(
                "variant-case-extract:" + item.name + ":" + std::to_string(caseIndex));
            markPublic(payloadMethod);
            ast::AstNode* payloadBody = makeBlock(item.range);
            ast::AstNode* payloadMove = makeMove(makeMemberExpression(
                makeIdentifierExpression(item.range, payloadFieldName),
                "__nsc_case_" + item.name), item.range);
            payloadMove->setAuxiliaryText("variant-payload-extract");
            payloadBody->addChild(makeReturn(payloadMove, item.range));
            payloadMethod->setFirst(payloadBody);
            node->addChild(payloadMethod);
        }
    }

    ast::AstNode* tagMethod = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, name.range());
    tagMethod->setText("CaseTag");
    tagMethod->setTypeSyntax(makeTagType());
    tagMethod->setAuxiliaryText("variant-case-tag");
    markPublic(tagMethod);
    tagMethod->addModifier(TokenKind::KwReadonly);
    ast::AstNode* tagBody = makeBlock(name.range());
    tagBody->addChild(makeReturn(
        makeIdentifierExpression(name.range(), "__nsc_variant_tag"), name.range()));
    tagMethod->setFirst(tagBody);
    node->addChild(tagMethod);

    const bool explicitCopyVariant = node->auxiliaryText().find("copy") != std::string::npos;
    std::string metadata = "variant,move-only-value";
    if (explicitCopyVariant) metadata += ",copy";
    metadata += ",tag=" + tagTypeName + ",cases=";
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        if (index != 0) metadata += '|';
        metadata += cases[index].name;
        if (cases[index].payloadType != nullptr) metadata += ":payload";
    }
    node->setAuxiliaryText(std::move(metadata));
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseOpaqueTypeDeclaration(bool explicitCopy)
{
    const std::size_t beginIndex = index_;
    consume();
    if (!(at(TokenKind::Identifier) && current().lexeme() == "type"))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected 'type' after 'opaque'");
        synchronizeGlobal();
        return nullptr;
    }
    consume();

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "opaque type name");
        synchronizeGlobal();
        return nullptr;
    }
    const lexer::Token& name = consume();
    const std::string typeName(name.lexeme());
    knownTypeNames_.insert(typeName);

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StructDeclaration, name.range());
    node->setText(typeName);
    node->setAuxiliaryText(explicitCopy
        ? "opaque-type,move-only-value,copy"
        : "opaque-type,move-only-value");
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);

    if (!expect(TokenKind::Equal, "'='"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    if (!canStartTypeAt(index_))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "opaque type requires a representation type after '='");
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    ast::TypeSyntax* representation = parseType();
    parseGenericConstraintClauses(node);
    expect(TokenKind::Semicolon, "';'");

    const auto makeSelfType = [&]()
    {
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            name.range(), TokenKind::Identifier, typeName);
        for (const std::string& parameter : node->genericParameters())
        {
            type->addTypeArgument(arena_.make<ast::TypeSyntax>(
                name.range(), TokenKind::Identifier, parameter));
        }
        return type;
    };
    const auto markInternal = [](ast::AstNode* member)
    {
        member->setTokenKind(TokenKind::KwInternal);
        member->addModifier(TokenKind::KwInternal);
    };
    const auto makeReturn = [&](ast::AstNode* expression)
    {
        ast::AstNode* statement = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ReturnStatement, expression->range());
        statement->setFirst(expression);
        return statement;
    };
    const auto makeMove = [&](ast::AstNode* expression)
    {
        ast::AstNode* move = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, expression->range());
        move->setFirst(expression);
        return move;
    };

    constexpr std::string_view representationField = "__nsc_opaque_value";
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, name.range());
    field->setText(std::string(representationField));
    field->setTypeSyntax(representation);
    field->setAuxiliaryText("opaque-representation");
    field->setTokenKind(TokenKind::KwPrivate);
    field->addModifier(TokenKind::KwPrivate);
    node->addChild(field);

    ast::AstNode* from = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, name.range());
    from->setText("FromRepresentation");
    from->setTypeSyntax(makeSelfType());
    from->setAuxiliaryText("opaque-boundary:from");
    markInternal(from);
    from->addModifier(TokenKind::KwStatic);
    ast::AstNode* parameter = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ParameterDeclaration, name.range());
    parameter->setText("value");
    parameter->setTypeSyntax(representation);
    from->addChild(parameter);
    ast::AstNode* fromBody = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, name.range());
    ast::AstNode* defaultValue = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DefaultValueExpression, name.range());
    defaultValue->setTypeSyntax(makeSelfType());
    fromBody->addChild(makeVariableDeclaration(
        makeSelfType(), "__nsc_opaque_result", defaultValue, false));
    fromBody->addChild(makeExpressionStatement(makeAssignmentExpression(
        makeMemberExpression(
            makeIdentifierExpression(name.range(), "__nsc_opaque_result"),
            std::string(representationField)),
        makeIdentifierExpression(name.range(), "value"))));
    ast::AstNode* result = makeIdentifierExpression(name.range(), "__nsc_opaque_result");
    fromBody->addChild(makeReturn(explicitCopy ? result : makeMove(result)));
    from->setFirst(fromBody);
    node->addChild(from);

    ast::AstNode* to = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, name.range());
    to->setText("ToRepresentation");
    to->setTypeSyntax(representation);
    to->setAuxiliaryText("opaque-boundary:to");
    markInternal(to);
    to->addModifier(TokenKind::KwReadonly);
    ast::AstNode* toBody = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, name.range());
    toBody->addChild(makeReturn(makeIdentifierExpression(
        name.range(), std::string(representationField))));
    to->setFirst(toBody);
    node->addChild(to);

    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseStructBody(
    ast::AstNode* node,
    std::string_view structName,
    std::size_t beginIndex)
{
    if (node == nullptr) return nullptr;

    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member = nullptr;
        if (atContextualKeyword("associated") &&
            (peek().kind() == TokenKind::KwConst ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "type")))
        {
            consume();
            member = parseAssociatedItem(true);
        }
        else
        {
            member = parseClassMember(structName, true, true);
        }
        if (member != nullptr)
        {
            // C6 "struct members". This gate used to be ONE condition refusing constructors,
            // destructors, properties, indexers, operators, conversions, `protected` and every
            // virtual form at once, with a single "Stage 40 ... remain unsupported" message. That
            // lumped together three genuinely different things, and the difference is the whole
            // point:
            //
            //   * Some of them were only FORBIDDEN, never missing -- a property on a struct
            //     lowers to an ordinary instance method taking the receiver's address, which this
            //     compiler has emitted since Stage 40. Measured by lifting the gate: it compiled
            //     and called the getter correctly.
            //   * Some needed a small, bounded piece of work -- operators, indexers, constructors.
            //   * Two forms required explicit semantic contracts rather than merely lifting the
            //     old gate. Virtual value methods remain invalid; destructors are now admitted
            //     only for move-only structs, whose unique-owner/drop-flag model gives them a
            //     deterministic final-destruction point.
            //
            //       - a copy/union struct DESTRUCTOR still has no unique final owner and remains
            //         rejected. A move-only struct has one, and T06-F02 threads its destructor
            //         through value-drop cleanup.
            //       - `virtual`/`override`/`abstract`/`sealed` on a struct degrades to a DIRECT
            //         call. A value has no vtable pointer and no place to put one without changing
            //         its layout -- and its layout is the contract it crosses the C boundary with.
            //         Polymorphism is what classes are for.
            if (member->kind() == ast::AstNodeKind::DestructorDeclaration)
            {
                const std::string_view markers = node->auxiliaryText();
                const bool rawUnion = markers.find("union") != std::string_view::npos;
                if (rawUnion)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        member->range(),
                        "a raw union cannot declare a destructor because it has no active-member "
                        "discriminant");
                }
                else
                {
                    node->addChild(member);
                }
            }
            else if (member->kind() == ast::AstNodeKind::MethodDeclaration &&
                     (member->hasModifier(TokenKind::KwVirtual) ||
                      member->hasModifier(TokenKind::KwOverride) ||
                      member->hasModifier(TokenKind::KwAbstract) ||
                      member->hasModifier(TokenKind::KwSealed)))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "a struct method cannot be virtual, override, abstract or sealed: a value has "
                    "no vtable pointer, and adding one would change the layout the struct crosses "
                    "the C boundary with; use a class for polymorphism");
            }
            else if (member->tokenKind() == TokenKind::KwProtected)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "a struct member cannot be protected: structs are not inheritable, so there is "
                    "no derived type for protected to mean anything to");
            }
            else
            {
                node->addChild(member);
            }
        }
        if (index_ == before)
        {
            consume();
        }
    }

    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseClassDeclaration(
    bool isPartial,
    bool isAbstract,
    bool isSealed,
    bool isStatic)
{
    const std::size_t beginIndex = index_;
    consume();

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "class name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    const std::string className(name.lexeme());
    knownTypeNames_.insert(className);
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ClassDeclaration,
        name.range());
    node->setText(className);
    node->setFlag(isPartial);
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);
    if (isAbstract)
    {
        node->addModifier(TokenKind::KwAbstract);
    }
    if (isSealed)
    {
        node->addModifier(TokenKind::KwSealed);
    }
    if (isStatic)
    {
        node->addModifier(TokenKind::KwStatic);
    }

    // NS304-T09-F04: primary constructor de clase desuarado a campos + constructor (misma
    // semántica promote-to-fields que el struct/record primario de T02-F05). No hay captura
    // oculta ni ABI alternativo: los campos generados son el layout físico.
    if (at(TokenKind::LeftParen))
    {
        consume();
        std::vector<ast::AstNode*> parameters;
        std::unordered_set<std::string> names;
        if (!at(TokenKind::RightParen))
        {
            while (true)
            {
                TokenKind accessibility = TokenKind::Invalid;
                bool readonlyField = false;
                while (isAccessibilityToken(current().kind()) || at(TokenKind::KwReadonly))
                {
                    const lexer::Token& modifier = consume();
                    if (modifier.kind() == TokenKind::KwReadonly)
                    {
                        if (readonlyField)
                        {
                            diagnostics_.report(
                                diagnostics::DiagnosticId::ExpectedDeclaration,
                                diagnostics::DiagnosticSeverity::Error,
                                modifier.range(),
                                "duplicate readonly modifier on promoted field");
                        }
                        readonlyField = true;
                        continue;
                    }
                    if (accessibility != TokenKind::Invalid)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            modifier.range(),
                            "conflicting accessibility modifiers on promoted field");
                    }
                    else
                    {
                        accessibility = modifier.kind();
                    }
                }
                ast::AstNode* parameter = parseParameter();
                if (parameter != nullptr)
                {
                    if (!names.insert(parameter->text()).second)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::DuplicateSymbol,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "duplicate promoted field '" + parameter->text() + "'");
                    }
                    if (!parameter->auxiliaryText().empty())
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedDeclaration,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "a promoted parameter cannot be params, ref, out, or in");
                    }
                    const TokenKind fieldAccessibility = accessibility == TokenKind::Invalid
                        ? TokenKind::KwPrivate : accessibility;
                    ast::AstNode* field = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::FieldDeclaration, parameter->range());
                    field->setText(parameter->text());
                    field->setTypeSyntax(parameter->typeSyntax());
                    field->setAuxiliaryText("promoted-primary-field");
                    field->setTokenKind(fieldAccessibility);
                    field->addModifier(fieldAccessibility);
                    if (readonlyField) field->addModifier(TokenKind::KwReadonly);
                    node->addChild(field);
                    parameters.push_back(parameter);
                }
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')'");

        ast::AstNode* constructor = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ConstructorDeclaration,
            SourceRange(name.range().begin(), tokens_[index_ - 1].range().end()));
        constructor->setText(className);
        constructor->setAuxiliaryText("promoted-primary-constructor");
        constructor->setTokenKind(TokenKind::KwPublic);
        constructor->addModifier(TokenKind::KwPublic);
        ast::AstNode* body = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, constructor->range());
        for (ast::AstNode* parameter : parameters)
        {
            constructor->addChild(parameter);
            ast::AstNode* receiver = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ThisExpression, parameter->range());
            ast::AstNode* field = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MemberExpression, parameter->range());
            field->setText(parameter->text());
            field->setFirst(receiver);
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IdentifierExpression, parameter->range());
            value->setText(parameter->text());
            ast::AstNode* assignment = makeAssignmentExpression(field, value);
            body->addChild(makeExpressionStatement(assignment));
        }
        constructor->setFirst(body);
        node->addChild(constructor);

        std::string markers = node->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "primary-fields=" + std::to_string(parameters.size());
        node->setAuxiliaryText(std::move(markers));
    }

    if (consumeIf(TokenKind::Colon))
    {
        if (isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "a static class cannot specify a base class or interfaces");
        }
        bool first = true;
        do
        {
            if (!canStartTypeAt(index_))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedType,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "expected base class or interface type after ':'/','");
                break;
            }
            ast::TypeSyntax* specification = parseType();
            if (first)
            {
                node->setTypeSyntax(specification); // compatibility with pre-Stage36 class-base readers
                first = false;
            }
            node->addTypeSyntax(specification);
        }
        while (consumeIf(TokenKind::Comma));
    }

    // NSC-LIMIT-007: DESPUES de `: Base, IFoo` y antes del cuerpo, que es la posicion del
    // lenguaje. El rechazo que habia antes estaba DELANTE de la lista de bases, asi que
    // `class C<T> : Base where T : class` ni siquiera llegaba a leerse como clausula.
    //
    // NO hay una llamada equivalente en parseInterfaceDeclaration, y no es un olvido: una
    // interfaz de este lenguaje NO declara parametros genericos --no llama a
    // parseGenericParameterList-- asi que una clausula alli no podria nombrar nada. Poner la
    // llamada de todas formas seria codigo muerto que aparenta una cobertura que no existe.
    parseGenericConstraintClauses(node);

    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member = nullptr;
        if (atContextualKeyword("associated") &&
            (peek().kind() == TokenKind::KwConst ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "type")))
        {
            consume();
            member = parseAssociatedItem(true);
        }
        else
        {
            member = parseClassMember(className);
        }
        if (member != nullptr)
        {
            const bool nestedType = member->kind() == ast::AstNodeKind::ClassDeclaration ||
                member->kind() == ast::AstNodeKind::StructDeclaration ||
                member->kind() == ast::AstNodeKind::InterfaceDeclaration ||
                member->kind() == ast::AstNodeKind::EnumDeclaration ||
                member->kind() == ast::AstNodeKind::DelegateDeclaration;
            const bool staticMember = member->hasModifier(TokenKind::KwStatic) ||
                (member->kind() == ast::AstNodeKind::FieldDeclaration &&
                 member->hasModifier(TokenKind::KwConst)) ||
                member->kind() == ast::AstNodeKind::ExternCDeclaration;
            if (isStatic && !nestedType && !staticMember)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "a static class can contain only static members (nested types are allowed)");
            }
            node->addChild(member);
        }
        if (index_ == before)
        {
            consume();
        }
    }

    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseTraitImplementationDeclaration()
{
    const std::size_t beginIndex = index_;
    consume(); // contextual impl

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ClassDeclaration, tokens_[beginIndex].range());
    node->setAuxiliaryText("ns3-trait-impl");
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);

    ast::TypeSyntax* trait = parseType();
    if (trait == nullptr)
    {
        synchronizeGlobal();
        return node;
    }
    trait->setExplicitTraitImplementation();
    node->setTypeSyntax(trait);
    node->addTypeSyntax(trait);

    if (!expect(TokenKind::KwFor, "'for' after trait type in impl declaration"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    ast::TypeSyntax* target = parseType();
    node->setSecond(target == nullptr ? nullptr : arena_.make<ast::AstNode>(
        ast::AstNodeKind::IdentifierExpression, target->range()));
    if (node->second() != nullptr) node->second()->setTypeSyntax(target);

    if (target == nullptr || target->baseTokenKind() != TokenKind::Identifier ||
        target->isTuple() || target->isDynamicTrait() ||
        target->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
        !target->modifiers().empty() || target->baseName().find('.') != std::string::npos ||
        target->baseName().find(':') != std::string::npos)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            target == nullptr ? current().range() : target->range(),
            "an impl target must be a direct unqualified class type owned by this module");
    }
    else
    {
        node->setText(target->baseName());
    }

    parseGenericConstraintClauses(node);
    if (!expect(TokenKind::LeftBrace, "'{' after impl target"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member =
            ((atContextualKeyword("type") && peek().kind() == TokenKind::Identifier) ||
             at(TokenKind::KwConst))
            ? parseAssociatedItem(true)
            : parseClassMember(node->text());
        if (member != nullptr)
        {
            const bool associated =
                member->kind() == ast::AstNodeKind::AssociatedTypeDeclaration ||
                member->kind() == ast::AstNodeKind::AssociatedConstantDeclaration;
            const bool allowed = associated ||
                member->kind() == ast::AstNodeKind::MethodDeclaration ||
                member->kind() == ast::AstNodeKind::PropertyDeclaration ||
                member->kind() == ast::AstNodeKind::IndexerDeclaration;
            if (!allowed)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "an impl block may contain only methods, properties, indexers, and operators");
            }
            else if (associated)
            {
                node->addChild(member);
            }
            else
            {
                if (member->tokenKind() == TokenKind::Invalid)
                {
                    member->setTokenKind(TokenKind::KwPublic);
                    member->addModifier(TokenKind::KwPublic);
                }
                if (member->tokenKind() != TokenKind::KwPublic)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        member->range(),
                        "members of an impl block are public trait witnesses");
                }
                node->addChild(member);
            }
        }
        if (index_ == before) consume();
    }
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseInterfaceDeclaration(bool isPartial, bool isTrait)
{
    const std::size_t beginIndex = index_;
    consume(); // interface / contextual trait

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "interface name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    knownTypeNames_.insert(std::string(name.lexeme()));
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::InterfaceDeclaration, name.range());
    node->setText(std::string(name.lexeme()));
    node->setFlag(isPartial);
    if (isTrait) node->setAuxiliaryText("ns3-trait");
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);

    if (consumeIf(TokenKind::Colon))
    {
        do
        {
            if (!canStartTypeAt(index_))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedType,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "expected base interface type after ':'/','");
                break;
            }
            ast::TypeSyntax* specification = parseType();
            if (node->typeSyntax() == nullptr)
            {
                node->setTypeSyntax(specification);
            }
            node->addTypeSyntax(specification);
        }
        while (consumeIf(TokenKind::Comma));
    }

    // Generic interfaces share declaration-site constraints with every other generic type.
    // The monomorphizer cannot recover clauses that were never represented in the AST.
    parseGenericConstraintClauses(node);

    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* parsedMember = isTrait &&
            ((atContextualKeyword("type") && peek().kind() == TokenKind::Identifier) ||
             at(TokenKind::KwConst))
            ? parseAssociatedItem(false)
            : parseInterfaceMember(isTrait);
        if (ast::AstNode* member = parsedMember; member != nullptr)
        {
            if (isTrait && member->kind() == ast::AstNodeKind::MethodDeclaration &&
                !member->genericParameters().empty())
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "generic trait methods are not object-safe in the current T05 vertical; "
                    "move the type parameter to the trait declaration");
            }
            node->addChild(member);
        }
        if (index_ == before)
        {
            consume();
        }
    }
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseAssociatedItem(bool implementation)
{
    const std::size_t beginIndex = index_;
    const bool associatedType = atContextualKeyword("type");
    consume(); // contextual type / const

    ast::TypeSyntax* declaredType = nullptr;
    if (!associatedType)
    {
        declaredType = parseType();
        if (declaredType == nullptr)
        {
            synchronizeStatement();
            return nullptr;
        }
    }
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier,
            associatedType ? "associated type name" : "associated constant name");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token& name = consume();
    ast::AstNode* item = arena_.make<ast::AstNode>(
        associatedType ? ast::AstNodeKind::AssociatedTypeDeclaration
                       : ast::AstNodeKind::AssociatedConstantDeclaration,
        name.range());
    item->setText(std::string(name.lexeme()));

    bool hasDefinition = false;
    if (consumeIf(TokenKind::Equal))
    {
        hasDefinition = true;
        if (associatedType)
            item->setTypeSyntax(parseType());
        else
            item->setFirst(parseExpression());
    }
    else if (!associatedType)
    {
        item->setTypeSyntax(declaredType);
    }
    if (!associatedType && hasDefinition) item->setTypeSyntax(declaredType);

    if (implementation && !hasDefinition)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            item->range(),
            associatedType
                ? "an impl must define an associated type with '= Type'"
                : "an impl must define an associated constant with '= expression'");
    }
    if (associatedType && hasDefinition && item->typeSyntax() == nullptr)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected associated type definition after '='");
    }
    expect(TokenKind::Semicolon, "';' after associated item");
    item->setRange(tokenRangeFrom(beginIndex));
    return item;
}

ast::AstNode* Parser::parseInterfaceMember(bool isTrait)
{
    const std::size_t beginIndex = index_;
    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador async se baja en
        // el callable cuando el punto lo permite (follow-up para class/interface member).
        consume();
    }
    TokenKind accessibility = TokenKind::KwPublic;
    bool sawPublic = false;
    bool sawAbstract = false;
    bool sawNoHeap = false;
    bool sawStatic = false;

    while (at(TokenKind::KwPublic) || at(TokenKind::KwAbstract) || at(TokenKind::KwNoHeap) ||
           at(TokenKind::KwStatic))
    {
        const TokenKind modifier = consume().kind();
        if (modifier == TokenKind::KwPublic)
        {
            if (sawPublic)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "duplicate public modifier on interface member");
            }
            sawPublic = true;
        }
        else if (modifier == TokenKind::KwAbstract)
        {
            if (sawAbstract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "duplicate abstract modifier on interface member");
            }
            sawAbstract = true;
        }
        else if (modifier == TokenKind::KwNoHeap)
        {
            if (sawNoHeap)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "duplicate noheap modifier on interface member");
            }
            sawNoHeap = true;
        }
        else
        {
            if (sawStatic)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "duplicate static modifier on trait member");
            }
            sawStatic = true;
            if (!isTrait)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "static interface members require a NativeSharp 3 trait declaration");
            }
        }
    }

    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador async se baja en
        // el callable cuando el punto lo permite (follow-up para class/interface member).
        consume();
    }

    const bool isReferenceReturn = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1U) || peek().kind() == TokenKind::LeftBracket);
    std::string returnLifetime;
    if (isReferenceReturn)
    {
        consume();
        returnLifetime = parseOptionalLifetimeAnnotation("ref return");
    }

    // A generic parameter is intentionally not a globally known type name. As in class members,
    // accept an identifier-shaped type here and let name lookup decide whether it names this
    // interface's parameter or is genuinely unknown.
    // A method-scoped generic parameter is declared after the method name, so the return type
    // lookahead cannot know `T` yet in `owned T Transfer<T>(...)`.  The ownership keyword is
    // nevertheless an unambiguous type prefix; let parseType consume it and preserve the same
    // unresolved-identifier path already allowed immediately below for ordinary `T Method<T>`.
    if (!canStartTypeAt(index_) && !at(TokenKind::Identifier) &&
        !lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "Stage 38 interfaces contain method, property, or indexer contracts only");
        synchronizeStatement();
        return nullptr;
    }
    ast::TypeSyntax* returnType = parseType();

    if (at(TokenKind::KwOperator))
    {
        if (!isTrait)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "operator requirements are valid only in a NativeSharp 3 trait");
        }
        ast::AstNode* requirement = parseOperatorTail(
            returnType, accessibility, sawStatic, beginIndex, isTrait);
        if (requirement != nullptr && requirement->first() != nullptr)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                requirement->range(),
                "trait operator requirements declare no body; provide the implementation on "
                "the conforming type");
        }
        return requirement;
    }

    if (at(TokenKind::KwThis))
    {
        return parseIndexerTail(
            returnType, accessibility, false, false, false, sawAbstract, false,
            isReferenceReturn, beginIndex, true);
    }

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "interface member name");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token& name = consume();
    if (at(TokenKind::LeftParen) || at(TokenKind::Less))
    {
        // NS304-T02-F01: every interface (not just a trait) accepts bodyless methods. With ';'
        // parseMethodTail marks the method abstract implicitly (allowBodyless); with '{...}' it
        // keeps the default implementation. isTrait must not gate that permission, otherwise an
        // ordinary `interface I { int Read(); }` is misread as a concrete bodyless class method.
        ast::AstNode* method = parseMethodTail(
            returnType, name, accessibility,
            false, sawStatic, false, false, sawAbstract,
            false, false, false, false, false, false,
            sawNoHeap, false, beginIndex, true);
        if (method != nullptr)
        {
            method->setReference(isReferenceReturn);
            method->setLifetimeName(std::move(returnLifetime));
            if (isTrait && sawStatic)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    method->range(),
                    "ordinary static trait methods are not part of T05-F01; declare a static "
                    "property or operator requirement");
            }
        }
        return method;
    }
    if (at(TokenKind::LeftBrace))
    {
        if (isReferenceReturn)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "ref return is valid only on a method in item 313");
        }
        return parsePropertyTail(
            returnType, name, accessibility,
            false, sawStatic, false, false, sawAbstract, false, beginIndex, true);
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        name.range(),
        "Stage 38 interfaces do not contain fields; expected method, property, or indexer contract");
    synchronizeStatement();
    return nullptr;
}

// NSC-LIMIT-009. `union` es contextual --sigue siendo un identificador corriente en todas partes--
// asi que se reconoce igual que en `parseGlobalDeclaration`: por lo que viene detras.
bool Parser::atNestedTypeDeclaration() const noexcept
{
    return at(TokenKind::KwClass) || at(TokenKind::KwStruct) || at(TokenKind::KwInterface) ||
        at(TokenKind::KwEnum) || at(TokenKind::KwDelegate) ||
        (atContextualKeyword("union") && peek().kind() == TokenKind::Identifier) ||
        (atContextualKeyword("record") && peek().kind() == TokenKind::KwStruct) ||
        (atContextualKeyword("variant") && peek().kind() == TokenKind::Identifier);
}

ast::AstNode* Parser::parseNestedTypeDeclaration(
    const NestedTypeModifiers& modifiers,
    std::size_t beginIndex)
{
    // Lo que NO significa nada en una declaracion de tipo. Se rechaza cada uno por su nombre en
    // vez de un «modificador no valido» generico: quien escribio `static class Item` dentro de
    // una clase viene de C#, donde eso SI existe, y necesita saber que aqui no -- no que su
    // linea es ilegible.
    const auto refuse = [&](const char* modifierName, const char* reason)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[beginIndex].range(),
            std::string("'") + modifierName + "' is not valid on a nested type declaration: " + reason);
    };
    const bool nestedClass = at(TokenKind::KwClass);
    const bool nestedRecordStruct =
        atContextualKeyword("record") && peek().kind() == TokenKind::KwStruct;
    const bool nestedStruct = at(TokenKind::KwStruct) || nestedRecordStruct;
    if (modifiers.isPartial && !nestedClass)
        refuse("partial", "only nested classes can be split into materialization parts");
    if (modifiers.isStatic && !nestedClass)
        refuse("static", "only a nested class can be static");
    const bool nestedReadonlyStruct = modifiers.isReadonly && nestedStruct;
    if (modifiers.isReadonly && !nestedReadonlyStruct)
        refuse("readonly", "only a non-union struct can be readonly");
    if (modifiers.isConst) refuse("const", "a type is not a value");
    if (modifiers.isVirtual) refuse("virtual", "a type is not dispatched");
    if (modifiers.isOverride) refuse("override", "a type does not override anything");
    if (modifiers.isExported || modifiers.isExternC)
    {
        refuse("export extern C", "the host C ABI exports functions, not types");
    }
    if (modifiers.hasExplicitFieldOffset)
    {
        refuse("at(N)", "a type has no offset inside its container; only a field does");
    }

    // `abstract` y `sealed` SI significan algo, pero solo en una clase. Se dejan pasar y el
    // parser de clase los aplica; en cualquier otra forma se rechazan aqui, que es donde se sabe
    // cual de las cinco se esta abriendo.
    if ((modifiers.isAbstract || modifiers.isSealed) && !nestedClass)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[beginIndex].range(),
            "abstract/sealed modifiers are supported only for class declarations");
    }

    ast::AstNode* declaration = nullptr;
    if (at(TokenKind::KwClass))
    {
        declaration = parseClassDeclaration(
            modifiers.isPartial, modifiers.isAbstract, modifiers.isSealed, modifiers.isStatic);
    }
    else if (at(TokenKind::KwInterface))
    {
        declaration = parseInterfaceDeclaration(false);
    }
    else if (atContextualKeyword("variant"))
    {
        declaration = parseVariantDeclaration();
    }
    else if (nestedStruct || atContextualKeyword("union"))
    {
        if (nestedRecordStruct)
        {
            consume();
        }
        declaration = parseStructDeclaration(
            at(TokenKind::KwStruct) ? StructLayoutForm::Struct : StructLayoutForm::Union,
            modifiers.isBorrowedView && at(TokenKind::KwStruct),
            nestedRecordStruct,
            modifiers.isCopyType);
    }
    else if (at(TokenKind::KwEnum))
    {
        declaration = parseEnumDeclaration();
    }
    else
    {
        declaration = parseDelegateDeclaration();
    }

    if (declaration == nullptr)
    {
        return nullptr;
    }

    if (modifiers.isCopyType)
    {
        if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
        {
            refuse("copy", "only nested structs, records, and variants can opt into Copy");
        }
        else if (declaration->auxiliaryText().find("copy") == std::string::npos)
        {
            std::string markers = declaration->auxiliaryText();
            markers += (markers.empty() ? "" : ",");
            markers += "copy";
            declaration->setAuxiliaryText(std::move(markers));
        }
    }

    // NSC-LIMIT-009 -- LO QUE ESTA ENTREGA NO HACE, dicho en el parser y no en silencio.
    //
    // Un tipo anidado GENERICO exigiria que la identidad canonica llevara la aridad de CADA
    // segmento de la cadena --`Outer`2+Item`1`-- y que la materializacion generica clonara un
    // cuerpo que a su vez declara tipos. Ninguna de las dos cosas esta escrita, y una identidad
    // con aridad perdida no es un limite: es una colision silenciosa entre `Outer<T>.Item` y
    // `Outer.Item`. Se rechaza, que es lo unico honesto que se puede hacer hoy.
    if (!declaration->genericParameters().empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a nested type cannot declare generic parameters yet; declare '" +
                declaration->text() + "' at namespace scope");
    }

    if (modifiers.accessibility != TokenKind::Invalid)
    {
        declaration->setTokenKind(modifiers.accessibility);
    }
    if (modifiers.isNew)
    {
        declaration->addModifier(TokenKind::KwNew);
    }
    if (nestedReadonlyStruct)
    {
        declaration->addModifier(TokenKind::KwReadonly);
        for (ast::AstNode* member : declaration->children())
        {
            if (member != nullptr && member->kind() == ast::AstNodeKind::FieldDeclaration &&
                member->auxiliaryText() == "promoted-primary-field" &&
                !member->hasModifier(TokenKind::KwReadonly))
            {
                member->addModifier(TokenKind::KwReadonly);
            }
        }
    }
    if (modifiers.layoutPack.has_value() || modifiers.layoutAlign.has_value() ||
        modifiers.layoutSize.has_value())
    {
        if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "pack/align/size layout modifiers apply only to nested struct and union declarations");
        }
        else if (declaration->auxiliaryText().find("variant") != std::string::npos)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration->range(),
                "pack/align/size cannot override the canonical tagged-variant layout");
        }
        else
        {
            std::string layout = declaration->auxiliaryText();
            const auto appendLayout = [&](std::string_view name, const std::optional<std::uint64_t> value)
            {
                if (!value.has_value()) return;
                layout += (layout.empty() ? "" : ",");
                layout += std::string(name) + "=" + std::to_string(*value);
            };
            appendLayout("pack", modifiers.layoutPack);
            appendLayout("align", modifiers.layoutAlign);
            appendLayout("size", modifiers.layoutSize);
            declaration->setAuxiliaryText(std::move(layout));
        }
    }
    if (beginIndex < tokens_.size())
    {
        declaration->setRange(join(tokens_[beginIndex].range(), declaration->range()));
    }
    return declaration;
}

ast::AstNode* Parser::parseClassMember(
    std::string_view className,
    bool allowUnresolvedNamedTypes,
    bool structContext)
{
    (void)allowUnresolvedNamedTypes;
    const std::size_t beginIndex = index_;
    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador async se baja en
        // el callable cuando el punto lo permite (follow-up para class/interface member).
        consume();
    }
    TokenKind accessibility = TokenKind::Invalid;
    bool isPartial = false;
    bool isNew = false;
    bool isStatic = false;
    bool isReadonly = false;
    bool isVolatile = false;
    bool isConst = false;
    bool isVirtual = false;
    bool isOverride = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isExported = false;
    bool isExternC = false;
    bool isInline = false;
    bool isForceInline = false;
    bool isNoInline = false;
    bool isNoHeap = false;
    bool isRequired = false;
    bool isUnsafe = false;
    bool isControlledSpecialization = false;
    SourceRange controlledSpecializationRange{};
    bool isBorrowedView = false;
    bool isCopyType = false;
    std::optional<std::uint64_t> nestedLayoutPack;
    std::optional<std::uint64_t> nestedLayoutAlign;
    std::optional<std::uint64_t> nestedLayoutSize;
    // C2.2 "union and explicit layout": `at(N)` gives a field its own byte offset inside the
    // aggregate -- the NativeSharp spelling of C's hand-laid-out records and of C#'s
    // [FieldOffset(N)]. Contextual, like `pack`/`align`, so `at` stays usable as an ordinary
    // name everywhere else.
    std::optional<std::uint64_t> explicitFieldOffset;
    // Accepted on EITHER side of the accessibility/static/readonly modifiers, because both
    // `at(4) public int x;` and `public at(4) int x;` read naturally and neither ordering is
    // more correct than the other.
    const auto parseExplicitFieldOffset = [&]()
    {
        while (atContextualKeyword("at") && peek().kind() == TokenKind::LeftParen)
        {
            const lexer::Token& keyword = consume();
            consume();
            if (at(TokenKind::IntegerLiteral) &&
                std::holds_alternative<std::uint64_t>(current().value()))
            {
                explicitFieldOffset = std::get<std::uint64_t>(current().value());
                consume();
            }
            else
            {
                expect(TokenKind::IntegerLiteral, "field byte offset");
            }
            expect(TokenKind::RightParen, "')'");
            if (explicitFieldOffset.has_value() && *explicitFieldOffset > 0xFFFFFFULL)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    keyword.range(),
                    "explicit field offset is out of range");
                explicitFieldOffset.reset();
            }
        }
    };
    const auto rejectControlledSpecialization = [&](std::string_view declarationKind)
    {
        if (!isControlledSpecialization) return;
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            controlledSpecializationRange,
            "specialize applies only to an ordinary generic method, not " +
                std::string(declarationKind));
    };
    // `at(N)` may appear anywhere among the modifiers -- before them, after them, or between two
    // of them -- because no ordering of `public`, `static` and `at(4)` is more correct than
    // another and a parser that accepted only one of them would be an arbitrary rule to remember.
    while (true)
    {
        parseExplicitFieldOffset();
        if (atContextualKeyword("unsafe"))
        {
            std::size_t afterType = index_ + 1U;
            const bool methodModifier = scanTypeAt(index_ + 1U, afterType) &&
                afterType + 1U < tokens_.size() &&
                tokens_[afterType].kind() == TokenKind::Identifier &&
                (tokens_[afterType + 1U].kind() == TokenKind::LeftParen ||
                 tokens_[afterType + 1U].kind() == TokenKind::Less);
            if (methodModifier)
            {
                const lexer::Token& modifier = consume();
                if (isUnsafe)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        modifier.range(),
                        "duplicate unsafe modifier on method");
                }
                isUnsafe = true;
                continue;
            }
        }
        if (atContextualKeyword("specialize"))
        {
            const lexer::Token& modifier = consume();
            if (isControlledSpecialization)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate specialize modifier on class member");
            }
            else
            {
                isControlledSpecialization = true;
                controlledSpecializationRange = modifier.range();
            }
            continue;
        }
        const bool partialNestedModifier = atContextualKeyword("partial") &&
            (peek().kind() == TokenKind::KwClass || peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal || peek().kind() == TokenKind::KwProtected ||
             peek().kind() == TokenKind::KwPrivate || peek().kind() == TokenKind::KwStatic ||
             peek().kind() == TokenKind::KwAbstract || peek().kind() == TokenKind::KwSealed);
        if (partialNestedModifier)
        {
            const lexer::Token& modifier = consume();
            if (isPartial)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate 'partial' modifier on nested type declaration");
            }
            isPartial = true;
            continue;
        }
        if ((atContextualKeyword("pack") || atContextualKeyword("align") ||
             atContextualKeyword("size")) && peek().kind() == TokenKind::LeftParen)
        {
            const bool isPack = atContextualKeyword("pack");
            const bool isAlign = atContextualKeyword("align");
            const lexer::Token& keyword = consume();
            consume();
            std::optional<std::uint64_t> value;
            if (at(TokenKind::IntegerLiteral) &&
                std::holds_alternative<std::uint64_t>(current().value()))
            {
                value = std::get<std::uint64_t>(current().value());
                consume();
            }
            else
            {
                expect(TokenKind::IntegerLiteral, isPack ? "pack value" : isAlign ? "alignment value" : "size value");
            }
            expect(TokenKind::RightParen, "')'");
            if (value.has_value() && ((isPack || isAlign)
                    ? (*value == 0 || (*value & (*value - 1)) != 0)
                    : (*value == 0)))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    keyword.range(),
                    isPack ? "pack requires a power-of-two byte value"
                           : isAlign ? "align requires a power-of-two byte value"
                                     : "size requires a positive byte count");
                value.reset();
            }
            if (isPack) nestedLayoutPack = value;
            else if (isAlign) nestedLayoutAlign = value;
            else nestedLayoutSize = value;
            continue;
        }
        if (at(TokenKind::KwBorrowed) &&
            (peek().kind() == TokenKind::KwStruct ||
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "copy")))
        {
            const lexer::Token& modifier = consume();
            if (isBorrowedView)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate borrowed modifier on nested type");
            }
            isBorrowedView = true;
            continue;
        }
        const bool copyNestedModifier = atContextualKeyword("copy") &&
            (peek().kind() == TokenKind::KwStruct || peek().kind() == TokenKind::KwClass ||
             peek().kind() == TokenKind::KwEnum || peek().kind() == TokenKind::KwDelegate ||
             peek().kind() == TokenKind::KwPublic ||
             peek().kind() == TokenKind::KwInternal || peek().kind() == TokenKind::KwProtected ||
             peek().kind() == TokenKind::KwPrivate || peek().kind() == TokenKind::KwReadonly ||
             (peek().kind() == TokenKind::Identifier &&
              (peek().lexeme() == "pack" || peek().lexeme() == "align" ||
               peek().lexeme() == "size" || peek().lexeme() == "record" ||
               peek().lexeme() == "variant")));
        if (copyNestedModifier)
        {
            const lexer::Token& modifier = consume();
            if (isCopyType)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate copy modifier on nested value type");
            }
            isCopyType = true;
            continue;
        }
        if (!(at(TokenKind::KwPublic) || at(TokenKind::KwInternal) ||
              at(TokenKind::KwProtected) || at(TokenKind::KwPrivate) ||
              at(TokenKind::KwNew) || at(TokenKind::KwStatic) ||
              at(TokenKind::KwReadonly) || at(TokenKind::KwConst) ||
              at(TokenKind::KwVolatile) ||
              at(TokenKind::KwInline) || at(TokenKind::KwForceInline) ||
              at(TokenKind::KwNoInline) || at(TokenKind::KwNoHeap) ||
              at(TokenKind::KwVirtual) || at(TokenKind::KwOverride) ||
              at(TokenKind::KwAbstract) || at(TokenKind::KwSealed) ||
              at(TokenKind::KwExport) || at(TokenKind::KwExtern) ||
              atContextualKeyword("required")))
        {
            break;
        }
        {
        const lexer::Token& modifier = consume();
        if (modifier.kind() == TokenKind::KwNew)
        {
            if (isNew)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate new modifier on class member");
            }
            isNew = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwVirtual)
        {
            if (isVirtual)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate virtual modifier on class member");
            }
            isVirtual = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwStatic)
        {
            if (isStatic)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate static modifier on class member");
            }
            isStatic = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwReadonly)
        {
            if (isReadonly)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate readonly modifier on class member");
            }
            isReadonly = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwVolatile)
        {
            if (isVolatile)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate volatile modifier on class member");
            }
            isVolatile = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwInline)
        {
            if (isInline)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate inline modifier on class member");
            }
            isInline = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwForceInline)
        {
            if (isForceInline)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate forceinline modifier on class member");
            }
            isForceInline = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwNoInline)
        {
            if (isNoInline)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate noinline modifier on class member");
            }
            isNoInline = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwNoHeap)
        {
            if (isNoHeap)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate noheap modifier on class member");
            }
            isNoHeap = true;
            continue;
        }
        if (modifier.lexeme() == "required")
        {
            // NS304-T09-F01: `required` es un modificador contextual de campo/property.
            if (isRequired)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate required modifier on class member");
            }
            isRequired = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwConst)
        {
            if (isConst)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate const modifier on class member");
            }
            isConst = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwAbstract)
        {
            if (isAbstract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate abstract modifier on class member");
            }
            isAbstract = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwSealed)
        {
            if (isSealed)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate sealed modifier on class member");
            }
            isSealed = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwOverride)
        {
            if (isOverride)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate override modifier on class member");
            }
            isOverride = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwExport)
        {
            if (isExported)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate export modifier on class member");
            }
            isExported = true;
            continue;
        }
        if (modifier.kind() == TokenKind::KwExtern)
        {
            if (isExternC)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    "duplicate extern C modifier on class member");
            }
            expectContextualKeyword("C", "'C' after extern");
            // Interop Foundation (Stage 55.x, generic FFI): `extern C from "<dll>" { ... }` as a
            // CLASS-MEMBER block declares imported C functions. Each declaration inside becomes
            // a body-less static method of the enclosing class -- callable as ClassName.Fn(...)
            // through the ordinary member machinery -- whose C symbol is the member name and
            // whose (dll, symbol) pair flows to the linker so the import library materializes
            // without the compiler learning any API by name. `from` is a contextual identifier,
            // not a keyword: no existing program changes meaning.
            // Two spellings, one machinery:
            //   extern C from "user32.dll" { ... }  -> DYNAMIC imports; the (dll, symbol) pairs
            //       materialize an import library at link time.
            //   extern C { ... }                    -> STATIC-RESOLVED imports; declarations
            //       only, no fabricated import library -- the symbols must come from the link
            //       inputs (--link-lib foo.lib), which is exactly how a clang-built static
            //       archive is consumed. MEASURED before this split existed: routing a static
            //       .lib through the dynamic path minted import thunks against a DLL that does
            //       not exist, and the loader failed at runtime (exit 53) while the link
            //       "succeeded".
            if ((at(TokenKind::Identifier) && current().lexeme() == "from") || at(TokenKind::LeftBrace))
            {
                rejectControlledSpecialization("an extern C import block");
                if (isInline || isForceInline || isNoInline || isNoHeap)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        modifier.range(),
                        "inline cannot modify an extern C import block");
                }
                const bool dynamicImport = at(TokenKind::Identifier);
                if (dynamicImport) consume();
                std::string dllName;
                if (dynamicImport)
                {
                if (at(TokenKind::StringLiteral))
                {
                    dllName = std::string(current().lexeme());
                    // The lexeme carries its surrounding quotes; the DLL name must not.
                    if (dllName.size() >= 2 && dllName.front() == '"' && dllName.back() == '"')
                    {
                        dllName = dllName.substr(1, dllName.size() - 2);
                    }
                    consume();
                }
                else
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedToken,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "extern C from requires a DLL name string, e.g. extern C from \"user32.dll\"");
                }
                }
                ast::AstNode* block = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::ExternCDeclaration, tokenRangeFrom(beginIndex));
                block->setText(dllName);
                expect(TokenKind::LeftBrace, "'{'");
                while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
                {
                    const std::size_t entryBegin = index_;
                    TokenKind entryAccessibility = TokenKind::KwPublic;
                    if (isAccessibilityToken(current().kind()))
                    {
                        entryAccessibility = consume().kind();
                    }
                    consumeIf(TokenKind::KwStatic); // implicit; tolerated for symmetry
                    ast::TypeSyntax* returnType = parseType();
                    if (!at(TokenKind::Identifier))
                    {
                        expect(TokenKind::Identifier, "imported C function name");
                        synchronizeStatement();
                        if (at(TokenKind::RightBrace)) break;
                        continue;
                    }
                    const lexer::Token& importName = consume();
                    ast::AstNode* imported = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::MethodDeclaration, importName.range());
                    imported->setText(std::string(importName.lexeme()));
                    imported->setAuxiliaryText("cimport:" + dllName);
                    imported->setTypeSyntax(returnType);
                    imported->setTokenKind(entryAccessibility);
                    imported->addModifier(entryAccessibility);
                    imported->addModifier(TokenKind::KwStatic);
                    imported->addModifier(TokenKind::KwExtern);
                    expect(TokenKind::LeftParen, "'('");
                    bool sawFixedParameter = false;
                    bool variadic = false;
                    if (!at(TokenKind::RightParen))
                    {
                        while (true)
                        {
                            if (at(TokenKind::Ellipsis))
                            {
                                if (!sawFixedParameter)
                                {
                                    diagnostics_.report(
                                        diagnostics::DiagnosticId::ExpectedDeclaration,
                                        diagnostics::DiagnosticSeverity::Error,
                                        current().range(),
                                        "a variadic C declaration requires at least one fixed parameter before '...'");
                                }
                                consume();
                                variadic = true;
                                break;
                            }
                            ast::AstNode* parameter = parseParameter();
                            if (parameter != nullptr)
                            {
                                imported->addChild(parameter);
                                sawFixedParameter = true;
                            }
                            if (!consumeIf(TokenKind::Comma)) break;
                        }
                    }
                    imported->setVariadic(variadic);
                    expect(TokenKind::RightParen, "')'");
                    parseFfiUnwindClause(imported);
                    expect(TokenKind::Semicolon, "';' (imported C functions have no body)");
                    imported->setRange(tokenRangeFrom(entryBegin));
                    block->addChild(imported);
                }
                expect(TokenKind::RightBrace, "'}'");
                block->setRange(tokenRangeFrom(beginIndex));
                return block;
            }
            isExternC = true;
            continue;
        }

        if (accessibility != TokenKind::Invalid)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                modifier.range(),
                accessibility == modifier.kind()
                    ? "duplicate accessibility modifier on class member"
                    : "conflicting accessibility modifiers on class member");
        }
        else
        {
            accessibility = modifier.kind();
        }
        }
    }

    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador async se baja en
        // el callable cuando el punto lo permite (follow-up para class/interface member).
        consume();
    }

    if (isVirtual && isOverride)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a method cannot be declared both virtual and override");
    }
    if (isNew && isOverride)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a member cannot be both new and override");
    }
    if (isStatic && (isVirtual || isOverride || isAbstract || isSealed))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "static methods cannot be virtual, override, abstract or sealed");
    }
    if (isVirtual && isAbstract)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "abstract methods are implicitly virtual; do not combine abstract with virtual");
    }
    if (isSealed && !isOverride)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "sealed is valid on a method only together with override");
    }
    if (isAbstract && isSealed)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a method cannot be both abstract and sealed");
    }
    if (isInline && isAbstract)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "inline requires a method body and cannot modify an abstract method");
    }
    if (isForceInline && isAbstract)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "forceinline requires a method body and cannot modify an abstract method");
    }
    if (isNoInline && isAbstract)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "noinline requires a method body and cannot modify an abstract method");
    }
    if (isVolatile && isReadonly)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a field cannot be both volatile and readonly");
    }
    if ((static_cast<unsigned>(isInline) + static_cast<unsigned>(isForceInline) +
         static_cast<unsigned>(isNoInline)) > 1U)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "inline, forceinline, and noinline are contradictory inlining policies");
    }
    if (isConst && (isReadonly || isStatic || isVolatile))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const already has static immutable semantics; do not combine const with static, readonly, or volatile");
    }
    if (isExported != isExternC)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "Stage 47 host ABI requires the paired modifier sequence 'export extern C'");
    }

    if (at(TokenKind::Tilde))
    {
        rejectControlledSpecialization("a destructor");
        if (accessibility != TokenKind::Invalid || isNew || isStatic || isReadonly || isVolatile || isConst ||
            isInline || isForceInline || isNoInline || isNoHeap ||
            isVirtual || isOverride || isAbstract || isSealed || isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "NativeSharp destructors do not accept accessibility or member modifiers");
        }
        return parseDestructorTail(className, beginIndex);
    }

    if (at(TokenKind::Identifier) && current().lexeme() == className &&
        peek().kind() == TokenKind::LeftParen)
    {
        rejectControlledSpecialization("a constructor");
        if (isStatic)
        {
            if (accessibility != TokenKind::Invalid)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "static constructors cannot declare accessibility");
            }
            if (isNew || isVirtual || isOverride || isReadonly || isVolatile || isConst ||
                isInline || isForceInline || isNoInline || isAbstract || isSealed ||
                isExported || isExternC)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "static constructors accept only the static and optional noheap modifiers");
            }
        }
        else if (isNew || isVirtual || isOverride || isReadonly || isVolatile || isConst ||
            isInline || isForceInline || isNoInline || isAbstract || isSealed ||
            isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "constructors cannot be readonly, volatile, const, virtual, override, abstract or sealed");
        }
        return parseConstructorTail(className, accessibility, isStatic, isNoHeap, beginIndex);
    }

    if (at(TokenKind::KwImplicit) || at(TokenKind::KwExplicit))
    {
        rejectControlledSpecialization("a conversion operator");
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isNew || isReadonly || isVolatile || isConst || isInline || isForceInline || isNoInline || isNoHeap ||
            isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "conversion operators cannot be readonly, volatile, const, virtual, override, abstract or sealed");
        }
        const bool implicitConversion = at(TokenKind::KwImplicit);
        return parseConversionOperatorTail(
            implicitConversion, accessibility, isStatic, beginIndex);
    }

    if (at(TokenKind::KwEvent))
    {
        rejectControlledSpecialization("an event");
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isNew || isReadonly || isVolatile || isConst || isInline || isForceInline || isNoInline || isNoHeap ||
            isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "events support accessibility and optional static only; volatile applies only to fields");
        }
        return parseEventTail(accessibility, isNew, isStatic, beginIndex);
    }

    // Items 324-325. `struct/union { ... } field;` is one source declaration with two semantic identities:
    // an inline value type that owns the physical layout, and an ordinary field in the containing
    // type. Keep the synthesized declaration as the direct child so name lookup can predeclare it
    // exactly like every other nested struct. Its first() edge carries the outer field; the actual
    // anonymous-aggregate members remain children(), so collecting the synthesized type cannot
    // accidentally give it a recursive field of its own.
    const bool anonymousStruct = at(TokenKind::KwStruct) && peek().kind() == TokenKind::LeftBrace;
    const bool anonymousUnion = atContextualKeyword("union") && peek().kind() == TokenKind::LeftBrace;
    if (anonymousStruct || anonymousUnion)
    {
        rejectControlledSpecialization("an anonymous aggregate field");
        const lexer::Token& aggregateKeyword = consume();
        const std::string synthesizedName =
            std::string(anonymousUnion ? "__nsc_anonymous_union_f" : "__nsc_anonymous_struct_f") +
            std::to_string(aggregateKeyword.range().begin().fileId().value()) + "_" +
            std::to_string(aggregateKeyword.range().begin().offset());
        knownTypeNames_.insert(synthesizedName);

        if (isPartial || isVirtual || isOverride || isAbstract || isSealed ||
            isExported || isExternC || isInline || isForceInline || isNoInline || isNoHeap ||
            isBorrowedView)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                aggregateKeyword.range(),
                "an anonymous aggregate field accepts field accessibility, new, static, readonly, "
                "volatile, const and layout modifiers only");
        }
        if (structContext && accessibility == TokenKind::KwProtected)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                aggregateKeyword.range(),
                "a struct member cannot be protected: structs are not inheritable");
        }

        ast::AstNode* declaration = arena_.make<ast::AstNode>(
            ast::AstNodeKind::StructDeclaration, aggregateKeyword.range());
        declaration->setText(synthesizedName);
        declaration->setAuxiliaryText(
            anonymousUnion ? "anonymous-union,union" : "anonymous-struct");
        // A public carrier needs its synthesized layout type to cross the same module boundary.
        // For private/internal carriers the matching accessibility also keeps the implementation
        // detail no more visible than the field that names it.
        if (accessibility != TokenKind::Invalid) declaration->setTokenKind(accessibility);
        const auto appendLayout = [&](std::string_view name, const std::optional<std::uint64_t> value)
        {
            if (!value.has_value()) return;
            std::string markers = declaration->auxiliaryText();
            markers += "," + std::string(name) + "=" + std::to_string(*value);
            declaration->setAuxiliaryText(std::move(markers));
        };
        appendLayout("pack", nestedLayoutPack);
        appendLayout("align", nestedLayoutAlign);
        appendLayout("size", nestedLayoutSize);
        parseStructBody(declaration, synthesizedName, beginIndex);

        const bool promotesMembers = !at(TokenKind::Identifier);
        std::string fieldName;
        SourceRange fieldNameRange = current().range();
        if (promotesMembers)
        {
            fieldName = "__nsc_anonymous_member_f" +
                std::to_string(aggregateKeyword.range().begin().fileId().value()) + "_" +
                std::to_string(aggregateKeyword.range().begin().offset());
            std::string markers = declaration->auxiliaryText();
            markers += ",promote";
            declaration->setAuxiliaryText(std::move(markers));
            if (isNew || isStatic || isReadonly || isVolatile || isConst)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    aggregateKeyword.range(),
                    "an unnamed anonymous aggregate accepts accessibility, at, pack, align and size only");
            }
        }
        else
        {
            const lexer::Token& name = consume();
            fieldName = std::string(name.lexeme());
            fieldNameRange = name.range();
        }
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            aggregateKeyword.range(), TokenKind::Identifier, synthesizedName);
        ast::AstNode* field = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration, fieldNameRange);
        field->setTypeSyntax(type);
        field->setText(std::move(fieldName));
        if (explicitFieldOffset.has_value())
            field->setAuxiliaryText("at:" + std::to_string(*explicitFieldOffset));
        if (promotesMembers)
        {
            std::string metadata = field->auxiliaryText();
            metadata += (metadata.empty() ? "" : ",");
            metadata += "anonymous-member-carrier";
            field->setAuxiliaryText(std::move(metadata));
        }
        if (accessibility != TokenKind::Invalid)
        {
            field->setTokenKind(accessibility);
            field->addModifier(accessibility);
        }
        if (isVolatile) field->addModifier(TokenKind::KwVolatile);
        if (isStatic) field->addModifier(TokenKind::KwStatic);
        if (isNew) field->addModifier(TokenKind::KwNew);
        if (isReadonly) field->addModifier(TokenKind::KwReadonly);
        if (isConst) field->addModifier(TokenKind::KwConst);
        if (!promotesMembers && consumeIf(TokenKind::Equal))
            field->setFirst(at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
        else if (!promotesMembers && isConst)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                fieldNameRange,
                "const field requires an initializer");
        }
        expect(TokenKind::Semicolon, "';'");
        field->setRange(tokenRangeFrom(beginIndex));
        declaration->setFirst(field);
        declaration->setRange(tokenRangeFrom(beginIndex));
        return declaration;
    }

    // NSC-LIMIT-009. UN TIPO ANIDADO. Va antes de la guarda de abajo porque `class`, `struct`,
    // `interface`, `enum` y `delegate` no son comienzos de tipo ni identificadores, de modo que
    // caian todos en «expected an instance field or method declaration in class body» -- que es
    // literalmente lo unico que le pasaba a `class Outer { struct Item { } }`.
    if (atNestedTypeDeclaration())
    {
        rejectControlledSpecialization("a nested type");
        if (isVolatile)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "volatile applies only to fields, not nested types");
        }
        if (isInline || isForceInline || isNoInline || isNoHeap)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "inline is valid only on an ordinary method with a body");
        }
        return parseNestedTypeDeclaration(
            NestedTypeModifiers{
                accessibility,
                isPartial,
                isNew,
                isStatic,
                isReadonly,
                isConst,
                isVirtual,
                isOverride,
                isAbstract,
                isSealed,
                isExported,
                isExternC,
                explicitFieldOffset.has_value(),
                isBorrowedView,
                isCopyType,
                nestedLayoutPack,
                nestedLayoutAlign,
                nestedLayoutSize},
            beginIndex);
    }

    const bool isReferenceReturn = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1U) || peek().kind() == TokenKind::LeftBracket);
    std::string returnLifetime;
    if (isReferenceReturn)
    {
        consume();
        returnLifetime = parseOptionalLifetimeAnnotation("ref return");
    }

    // A method-scoped generic parameter is declared after the method name, so the return type
    // lookahead cannot know `T` yet in `owned T Transfer<T>(...)`. The ownership keyword is an
    // unambiguous type prefix; parseType will retain it and the generic scope will validate T.
    if (!canStartTypeAt(index_) && !at(TokenKind::Identifier) &&
        !lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        rejectControlledSpecialization("an invalid member declaration");
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected an instance field or method declaration in class body");
        synchronizeStatement();
        return nullptr;
    }

    ast::TypeSyntax* type = parseType();

    // Item 327. C permits an unnamed bit-field declarator (`uint : 0;`) to reserve or terminate
    // a storage unit. Give it an unreachable deterministic internal name so the ordinary member
    // pipeline can retain declaration order, while keeping the width expression on second().
    // Item 328 owns physical placement; this item only establishes lossless source syntax.
    if (at(TokenKind::Colon))
    {
        rejectControlledSpecialization("an unnamed bit-field");
        const lexer::Token& colon = consume();
        ast::AstNode* width = parseExpression();
        if (width != nullptr && width->kind() == ast::AstNodeKind::AssignmentExpression)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                width->range(),
                "bit-field width cannot contain an assignment or initializer");
        }
        ast::AstNode* field = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration,
            colon.range());
        field->setTypeSyntax(type);
        field->setText("__nsc_unnamed_bitfield_f" +
            std::to_string(colon.range().begin().fileId().value()) + "_" +
            std::to_string(colon.range().begin().offset()));
        field->setAuxiliaryText("bitfield;bitfield-unnamed");
        field->setSecond(width);
        if (isInvalidVoidObjectType(type))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                type->range(),
                "bit-field cannot have object type 'void'");
        }
        if (accessibility != TokenKind::Invalid)
        {
            field->setTokenKind(accessibility);
            field->addModifier(accessibility);
        }
        if (isVolatile) field->addModifier(TokenKind::KwVolatile);
        if (isStatic || isNew || isReadonly || isConst || isReferenceReturn ||
            isVirtual || isOverride || isAbstract || isSealed || isExported || isExternC ||
            isInline || isForceInline || isNoInline || isNoHeap ||
            isPartial || isBorrowedView || explicitFieldOffset.has_value() ||
            nestedLayoutPack.has_value() || nestedLayoutAlign.has_value() || nestedLayoutSize.has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                colon.range(),
                "an unnamed bit-field accepts accessibility and optional volatile only");
        }
        expect(TokenKind::Semicolon, "';'");
        field->setRange(tokenRangeFrom(beginIndex));
        return field;
    }

    if (at(TokenKind::KwOperator))
    {
        rejectControlledSpecialization("an operator");
        if (isReferenceReturn)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "ref return is valid only on an ordinary method in item 313");
        }
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isReadonly || isVolatile || isConst || isNoHeap ||
            isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "user-defined operators cannot be readonly, volatile, const, noheap, virtual, override, abstract or sealed");
        }
        ast::AstNode* operatorDeclaration =
            parseOperatorTail(type, accessibility, isStatic, beginIndex);
        if (operatorDeclaration != nullptr)
        {
            if (isInline) operatorDeclaration->addModifier(TokenKind::KwInline);
            if (isForceInline) operatorDeclaration->addModifier(TokenKind::KwForceInline);
            if (isNoInline) operatorDeclaration->addModifier(TokenKind::KwNoInline);
        }
        return operatorDeclaration;
    }

    if (at(TokenKind::KwThis))
    {
        rejectControlledSpecialization("an indexer");
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isStatic || isVolatile || isConst || isInline || isForceInline || isNoInline || isNoHeap ||
            (isReadonly && !structContext))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "indexers are instance members; readonly is valid only for a struct indexer and "
                "static, volatile, or const are never valid here");
        }
        ast::AstNode* indexer = parseIndexerTail(
            type, accessibility, isNew, isVirtual, isOverride, isAbstract, isSealed,
            isReferenceReturn, beginIndex, false);
        if (isReadonly && indexer != nullptr)
        {
            indexer->addModifier(TokenKind::KwReadonly);
            bool setterFound = false;
            for (ast::AstNode* accessor : indexer->children())
            {
                if (accessor == nullptr || accessor->kind() != ast::AstNodeKind::MethodDeclaration) continue;
                if (accessor->auxiliaryText().starts_with("indexer:set:")) setterFound = true;
                else if (accessor->auxiliaryText().starts_with("indexer:get:"))
                    accessor->addModifier(TokenKind::KwReadonly);
            }
            if (setterFound)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    indexer->range(),
                    "a readonly struct indexer cannot declare a setter");
            }
        }
        return indexer;
    }

    if (!at(TokenKind::Identifier))
    {
        rejectControlledSpecialization("an invalid member declaration");
        expect(TokenKind::Identifier, "class member name");
        synchronizeStatement();
        return nullptr;
    }

    const lexer::Token& name = consume();
    if (name.lexeme() == className)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "ordinary class members cannot have the same name as the containing class; "
            "that name is reserved for constructors");
    }

    if (at(TokenKind::LeftParen) || at(TokenKind::Less))
    {
        if (isVolatile || isConst || (isReadonly && (!structContext || isStatic)))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "readonly is valid only on an instance struct method; const and volatile are field-only");
        }
        ast::AstNode* method = parseMethodTail(
            type, name, accessibility, isNew, isStatic, isVirtual, isOverride, isAbstract, isSealed,
            isExported, isExternC, isInline, isForceInline, isNoInline, isNoHeap, isUnsafe,
            beginIndex);
        if (method != nullptr && isControlledSpecialization)
        {
            method->setAuxiliaryText("controlled-specialization");
            if (method->genericParameters().empty())
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    controlledSpecializationRange,
                    "specialize requires an ordinary generic method");
            }
            if (isVirtual || isOverride || isAbstract || isSealed)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    controlledSpecializationRange,
                    "controlled specialization is not valid on virtual, override, abstract, or sealed methods");
            }
        }
        if (isReadonly && structContext && !isStatic && method != nullptr)
            method->addModifier(TokenKind::KwReadonly);
        if (method != nullptr)
        {
            method->setReference(isReferenceReturn);
            method->setLifetimeName(std::move(returnLifetime));
        }
        return method;
    }

    if (at(TokenKind::LeftBrace))
    {
        rejectControlledSpecialization("a property");
        if (isReferenceReturn)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "ref return is valid only on an ordinary method in item 313");
        }
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isVolatile || isConst || isInline || isForceInline || isNoInline || isNoHeap ||
            (isReadonly && (!structContext || isStatic)))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "readonly is valid only on an instance struct property; const, volatile, and inline are not property modifiers");
        }
        ast::AstNode* property = parsePropertyTail(
            type, name, accessibility, isNew, isStatic, isVirtual, isOverride, isAbstract, isSealed,
            beginIndex, false);
        if (isReadonly && structContext && !isStatic && property != nullptr)
        {
            property->addModifier(TokenKind::KwReadonly);
            bool setterFound = false;
            for (ast::AstNode* child : property->children())
            {
                if (child == nullptr) continue;
                if (child->kind() == ast::AstNodeKind::FieldDeclaration)
                    child->addModifier(TokenKind::KwReadonly);
                else if (child->kind() == ast::AstNodeKind::MethodDeclaration &&
                         child->auxiliaryText().starts_with("property:get:"))
                    child->addModifier(TokenKind::KwReadonly);
                else if (child->kind() == ast::AstNodeKind::MethodDeclaration &&
                         child->auxiliaryText().starts_with("property:set:"))
                    setterFound = true;
            }
            if (setterFound)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    property->range(),
                    "a readonly struct property cannot declare a setter");
            }
        }
        return property;
    }

    if (isExported || isExternC)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    }
    rejectControlledSpecialization("a field");
    if (isReferenceReturn)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "ref return is valid only on a method, not a field");
    }

    if (isVirtual || isOverride || isAbstract || isSealed)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "virtual, override, abstract and sealed modifiers are valid only on instance methods in Stage 35");
    }
    if (isInline || isForceInline || isNoInline || isNoHeap)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "inline is valid only on an ordinary method with a body");
    }

    if (isInvalidVoidObjectType(type))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type->range(),
            "instance field cannot have object type 'void'; pointer-to-void remains a valid pointer type");
    }

    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration,
        name.range());
    field->setTypeSyntax(type);
    field->setText(std::string(name.lexeme()));
    ast::AstNode* bitWidth = nullptr;
    if (consumeIf(TokenKind::Colon))
    {
        bitWidth = parseExpression();
        if (bitWidth != nullptr && bitWidth->kind() == ast::AstNodeKind::AssignmentExpression)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                bitWidth->range(),
                "bit-field width cannot contain an assignment or initializer");
        }
        field->setSecond(bitWidth);
        field->setAuxiliaryText("bitfield");
        if (isStatic || isNew || isReadonly || isConst || isReferenceReturn ||
            isPartial || isBorrowedView || explicitFieldOffset.has_value() ||
            nestedLayoutPack.has_value() || nestedLayoutAlign.has_value() || nestedLayoutSize.has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "a bit-field accepts accessibility and optional volatile only");
        }
    }
    // Items 272: field layout modifiers ride on the field's auxiliary text, which is where
    // ProjectMemberModel reads them back. `align(N)` is the same parsed modifier used by a nested
    // type; reaching an ordinary field resolves the contextual ambiguity in favor of the field.
    if (explicitFieldOffset.has_value())
    {
        field->setAuxiliaryText("at:" + std::to_string(*explicitFieldOffset));
    }
    if (nestedLayoutAlign.has_value())
    {
        std::string metadata = field->auxiliaryText();
        if (!metadata.empty()) metadata += ";";
        metadata += "field-align:" + std::to_string(*nestedLayoutAlign);
        field->setAuxiliaryText(std::move(metadata));
    }
    if (isRequired)
    {
        // NS304-T09-F01: `required` viaja en el auxiliaryText del campo, que es donde
        // ProjectMemberModel lee los marcadores de layout/required.
        std::string metadata = field->auxiliaryText();
        if (!metadata.empty()) metadata += ";";
        metadata += "required";
        field->setAuxiliaryText(std::move(metadata));
    }
    if (accessibility != TokenKind::Invalid)
    {
        field->setTokenKind(accessibility);
        field->addModifier(accessibility);
    }
    if (isVolatile)
    {
        field->addModifier(TokenKind::KwVolatile);
    }
    if (isStatic)
    {
        field->addModifier(TokenKind::KwStatic);
    }
    if (isNew)
    {
        field->addModifier(TokenKind::KwNew);
    }
    if (isReadonly)
    {
        field->addModifier(TokenKind::KwReadonly);
    }
    if (isConst)
    {
        field->addModifier(TokenKind::KwConst);
    }

    if (bitWidth == nullptr && consumeIf(TokenKind::Equal))
    {
        // C7 "aggregate initializer": a field takes the same braced form a local does, for the
        // same reason -- it has a declared type to be filled against.
        field->setFirst(at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    }
    else if (bitWidth == nullptr && isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "const field requires an initializer");
    }

    expect(TokenKind::Semicolon, "';'");
    field->setRange(tokenRangeFrom(beginIndex));
    return field;
}

ast::AstNode* Parser::parseEventTail(
    TokenKind accessibility,
    bool isNew,
    bool isStatic,
    std::size_t beginIndex)
{
    const SourceRange eventRange = consume().range(); // event
    if (!canStartTypeAt(index_))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "event declaration requires a delegate type");
        synchronizeStatement();
        return nullptr;
    }
    ast::TypeSyntax* eventType = parseType();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "event name");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token& name = consume();
    ast::AstNode* event = arena_.make<ast::AstNode>(ast::AstNodeKind::EventDeclaration, eventRange);
    event->setText(std::string(name.lexeme()));
    event->setTypeSyntax(eventType);
    event->setAuxiliaryText("event");
    if (accessibility != TokenKind::Invalid)
    {
        event->setTokenKind(accessibility);
        event->addModifier(accessibility);
    }
    if (isStatic) event->addModifier(TokenKind::KwStatic);
    if (isNew) event->addModifier(TokenKind::KwNew);

    // Stage 42 event storage is an implementation detail.  Keep it as a native
    // pointer-sized field of the delegate type so layout/static-init machinery is reused.
    ast::AstNode* backing = arena_.make<ast::AstNode>(ast::AstNodeKind::FieldDeclaration, name.range());
    backing->setText("__nsc_event_" + std::string(name.lexeme()) + "$stage42");
    backing->setAuxiliaryText("event-backing:" + std::string(name.lexeme()));
    backing->setTypeSyntax(eventType);
    backing->setTokenKind(TokenKind::KwPrivate);
    backing->addModifier(TokenKind::KwPrivate);
    if (isStatic) backing->addModifier(TokenKind::KwStatic);
    event->addChild(backing);

    // NS304-T09-F08: un event puede declarar accessors custom `add`/`remove`; en ese caso el
    // backing sintetizado sigue como storage y los accessors bajan como metodos que Sema invoca.
    if (at(TokenKind::LeftBrace))
    {
        consume(); // '{'
        while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            const bool isAdd = atContextualKeyword("add");
            const bool isRemove = atContextualKeyword("remove");
            if (!isAdd && !isRemove)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "expected add or remove accessor in event declaration");
                synchronizeStatement();
                if (at(TokenKind::RightBrace)) break;
                continue;
            }
            const SourceRange accessorRange = consume().range();
            ast::AstNode* accessor = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MethodDeclaration, accessorRange);
            accessor->setText((isAdd ? "add_" : "remove_") + std::string(name.lexeme()));
            accessor->setAuxiliaryText(
                (isAdd ? "event:add:" : "event:remove:") + std::string(name.lexeme()));
            accessor->setTypeSyntax(arena_.make<ast::TypeSyntax>(
                accessorRange, TokenKind::KwVoid, "void"));
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, accessorRange);
            value->setText("value");
            value->setTypeSyntax(eventType);
            accessor->addChild(value);
            accessor->setFirst(parseBlock());
            accessor->setRange(tokenRangeFrom(beginIndex));
            event->addChild(accessor);
        }
        expect(TokenKind::RightBrace, "'}' after event accessors");
    }
    else
    {
        expect(TokenKind::Semicolon, "';'");
    }
    event->setRange(tokenRangeFrom(beginIndex));
    return event;
}

ast::AstNode* Parser::makeAccessorMethod(
    std::string name,
    ast::TypeSyntax* returnType,
    TokenKind accessibility,
    bool isStatic,
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    std::string metadata,
    SourceRange range)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(ast::AstNodeKind::MethodDeclaration, range);
    method->setText(std::move(name));
    method->setAuxiliaryText(std::move(metadata));
    method->setTypeSyntax(returnType);
    if (accessibility != TokenKind::Invalid)
    {
        method->setTokenKind(accessibility);
        method->addModifier(accessibility);
    }
    if (isStatic) method->addModifier(TokenKind::KwStatic);
    if (isVirtual) method->addModifier(TokenKind::KwVirtual);
    if (isOverride) method->addModifier(TokenKind::KwOverride);
    if (isAbstract) method->addModifier(TokenKind::KwAbstract);
    if (isSealed) method->addModifier(TokenKind::KwSealed);
    return method;
}

ast::AstNode* Parser::makeAutoGetterBody(
    std::string_view backingFieldName,
    SourceRange range)
{
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    ast::AstNode* statement = arena_.make<ast::AstNode>(ast::AstNodeKind::ReturnStatement, range);
    ast::AstNode* field = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, range);
    field->setText(std::string(backingFieldName));
    statement->setFirst(field);
    block->addChild(statement);
    return block;
}

ast::AstNode* Parser::makeAutoSetterBody(
    std::string_view backingFieldName,
    SourceRange range,
    bool transferOwnership)
{
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    ast::AstNode* statement = arena_.make<ast::AstNode>(ast::AstNodeKind::ExpressionStatement, range);
    ast::AstNode* assignment = arena_.make<ast::AstNode>(ast::AstNodeKind::AssignmentExpression, range);
    assignment->setTokenKind(TokenKind::Equal);
    ast::AstNode* field = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, range);
    field->setText(std::string(backingFieldName));
    ast::AstNode* value = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, range);
    value->setText("value");
    assignment->setFirst(field);
    // Bloque 1 item 9: for an `owned` automatic property the generated body must be
    // `backing = move(value);`, not `backing = value;`.
    //
    // Both the compiler-generated backing field and the setter's implicit `value` parameter are
    // built from the property's own TypeSyntax, so on an `owned` property BOTH are owners. A
    // plain assignment between two owners is precisely the copy-owner hole the language rejects
    // everywhere else (`owned T b = a;` is an error), and it is not a diagnostic to route around
    // here -- writing the pointer into the field without consuming the parameter would leave the
    // caller's transferred object with two owners, and the setter's own owned-parameter cleanup
    // would free it on the way out while the field kept pointing at it.
    //
    // The explicit form of this same property is written `set { backing = move(value); }` by
    // hand; the automatic form generates the identical tree so the two spellings mean the same
    // thing rather than one of them being quietly unsound.
    if (transferOwnership)
    {
        ast::AstNode* moveExpression = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, range);
        moveExpression->setFirst(value);
        assignment->setSecond(moveExpression);
    }
    else
    {
        assignment->setSecond(value);
    }
    statement->setFirst(assignment);
    block->addChild(statement);
    return block;
}

ast::AstNode* Parser::parsePropertyTail(
    ast::TypeSyntax* propertyType,
    const lexer::Token& name,
    TokenKind accessibility,
    bool isNew,
    bool isStatic,
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    std::size_t beginIndex,
    bool interfaceContext)
{
    ast::AstNode* property = arena_.make<ast::AstNode>(
        ast::AstNodeKind::PropertyDeclaration, name.range());
    property->setText(std::string(name.lexeme()));
    property->setTypeSyntax(propertyType);
    property->setAuxiliaryText("property");
    if (accessibility != TokenKind::Invalid)
    {
        property->setTokenKind(accessibility);
        property->addModifier(accessibility);
    }
    if (isStatic) property->addModifier(TokenKind::KwStatic);
    if (isNew) property->addModifier(TokenKind::KwNew);
    if (isVirtual) property->addModifier(TokenKind::KwVirtual);
    if (isOverride) property->addModifier(TokenKind::KwOverride);
    if (isAbstract) property->addModifier(TokenKind::KwAbstract);
    if (isSealed) property->addModifier(TokenKind::KwSealed);

    if (isInvalidVoidObjectType(propertyType))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            propertyType->range(),
            "property cannot have object type 'void'");
    }

    expect(TokenKind::LeftBrace, "'{'");
    bool sawGet = false;
    bool sawSet = false;
    bool sawAutomatic = false;
    bool sawExplicit = false;
    std::vector<ast::AstNode*> accessors;
    const std::string propertyName(name.lexeme());
    const std::string backingName = "__nsc_backing_" + propertyName + "$stage38";

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        TokenKind accessorAccessibility = TokenKind::Invalid;
        if (isAccessibilityToken(current().kind()))
        {
            accessorAccessibility = consume().kind();
            if (interfaceContext)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "interface property accessors cannot narrow accessibility in Stage 38");
            }
        }
        const bool getter = at(TokenKind::KwGet);
        // NS304-T09-F02: `init` se reconoce como un setter contextual de solo-inicializacion y se
        // marca en el metadata `:init` para que Sema distinga el setter restringido del ordinario.
        const bool initOnly = atContextualKeyword("init");
        const bool setter = at(TokenKind::KwSet) || initOnly;
        if (!getter && !setter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected get, set or init accessor in property declaration");
            synchronizeStatement();
            if (at(TokenKind::RightBrace)) break;
            continue;
        }
        const SourceRange accessorRange = consume().range();
        if ((getter && sawGet) || (setter && sawSet))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                accessorRange,
                getter ? "duplicate get accessor" : "duplicate set accessor");
        }
        sawGet = sawGet || getter;
        sawSet = sawSet || setter;

        const TokenKind effectiveAccessibility = accessorAccessibility != TokenKind::Invalid
            ? accessorAccessibility : accessibility;
        ast::TypeSyntax* returnType = getter
            ? propertyType
            : arena_.make<ast::TypeSyntax>(accessorRange, TokenKind::KwVoid, "void");
        ast::AstNode* accessor = makeAccessorMethod(
            (getter ? "get_" : "set_") + propertyName,
            returnType,
            effectiveAccessibility,
            isStatic,
            isVirtual,
            isOverride,
            isAbstract,
            isSealed,
            std::string("property:") + (getter ? "get:" : "set:") + propertyName +
                (initOnly ? ":init" : ""),
            accessorRange);
        if (isNew) accessor->addModifier(TokenKind::KwNew);
        if (setter)
        {
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, accessorRange);
            value->setText("value");
            value->setTypeSyntax(propertyType);
            accessor->addChild(value);
        }

        parseThrowsClause(accessor);

        if (consumeIf(TokenKind::Semicolon))
        {
            if (interfaceContext || isAbstract)
            {
                // A bodyless interface or trait accessor remains a pure contract.  The abstract
                // marker belongs to the accessor, not to the enclosing declaration: a property
                // may legally mix a default getter with a required setter.
                if (!accessor->hasModifier(TokenKind::KwAbstract))
                    accessor->addModifier(TokenKind::KwAbstract);
            }
            else
            {
                sawAutomatic = true;
                const bool ownedProperty = propertyType != nullptr &&
                    propertyType->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::Owned;
                accessor->setFirst(
                    getter ? makeAutoGetterBody(backingName, accessorRange)
                           : makeAutoSetterBody(backingName, accessorRange, ownedProperty));
            }
        }
        else if (at(TokenKind::LeftBrace))
        {
            sawExplicit = true;
            if (isAbstract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "abstract/interface property accessors cannot declare bodies");
            }
            accessor->setFirst(parseBlock());
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "property accessor requires ';' or a block body");
            synchronizeStatement();
        }
        accessor->setRange(tokenRangeFrom(beginIndex));
        accessors.push_back(accessor);
    }
    expect(TokenKind::RightBrace, "'}'");

    // C#-compatible auto-property initializer.  A property is represented by accessor methods
    // plus a compiler-generated backing field, so the initializer belongs to that field: this
    // deliberately reuses the complete Stage 37 field-initializer pipeline (type checking,
    // ownership and instance/static lowering) instead of inventing a second initialization path.
    ast::AstNode* propertyInitializer = nullptr;
    if (consumeIf(TokenKind::Equal))
    {
        propertyInitializer = at(TokenKind::LeftBrace)
            ? parseAggregateInitializer()
            : parseExpression();
        expect(TokenKind::Semicolon, "';'");
        if (!sawAutomatic || sawExplicit || interfaceContext || isAbstract)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                property->range(),
                "a property initializer requires a non-abstract automatic property");
        }
    }

    // Set-only properties: NO soportadas, y el motivo real esta un piso mas abajo que aqui.
    //
    // Se intento permitirlas quitando este rechazo, porque un setter con cuerpo explicito es un
    // metodo con nombre de propiedad y no parece necesitar un getter. Parsea bien. Lo que no
    // funciona es usarla: la resolucion de propiedades esta ANCLADA AL GETTER de punta a punta
    // -- una asignacion a una propiedad liga primero el getter y de ahi deriva el nombre del
    // setter ("set_" + associatedName, ver analyzeAssignment en ProjectMethodBodyModel) -- asi
    // que sin getter el miembro no liga y sale
    //
    //   error: receiver/type '...' has no compatible member named 'Setter'
    //
    // que es peor que este rechazo: convierte un "no soportado" claro en un "no existe"
    // desconcertante sobre un miembro que esta escrito ahi.
    //
    // Soportarlas de verdad es cambiar esa ruta de binding para que una expresion de miembro en
    // posicion de asignacion pueda ligar directamente contra el setter. Es trabajo en
    // ProjectMethodBodyModel, no aqui, y hasta que se haga el rechazo se queda -- pero diciendo
    // que es una limitacion conocida y con que reemplazarla, no como si fuera un error del
    // programador.
    if (!sawGet)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            property->range(),
            "set-only properties are not supported: property access resolves through the get "
            "accessor, so a property without one cannot be bound. Declare a get accessor, or "
            "use an ordinary method such as 'void SetX(T value)'");
    }
    if (sawAutomatic && sawExplicit)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            property->range(),
            "a Stage 38 property cannot mix automatic and explicit accessors");
    }

    if (interfaceContext && !sawExplicit &&
        !property->hasModifier(TokenKind::KwAbstract))
    {
        property->addModifier(TokenKind::KwAbstract);
    }

    if (sawAutomatic && !interfaceContext && !isAbstract)
    {
        ast::AstNode* backing = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration, name.range());
        backing->setText(backingName);
        backing->setAuxiliaryText("property-backing:" + propertyName);
        backing->setTypeSyntax(propertyType);
        backing->setTokenKind(TokenKind::KwPrivate);
        backing->addModifier(TokenKind::KwPrivate);
        if (isStatic) backing->addModifier(TokenKind::KwStatic);
        backing->setFirst(propertyInitializer);
        property->addChild(backing);
    }
    for (ast::AstNode* accessor : accessors)
    {
        property->addChild(accessor);
    }
    property->setRange(tokenRangeFrom(beginIndex));
    return property;
}

ast::AstNode* Parser::parseIndexerTail(
    ast::TypeSyntax* elementType,
    TokenKind accessibility,
    bool isNew,
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    bool isReferenceReturn,
    std::size_t beginIndex,
    bool interfaceContext)
{
    const SourceRange thisRange = consume().range(); // this
    ast::AstNode* indexer = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IndexerDeclaration, thisRange);
    indexer->setText("Item");
    indexer->setAuxiliaryText("indexer");
    indexer->setTypeSyntax(elementType);
    indexer->setReference(isReferenceReturn);
    if (accessibility != TokenKind::Invalid)
    {
        indexer->setTokenKind(accessibility);
        indexer->addModifier(accessibility);
    }
    if (isVirtual) indexer->addModifier(TokenKind::KwVirtual);
    if (isNew) indexer->addModifier(TokenKind::KwNew);
    if (isOverride) indexer->addModifier(TokenKind::KwOverride);
    if (isAbstract) indexer->addModifier(TokenKind::KwAbstract);
    if (isSealed) indexer->addModifier(TokenKind::KwSealed);

    expect(TokenKind::LeftBracket, "'['");
    std::vector<ast::AstNode*> indexParameters;
    if (!at(TokenKind::RightBracket))
    {
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr) indexParameters.push_back(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightBracket, "']'");
    if (indexParameters.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            indexer->range(),
            "indexer must declare at least one index parameter");
    }
    expect(TokenKind::LeftBrace, "'{'");

    bool sawGet = false;
    bool sawSet = false;
    bool sawBody = false;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        TokenKind accessorAccessibility = TokenKind::Invalid;
        if (isAccessibilityToken(current().kind()))
        {
            accessorAccessibility = consume().kind();
            if (interfaceContext)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "interface indexer accessors cannot narrow accessibility in Stage 38");
            }
        }
        const bool getter = at(TokenKind::KwGet);
        const bool initOnly = atContextualKeyword("init");
        const bool setter = at(TokenKind::KwSet) || initOnly;
        if (!getter && !setter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected get, set or init accessor in indexer declaration");
            synchronizeStatement();
            if (at(TokenKind::RightBrace)) break;
            continue;
        }
        const SourceRange accessorRange = consume().range();
        if ((getter && sawGet) || (setter && sawSet))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                accessorRange,
                getter ? "duplicate indexer get accessor" : "duplicate indexer set accessor");
        }
        sawGet = sawGet || getter;
        sawSet = sawSet || setter;
        if (isReferenceReturn && setter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                accessorRange,
                "a reference-returning indexer is get-only and cannot declare a setter");
        }
        const TokenKind effectiveAccessibility = accessorAccessibility != TokenKind::Invalid
            ? accessorAccessibility : accessibility;
        ast::TypeSyntax* returnType = getter
            ? elementType
            : arena_.make<ast::TypeSyntax>(accessorRange, TokenKind::KwVoid, "void");
        ast::AstNode* accessor = makeAccessorMethod(
            getter ? "get_Item" : "set_Item",
            returnType,
            effectiveAccessibility,
            false,
            isVirtual,
            isOverride,
            isAbstract,
            isSealed,
            std::string("indexer:") + (getter ? "get:Item" : "set:Item") +
                (initOnly ? ":init" : ""),
            accessorRange);
        if (getter) accessor->setReference(isReferenceReturn);
        if (isNew) accessor->addModifier(TokenKind::KwNew);
        for (ast::AstNode* parameter : indexParameters) accessor->addChild(parameter);
        if (setter)
        {
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, accessorRange);
            value->setText("value");
            value->setTypeSyntax(elementType);
            accessor->addChild(value);
        }

        parseThrowsClause(accessor);

        if (consumeIf(TokenKind::Semicolon))
        {
            if (interfaceContext || isAbstract)
            {
                if (!accessor->hasModifier(TokenKind::KwAbstract))
                    accessor->addModifier(TokenKind::KwAbstract);
            }
            else
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    accessorRange,
                    "concrete Stage 38 indexers require explicit accessor bodies");
            }
        }
        else if (at(TokenKind::LeftBrace))
        {
            sawBody = true;
            if (isAbstract)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "abstract/interface indexer accessors cannot declare bodies");
            }
            accessor->setFirst(parseBlock());
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "indexer accessor requires ';' or a block body");
            synchronizeStatement();
        }
        accessor->setRange(tokenRangeFrom(beginIndex));
        indexer->addChild(accessor);
    }
    expect(TokenKind::RightBrace, "'}'");
    if (interfaceContext && !sawBody &&
        !indexer->hasModifier(TokenKind::KwAbstract))
    {
        indexer->addModifier(TokenKind::KwAbstract);
    }
    if (!sawGet)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            indexer->range(),
            "set-only indexers are not part of the Stage 38 subset");
    }
    indexer->setRange(tokenRangeFrom(beginIndex));
    return indexer;
}

ast::AstNode* Parser::parseOperatorTail(
    ast::TypeSyntax* returnType,
    TokenKind accessibility,
    bool isStatic,
    std::size_t beginIndex,
    bool allowBodyless)
{
    const SourceRange operatorKeywordRange = consume().range(); // operator
    if (!isStage38OperatorToken(current().kind()))
    {
        const std::string_view spelling = operatorSpelling(current().kind());
        const bool shortCircuit = spelling == "&&" || spelling == "||" || spelling == "??";
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            shortCircuit
                ? "short-circuit operator '" + std::string(spelling) +
                    "' is intrinsic and cannot be overloaded; a future dedicated branching "
                    "protocol would be required"
                : "unsupported user-defined operator token in Stage 38");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token operation = consume();
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, operatorKeywordRange);
    method->setTypeSyntax(returnType);
    method->setTokenKind(operation.kind());
    method->setAuxiliaryText("operator:" + std::string(lexer::tokenKindName(operation.kind())));
    if (accessibility != TokenKind::KwPublic)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            operatorKeywordRange,
            "user-defined operators must be declared public");
    }
    if (!isStatic)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            operatorKeywordRange,
            "user-defined operators must be declared static");
    }
    method->setTokenKind(TokenKind::KwPublic);
    method->addModifier(TokenKind::KwPublic);
    method->addModifier(TokenKind::KwStatic);

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr) method->addChild(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    parseThrowsClause(method);
    parseFloatingPointModeClause(method);
    parseFfiUnwindClause(method);
    method->setText(operatorMethodName(operation.kind(), method->children().size()));

    const std::size_t arity = method->children().size();
    const bool unaryOnly = operation.kind() == TokenKind::Bang || operation.kind() == TokenKind::Tilde;
    const bool binaryOnly = operation.kind() != TokenKind::Plus && operation.kind() != TokenKind::Minus &&
        operation.kind() != TokenKind::Bang && operation.kind() != TokenKind::Tilde &&
        operation.kind() != TokenKind::PlusPlus && operation.kind() != TokenKind::MinusMinus;
    if ((unaryOnly && arity != 1) || (binaryOnly && arity != 2) ||
        ((operation.kind() == TokenKind::Plus || operation.kind() == TokenKind::Minus) &&
         arity != 1 && arity != 2) ||
        ((operation.kind() == TokenKind::PlusPlus || operation.kind() == TokenKind::MinusMinus) && arity != 1))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            method->range(),
            "user-defined operator has invalid arity for its token");
    }
    if (operation.kind() == TokenKind::PlusPlus || operation.kind() == TokenKind::MinusMinus)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            operation.range(),
            "a user-defined 'operator ++'/'operator --' is not supported: the built-in ++/-- now "
            "update an lvalue in place (ID 79), but calling a user operator and storing its result "
            "back would evaluate the operand twice, which is the one thing that lowering exists to "
            "avoid; write an explicit method call and assignment");
    }

    if (at(TokenKind::LeftBrace)) method->setFirst(parseBlock());
    else if (allowBodyless && consumeIf(TokenKind::Semicolon))
    {
        method->addModifier(TokenKind::KwAbstract);
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "user-defined operator requires a body");
        if (!consumeIf(TokenKind::Semicolon)) synchronizeStatement();
    }
    if (method->first() != nullptr && method->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyFloatingPointModeToTree(method->first(), method->floatingPointMode());
    method->setRange(tokenRangeFrom(beginIndex));
    return method;
}

ast::AstNode* Parser::parseConversionOperatorTail(
    bool implicitConversion,
    TokenKind accessibility,
    bool isStatic,
    std::size_t beginIndex)
{
    const SourceRange conversionRange = consume().range(); // implicit/explicit
    expect(TokenKind::KwOperator, "'operator'");
    if (!canStartTypeAt(index_))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "conversion operator requires a target type");
        synchronizeStatement();
        return nullptr;
    }
    ast::TypeSyntax* targetType = parseType();
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, conversionRange);
    method->setText(implicitConversion ? "op_Implicit" : "op_Explicit");
    method->setAuxiliaryText(implicitConversion ? "conversion:implicit" : "conversion:explicit");
    method->setTypeSyntax(targetType);
    if (accessibility != TokenKind::KwPublic)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            conversionRange,
            "conversion operators must be declared public");
    }
    if (!isStatic)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            conversionRange,
            "conversion operators must be declared static");
    }
    method->setTokenKind(TokenKind::KwPublic);
    method->addModifier(TokenKind::KwPublic);
    method->addModifier(TokenKind::KwStatic);

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr) method->addChild(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    parseThrowsClause(method);
    parseFfiUnwindClause(method);
    if (method->children().size() != 1)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            method->range(),
            "conversion operator must declare exactly one source parameter");
    }
    if (at(TokenKind::LeftBrace)) method->setFirst(parseBlock());
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "conversion operator requires a body");
        if (!consumeIf(TokenKind::Semicolon)) synchronizeStatement();
    }
    method->setRange(tokenRangeFrom(beginIndex));
    return method;
}

ast::AstNode* Parser::parseDestructorTail(
    std::string_view className,
    std::size_t beginIndex)
{
    consume(); // '~'
    if (!at(TokenKind::Identifier) || current().lexeme() != className)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "destructor name must exactly match the containing class name");
        if (at(TokenKind::Identifier))
        {
            consume();
        }
    }
    else
    {
        consume();
    }

    ast::AstNode* destructor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DestructorDeclaration,
        tokenRangeFrom(beginIndex));
    destructor->setText("~" + std::string(className));
    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "destructors cannot declare parameters");
        while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
        {
            consume();
        }
    }
    expect(TokenKind::RightParen, "')'");
    if (at(TokenKind::LeftBrace))
    {
        destructor->setFirst(parseBlock());
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "destructor declarations require a body");
        if (!consumeIf(TokenKind::Semicolon))
        {
            synchronizeStatement();
        }
    }
    destructor->setRange(tokenRangeFrom(beginIndex));
    return destructor;
}

ast::AstNode* Parser::parseConstructorTail(
    std::string_view className,
    TokenKind accessibility,
    bool isStatic,
    bool isNoHeap,
    std::size_t beginIndex)
{
    const lexer::Token& name = consume();
    ast::AstNode* constructor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConstructorDeclaration,
        name.range());
    constructor->setText(std::string(className));
    if (isStatic)
    {
        constructor->addModifier(TokenKind::KwStatic);
    }
    if (accessibility != TokenKind::Invalid)
    {
        constructor->setTokenKind(accessibility);
        constructor->addModifier(accessibility);
    }
    if (isNoHeap)
    {
        constructor->addModifier(TokenKind::KwNoHeap);
    }

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        if (isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "static constructors cannot declare parameters");
        }
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr)
            {
                constructor->addChild(parameter);
            }
            if (!consumeIf(TokenKind::Comma))
            {
                break;
            }
        }
    }
    expect(TokenKind::RightParen, "')'");

    if (consumeIf(TokenKind::Colon))
    {
        const SourceRange initializerBegin = tokens_[index_ - 1].range();
        if (isStatic)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                initializerBegin,
                "static constructors cannot declare base(...) or this(...) initializers");
        }
        const bool baseInitializer = at(TokenKind::KwBase);
        const bool thisInitializer = at(TokenKind::KwThis);
        if (!baseInitializer && !thisInitializer)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "constructor initializer must name base(...) or this(...)");
        }
        else
        {
            consume();
            ast::AstNode* initializer = arena_.make<ast::AstNode>(
                baseInitializer
                    ? ast::AstNodeKind::BaseConstructorInitializer
                    : ast::AstNodeKind::ThisConstructorInitializer,
                initializerBegin);
            expect(TokenKind::LeftParen, "'('");
            if (!at(TokenKind::RightParen))
            {
                while (true)
                {
                    ast::AstNode* argument = parseArgument();
                    if (argument != nullptr)
                    {
                        initializer->addChild(argument);
                    }
                    if (!consumeIf(TokenKind::Comma))
                    {
                        break;
                    }
                }
            }
            expect(TokenKind::RightParen, "')'");
            initializer->setRange(SourceRange(initializerBegin.begin(), tokens_[index_ - 1].range().end()));
            constructor->setSecond(initializer);
        }
    }

    if (at(TokenKind::LeftBrace))
    {
        constructor->setFirst(parseBlock());
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "constructor declarations require a body in Stage 34");
        expect(TokenKind::LeftBrace, "constructor body");
    }

    constructor->setRange(tokenRangeFrom(beginIndex));
    return constructor;
}

ast::AstNode* Parser::parseMethodTail(
    ast::TypeSyntax* returnType,
    const lexer::Token& name,
    TokenKind accessibility,
    bool isNew,
    bool isStatic,
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    bool isExported,
    bool isExternC,
    bool isInline,
    bool isForceInline,
    bool isNoInline,
    bool isNoHeap,
    bool isUnsafe,
    std::size_t beginIndex,
    bool allowBodyless)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration,
        name.range());
    method->setTypeSyntax(returnType);
    method->setText(std::string(name.lexeme()));
    if (accessibility != TokenKind::Invalid)
    {
        method->setTokenKind(accessibility);
        method->addModifier(accessibility);
    }
    if (isStatic)
    {
        method->addModifier(TokenKind::KwStatic);
    }
    if (isNew)
    {
        method->addModifier(TokenKind::KwNew);
    }
    if (isVirtual)
    {
        method->addModifier(TokenKind::KwVirtual);
    }
    if (isOverride)
    {
        method->addModifier(TokenKind::KwOverride);
    }
    if (isAbstract)
    {
        method->addModifier(TokenKind::KwAbstract);
    }
    if (isSealed)
    {
        method->addModifier(TokenKind::KwSealed);
    }
    if (isExported)
    {
        method->addModifier(TokenKind::KwExport);
    }
    if (isExternC)
    {
        method->addModifier(TokenKind::KwExtern);
    }
    if (isInline)
    {
        method->addModifier(TokenKind::KwInline);
    }
    if (isForceInline)
    {
        method->addModifier(TokenKind::KwForceInline);
    }
    if (isNoInline)
    {
        method->addModifier(TokenKind::KwNoInline);
    }
    if (isNoHeap)
    {
        method->addModifier(TokenKind::KwNoHeap);
    }
    method->setUnsafe(isUnsafe);

    parseGenericParameterList(method);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, method);

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr)
            {
                method->addChild(parameter);
            }
            if (!consumeIf(TokenKind::Comma))
            {
                break;
            }
        }
    }
    expect(TokenKind::RightParen, "')'");
    parseThrowsClause(method);
    // NSC-LIMIT-007: `void M<T>(T x) where T : class { ... }` -- despues de la lista de
    // parametros y antes del cuerpo o del punto y coma.
    parseFloatingPointModeClause(method);
    parseFfiUnwindClause(method);
    parseGenericConstraintClauses(method);

    if (isAbstract)
    {
        if (at(TokenKind::LeftBrace))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "abstract methods declare no body; terminate the declaration with ';'");
            method->setFirst(parseBlock());
        }
        else
        {
            expect(TokenKind::Semicolon, "';'");
        }
    }
    else if (at(TokenKind::LeftBrace))
    {
        method->setFirst(parseBlock());
    }
    else if (allowBodyless)
    {
        expect(TokenKind::Semicolon, "';'");
        method->addModifier(TokenKind::KwAbstract);
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "non-abstract class methods require a body in Stage 35");
        if (!consumeIf(TokenKind::Semicolon))
        {
            synchronizeStatement();
        }
    }

    if (method->first() != nullptr && method->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyFloatingPointModeToTree(method->first(), method->floatingPointMode());
    method->setRange(tokenRangeFrom(beginIndex));
    return method;
}

ast::AstNode* Parser::parseEnumDeclaration()
{
    const std::size_t beginIndex = index_;
    consume();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "enum name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    knownTypeNames_.insert(std::string(name.lexeme()));
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::EnumDeclaration, name.range());
    node->setText(std::string(name.lexeme()));

    // `enum Nombre : tipo` -- EL TIPO SUBYACENTE.
    //
    // POR QUE NO ES COMODIDAD. Hasta hoy TODO enum media cuatro bytes
    // (TargetDataLayout.cpp, BuiltinLayoutKind::Enum). Un header de C con `enum : uint8_t` dentro
    // de una struct produce entonces un layout SILENCIOSAMENTE EQUIVOCADO: el campo ocupa cuatro
    // bytes donde C pone uno, y todo lo que va detras se desplaza. Eso no es una carencia de
    // expresividad, es una respuesta falsa -- P1 por el criterio de este proyecto.
    //
    // El ancho y el signo de un enum son parte del ABI, y esta es la unica forma de decirlos.
    // Sin `: tipo` sigue siendo `int`, que es el valor por omision de C y lo que ya hacia.
    //
    // Se admiten los enteros incorporados y nada mas: ni `bool`, ni coma flotante, ni otro enum.
    // La comprobacion de cual se escribio la hace el modelo de nombres, que es quien resuelve
    // tipos; aqui solo se guarda lo que el autor escribio.
    if (consumeIf(TokenKind::Colon))
    {
        ast::TypeSyntax* underlying = parseType();
        node->setTypeSyntax(underlying);
    }

    expect(TokenKind::LeftBrace, "'{'");
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "enum member name");
            synchronizeStatement();
            if (at(TokenKind::RightBrace))
            {
                break;
            }
            continue;
        }

        const std::size_t memberBegin = index_;
        const lexer::Token& memberName = consume();
        ast::AstNode* member = arena_.make<ast::AstNode>(
            ast::AstNodeKind::EnumMemberDeclaration,
            memberName.range());
        member->setText(std::string(memberName.lexeme()));
        if (consumeIf(TokenKind::Equal))
        {
            member->setFirst(parseExpression());
        }
        member->setRange(tokenRangeFrom(memberBegin));
        node->addChild(member);

        if (!consumeIf(TokenKind::Comma))
        {
            break;
        }
    }

    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseExternCDeclaration()
{
    const std::size_t beginIndex = index_;
    consume();
    const bool isVectorCall = consumeIf(TokenKind::KwVectorCall);
    if (!isVectorCall && !atContextualKeyword("C"))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected 'C' after extern; extern namespace variables are not supported in "
            "NativeSharp 3.0; declare exactly one namespace-state definition, or use 'extern C' "
            "for a native function boundary");
        synchronizeGlobal();
        return nullptr;
    }
    if (!isVectorCall) consume();

    if (at(TokenKind::KwDelegate))
    {
        ast::AstNode* delegate = parseDelegateDeclaration();
        if (delegate != nullptr)
        {
            delegate->addModifier(TokenKind::KwExtern);
            if (isVectorCall) delegate->addModifier(TokenKind::KwVectorCall);
            delegate->setRange(tokenRangeFrom(beginIndex));
        }
        return delegate;
    }

    if (isVectorCall)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "extern vectorcall currently declares a delegate type");
        synchronizeGlobal();
        return nullptr;
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ExternCDeclaration, tokenRangeFrom(beginIndex));

    if (consumeIf(TokenKind::LeftBrace))
    {
        while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            const TokenKind accessibility =
                (at(TokenKind::KwPublic) || at(TokenKind::KwInternal))
                    ? consume().kind()
                    : TokenKind::Invalid;
            ast::TypeSyntax* returnType = parseType();
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "external function name");
                synchronizeGlobal();
                continue;
            }
            const lexer::Token& name = consume();
            ast::AstNode* function = parseFunctionTail(returnType, name, true);
            if (function != nullptr && accessibility != TokenKind::Invalid)
            {
                function->setTokenKind(accessibility);
                function->addModifier(accessibility);
            }
            node->addChild(function);
        }
        expect(TokenKind::RightBrace, "'}'");
    }
    else
    {
        ast::TypeSyntax* returnType = parseType();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "external function name");
            synchronizeGlobal();
            return node;
        }
        const lexer::Token& name = consume();
        node->addChild(parseFunctionTail(returnType, name, true));
    }

    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseFunctionOrGlobalVariable()
{
    const std::size_t beginIndex = index_;
    const bool isConst = consumeIf(TokenKind::KwConst);
    const bool isReferenceReturn = !isConst && atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1U) || peek().kind() == TokenKind::LeftBracket);
    std::string returnLifetime;
    if (isReferenceReturn)
    {
        consume();
        returnLifetime = parseOptionalLifetimeAnnotation("ref return");
    }
    ast::TypeSyntax* type = parseType();

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "declaration name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    if (!isConst && (at(TokenKind::LeftParen) || at(TokenKind::Less)))
    {
        ast::AstNode* function = parseFunctionTail(type, name, false);
        if (function != nullptr)
        {
            function->setReference(isReferenceReturn);
            function->setLifetimeName(std::move(returnLifetime));
        }
        return function;
    }
    if (!isConst && parsingExtensionBody_ && at(TokenKind::LeftBrace))
    {
        // NS304-T08-F02: extension property (`type Name { get/set }`).
        return parsePropertyTail(
            type, name, TokenKind::Invalid, false, false, false, false, false, false,
            beginIndex, false);
    }

    if (isReferenceReturn)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "ref return is valid only on a function, not a global variable");
    }

    ast::AstNode* global = arena_.make<ast::AstNode>(
        ast::AstNodeKind::GlobalVariableDeclaration,
        tokenRangeFrom(beginIndex));
    global->setTypeSyntax(type);
    global->setText(std::string(name.lexeme()));
    global->setFlag(isConst);

    if (consumeIf(TokenKind::Equal))
    {
        global->setFirst(parseExpression());
    }
    else if (isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const declaration requires an initializer");
    }

    expect(TokenKind::Semicolon, "';'");
    global->setRange(tokenRangeFrom(beginIndex));
    return global;
}

ast::AstNode* Parser::parseFunctionTail(
    ast::TypeSyntax* returnType,
    const lexer::Token& name,
    bool externalOnly)
{
    const std::size_t beginIndex = index_ > 0 ? index_ - 1 : 0;
    ast::AstNode* function = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FunctionDeclaration,
        name.range());
    function->setTypeSyntax(returnType);
    function->setText(std::string(name.lexeme()));
    function->setFlag(externalOnly);

    parseGenericParameterList(function);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, function);

    expect(TokenKind::LeftParen, "'('");
    bool sawFixedParameter = false;
    bool variadic = false;
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (at(TokenKind::Ellipsis))
            {
                if (!sawFixedParameter)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "a variadic C declaration requires at least one fixed parameter before '...'");
                }
                consume();
                variadic = true;
                break;
            }
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr)
            {
                function->addChild(parameter);
                sawFixedParameter = true;
            }
            if (!consumeIf(TokenKind::Comma))
            {
                break;
            }
        }
    }
    function->setVariadic(variadic);
    expect(TokenKind::RightParen, "')'");
    parseThrowsClause(function);
    parseFloatingPointModeClause(function);
    parseFfiUnwindClause(function);
    if (variadic && !externalOnly)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            function->range(),
            "'...' is valid only in a body-less extern C declaration");
    }
    // NSC-LIMIT-007: igual que un metodo -- despues de los parametros, antes del cuerpo o del ';'.
    parseGenericConstraintClauses(function);
    // ...SALVO QUE UNA FUNCION GLOBAL GENERICA NO SE MATERIALIZA.
    //
    // `isGenericMethodTemplateNode` reconoce MethodDeclaration y nada mas, de modo que una
    // `F<T>(...)` de namespace nunca entra en el modelo generico y por tanto nunca produce una
    // especializacion. Aceptar aqui la clausula habria dejado un `where` que NADIE comprueba: la
    // peor de las dos respuestas posibles, porque el autor lo lee como una garantia.
    //
    // Se rechaza y se borra del nodo. La clausula se parsea antes a proposito --y no se salta el
    // texto-- para que sus propios errores de forma se sigan reportando y para que el parser
    // recupere en el mismo sitio que con un metodo.
    if (!function->genericConstraints().empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            function->genericConstraints().front().range,
            "a generic constraint on a global function is not supported yet: a global generic "
            "function is not materialized, so the clause would never be checked; declare '" +
                function->text() + "' as a static method of a class");
        function->clearGenericConstraints();
    }

    if (externalOnly)
    {
        if (!consumeIf(TokenKind::Semicolon))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected ';'");

            // An extern-C declaration is body-less.  Keeping the current token on an
            // unexpected '{' makes the surrounding extern-block loop parse the same
            // token forever.  Consume the balanced block through the regular parser so
            // every diagnostic/dump action is guaranteed to make forward progress.
            if (at(TokenKind::LeftBrace))
            {
                (void)parseBlock();
            }
            else
            {
                synchronizeGlobal();
            }
        }
    }
    else if (consumeIf(TokenKind::Semicolon))
    {
    }
    else if (at(TokenKind::LeftBrace))
    {
        function->setFirst(parseBlock());
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected ';' or function body");
        synchronizeGlobal();
    }

    if (function->first() != nullptr && function->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyFloatingPointModeToTree(function->first(), function->floatingPointMode());
    function->setRange(tokenRangeFrom(beginIndex));
    return function;
}

void Parser::parseFloatingPointModeClause(ast::AstNode* callable)
{
    if (callable == nullptr || !atContextualKeyword("fp") ||
        peek().kind() != TokenKind::LeftParen)
        return;
    consume();
    consume();
    ast::FloatingPointModeSyntaxKind mode = ast::FloatingPointModeSyntaxKind::Inherit;
    if (atContextualKeyword("strict")) mode = ast::FloatingPointModeSyntaxKind::Strict;
    else if (atContextualKeyword("precise")) mode = ast::FloatingPointModeSyntaxKind::Precise;
    else if (atContextualKeyword("fast")) mode = ast::FloatingPointModeSyntaxKind::Fast;
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "floating-point mode must be strict, precise, or fast");
    }
    if (mode != ast::FloatingPointModeSyntaxKind::Inherit) consume();
    expect(TokenKind::RightParen, "')' after floating-point mode");
    callable->setFloatingPointMode(mode);
}

void Parser::parseThrowsClause(ast::AstNode* callable)
{
    if (callable == nullptr || !atContextualKeyword("throws")) return;
    consume();
    if (!canStartTypeAt(index_) && !at(TokenKind::Identifier) &&
        !lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "throws requires an explicit error type");
        return;
    }
    ast::TypeSyntax* errorType = parseType();
    if (errorType != nullptr &&
        errorType->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            errorType->range(),
            "throws error types cannot carry owned or borrowed qualifiers");
    }
    callable->setThrowsTypeSyntax(errorType);
}

void Parser::parseFfiUnwindClause(ast::AstNode* callable)
{
    if (callable == nullptr || !atContextualKeyword("unwind") ||
        peek().kind() != TokenKind::LeftParen) return;
    consume();
    consume();
    if (atContextualKeyword("forbidden"))
    {
        consume();
        callable->setFfiUnwindPolicy(ast::FfiUnwindSyntaxKind::Forbidden);
    }
    else if (atContextualKeyword("abort"))
    {
        consume();
        callable->setFfiUnwindPolicy(ast::FfiUnwindSyntaxKind::Abort);
    }
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "FFI unwind policy must be forbidden or abort");
    }
    expect(TokenKind::RightParen, "')' after FFI unwind policy");
}

ast::AstNode* Parser::parseParameter()
{
    const std::size_t beginIndex = index_;
    const bool parameterPack = atContextualKeyword("params");
    if (parameterPack)
    {
        consume();
    }
    // C4 "language references": `ref`/`out`/`in` before the type. The parameter's TYPE becomes a
    // pointer to what was written -- which is what a by-reference parameter actually receives,
    // and which reuses the pointer machinery C1 already certified end to end instead of adding a
    // second calling convention. Which of the three it is stays on the node, because the
    // difference between them is a matter of rules, not of representation.
    std::string passKind;
    std::string lifetimeName;
    if ((atContextualKeyword("ref") || atContextualKeyword("out") || at(TokenKind::KwIn)) &&
        (peek().kind() == TokenKind::Identifier || lexer::isBuiltinTypeKeyword(peek().kind()) ||
         peek().kind() == TokenKind::LeftBracket))
    {
        passKind = at(TokenKind::KwIn) ? "in" : std::string(current().lexeme());
        consume();
        lifetimeName = parseOptionalLifetimeAnnotation("by-reference parameter");
    }
    ast::TypeSyntax* type = parseType();
    if (type != nullptr && !passKind.empty())
    {
        type->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Pointer, nullptr, type->range()});
    }
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "parameter name");
        return nullptr;
    }

    const lexer::Token& name = consume();
    ast::AstNode* parameter = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ParameterDeclaration,
        tokenRangeFrom(beginIndex));
    parameter->setTypeSyntax(type);
    parameter->setText(std::string(name.lexeme()));
    parameter->setLifetimeName(std::move(lifetimeName));
    if (parameterPack && !passKind.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parameter->range(),
            "a params ReadOnlySpan<T> parameter cannot also be ref, out, or in");
    }
    if (parameterPack) parameter->setAuxiliaryText("params");
    else if (!passKind.empty()) parameter->setAuxiliaryText("pass:" + passKind);
    const auto appendContract = [&](std::string contract)
    {
        std::string marker = parameter->auxiliaryText();
        if (!marker.empty()) marker += '|';
        marker += std::move(contract);
        parameter->setAuxiliaryText(std::move(marker));
    };
    while ((atContextualKeyword("length") || atContextualKeyword("callback")) &&
           peek().kind() == TokenKind::LeftParen)
    {
        const bool length = atContextualKeyword("length");
        consume();
        consume();
        if (length)
        {
            if (!at(TokenKind::Identifier))
                expect(TokenKind::Identifier, "length parameter name");
            else
                appendContract("ffi:length:" + std::string(consume().lexeme()));
        }
        else if (atContextualKeyword("scoped") || atContextualKeyword("escaping"))
        {
            appendContract("ffi:callback:" + std::string(consume().lexeme()));
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "callback lifetime must be scoped or escaping");
        }
        expect(TokenKind::RightParen, "')' after FFI parameter contract");
    }
    if (consumeIf(TokenKind::Equal))
    {
        parameter->setFirst(parseAssignmentExpression());
        parameter->setRange(tokenRangeFrom(beginIndex));
    }
    return parameter;
}

ast::AstNode* Parser::parseBlock()
{
    const std::size_t beginIndex = index_;
    expect(TokenKind::LeftBrace, "'{'");
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, tokenRangeFrom(beginIndex));
    // A defer protects every statement that follows it in this lexical block. `destination`
    // therefore advances into the generated try body after every registration. A second defer
    // nests another try and naturally produces LIFO cleanup without teaching later stages a
    // second exit model.
    ast::AstNode* destination = block;

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* statement = parseStatement();
        if (statement != nullptr)
        {
            if (statement->kind() == ast::AstNodeKind::BlockStatement &&
                statement->auxiliaryText() == "defer-registration")
            {
                ast::AstNode* generatedTry = nullptr;
                for (ast::AstNode* generated : statement->children())
                {
                    destination->addChild(generated);
                    if (generated != nullptr && generated->kind() == ast::AstNodeKind::TryStatement)
                        generatedTry = generated;
                }
                if (generatedTry != nullptr && generatedTry->first() != nullptr)
                    destination = generatedTry->first();
            }
            else if (statement->kind() == ast::AstNodeKind::BlockStatement &&
                statement->auxiliaryText() == "conditional-pattern-inline")
            {
                for (ast::AstNode* child : statement->children()) destination->addChild(child);
            }
            else
            {
                destination->addChild(statement);
            }
        }
        if (index_ == before)
        {
            consume();
        }
    }

    expect(TokenKind::RightBrace, "'}'");
    block->setRange(tokenRangeFrom(beginIndex));
    return block;
}

ast::AstNode* Parser::parseStatement()
{
    if (at(TokenKind::LeftBrace)) return parseBlock();
    if (atContextualKeyword("fp") && peek().kind() == TokenKind::LeftParen)
    {
        consume();
        consume();
        ast::FloatingPointModeSyntaxKind mode = ast::FloatingPointModeSyntaxKind::Inherit;
        if (atContextualKeyword("strict")) mode = ast::FloatingPointModeSyntaxKind::Strict;
        else if (atContextualKeyword("precise")) mode = ast::FloatingPointModeSyntaxKind::Precise;
        else if (atContextualKeyword("fast")) mode = ast::FloatingPointModeSyntaxKind::Fast;
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "floating-point scope mode must be strict, precise, or fast");
        }
        std::string modeName;
        if (mode != ast::FloatingPointModeSyntaxKind::Inherit)
        {
            modeName = std::string(current().lexeme());
            consume();
        }
        expect(TokenKind::RightParen, "')' after floating-point scope mode");
        ast::AstNode* block = parseBlock();
        block->setAuxiliaryText("fp-scope:" + modeName);
        applyFloatingPointModeToTree(block, mode);
        return block;
    }
    if (atContextualKeyword("unsafe") &&
        (peek().kind() == TokenKind::LeftBrace || peek().kind() == TokenKind::LeftParen))
    {
        consume();
        std::string reason;
        if (consumeIf(TokenKind::LeftParen))
        {
            if (at(TokenKind::StringLiteral))
                reason = std::string(consume().lexeme());
            else
                expect(TokenKind::StringLiteral, "unsafe block reason string");
            expect(TokenKind::RightParen, "')' after unsafe block reason");
        }
        ast::AstNode* block = parseBlock();
        block->setAuxiliaryText("unsafe-block");
        block->setText(reason);
        if (language::unsafeBlockTooBroad(block->children().size(), !reason.empty()))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::NS3030MissingEffectContext,
                diagnostics::DiagnosticSeverity::Warning,
                block->range(),
                "unsafe block covers more than eight statements; narrow it or write unsafe(\"reason\") { ... }");
        }
        return block;
    }
    if (atContextualKeyword("asm") &&
        (peek().kind() == TokenKind::LeftBrace || peek().kind() == TokenKind::LeftParen))
        return parseInlineAssemblyStatement();
    const auto beginsConditionalPattern = [&]() noexcept
    {
        return peek().kind() == TokenKind::LeftParen &&
            peek(2).kind() == TokenKind::Identifier && peek(2).lexeme() == "let";
    };
    if ((at(TokenKind::KwIf) || at(TokenKind::KwWhile)) && beginsConditionalPattern())
        return parseConditionalPatternStatement();
    if (atContextualKeyword("let")) return parseConditionalPatternStatement();
    if (at(TokenKind::KwIf)) return parseIfStatement();
    if (at(TokenKind::KwWhile)) return parseWhileStatement();
    if (at(TokenKind::KwDo)) return parseDoWhileStatement();
    if (at(TokenKind::KwFor)) return parseForStatement();
    if (at(TokenKind::KwForeach)) return parseForeachStatement();
    if (at(TokenKind::KwSwitch)) return parseSwitchStatement();
    if (startsParenthesizedBlock("match"))
        return parseMatchStatement();

    if (!comprehensionContexts_.empty() && atContextualKeyword("yield"))
        return parseComprehensionYieldStatement();

    if (atContextualKeyword("yield") &&
        (peek().kind() == TokenKind::KwReturn || peek().kind() == TokenKind::KwBreak))
    {
        // NS304-T06-F01: `yield return expr` / `yield break` ya no se rechazan con NS3049. El
        // lowering productivo a frame/state machine es T06-F03.
        const std::size_t beginIndex = index_;
        consume(); // yield
        ast::AstNode* statement;
        if (at(TokenKind::KwReturn))
        {
            consume(); // return
            ast::AstNode* value = parseExpression();
            statement = arena_.make<ast::AstNode>(
                ast::AstNodeKind::YieldReturnStatement, tokenRangeFrom(beginIndex));
            statement->setFirst(value);
        }
        else
        {
            consume(); // break
            statement = arena_.make<ast::AstNode>(
                ast::AstNodeKind::YieldBreakStatement, tokenRangeFrom(beginIndex));
        }
        expect(TokenKind::Semicolon, "';' after 'yield return'/'yield break'");
        return statement;
    }

    if (at(TokenKind::KwStatic))
    {
        const SourceRange staticRange = consume().range();
        const bool declaration =
            (atContextualKeyword("pin") &&
             (peek().kind() == TokenKind::KwRaii ||
              (peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref"))) ||
            at(TokenKind::KwRaii) || at(TokenKind::KwConst) ||
            at(TokenKind::KwVolatile) || at(TokenKind::KwVar) ||
            (atContextualKeyword("ref") &&
             (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
              peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii)) ||
            (looksLikeVariableDeclaration() && !looksLikeClosedGenericTypeReference());
        if (declaration) return parseVariableDeclaration(true, true);
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            staticRange,
            "static statement must declare a local variable");
        synchronizeStatement();
        return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, staticRange);
    }

    if (at(TokenKind::KwBreak) || at(TokenKind::KwContinue))
    {
        const std::size_t beginIndex = index_;
        const TokenKind kind = consume().kind();
        ast::AstNode* value = nullptr;
        std::string targetLabel;
        if (kind == TokenKind::KwBreak)
        {
            if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Colon)
            {
                targetLabel = std::string(consume().lexeme());
                consume();
            }
            if (!at(TokenKind::Semicolon)) value = parseExpression();
        }
        else if (at(TokenKind::Identifier))
        {
            targetLabel = std::string(consume().lexeme());
        }
        expect(TokenKind::Semicolon, "';'");
        ast::AstNode* exit = arena_.make<ast::AstNode>(
            kind == TokenKind::KwBreak ? ast::AstNodeKind::BreakStatement : ast::AstNodeKind::ContinueStatement,
            tokenRangeFrom(beginIndex));
        exit->setFirst(value);
        exit->setText(std::move(targetLabel));
        return exit;
    }

    // NativeSharp 3.0 T08-F06. `must_tail` is a call-site contract rather than a
    // declaration hint: the following return must lower to one ABI-compatible tail call.
    if (atContextualKeyword("must_tail"))
    {
        const SourceRange attributeRange = consume().range();
        if (!at(TokenKind::KwReturn))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "must_tail must be followed by a return statement");
            synchronizeStatement();
            return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, attributeRange);
        }
        ast::AstNode* statement = parseReturnStatement();
        statement->setFlag(true);
        return statement;
    }
    if (at(TokenKind::KwReturn)) return parseReturnStatement();
    if (atContextualKeyword("defer")) return parseDeferStatement();
    if (at(TokenKind::KwTry)) return parseTryStatement();
    if (at(TokenKind::KwThrow)) return parseThrowStatement();
    if (at(TokenKind::KwUsing)) return parseUsingStatement();
    if (at(TokenKind::KwStaticAssert)) return parseStaticAssertStatement();

    if (at(TokenKind::KwVar) && peek().kind() == TokenKind::LeftParen)
    {
        const std::size_t beginIndex = index_;
        const SourceRange featureRange = consume().range();
        ast::AstNode* pattern = parseDeconstructionPattern();
        ast::AstNode* declaration = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, featureRange);
        if (pattern != nullptr && pattern->kind() == ast::AstNodeKind::TupleExpression)
        {
            for (ast::AstNode* binding : pattern->children()) declaration->addChild(binding);
        }
        expect(TokenKind::Equal, "'=' after deconstruction bindings");
        declaration->setFirst(parseAssignmentExpression());
        expect(TokenKind::Semicolon, "';'");
        declaration->setRange(tokenRangeFrom(beginIndex));
        return declaration;
    }

    // ID 131, la otra mitad. Al principio de una sentencia, `Caja<int>` puede empezar DOS cosas:
    // una declaracion (`Caja<int> c = ...`) o una expresion (`Caja<int>.Cuenta = 5;`). Las
    // distingue lo que sigue al `>` de cierre: un identificador en la primera, un `.` en la
    // segunda -- y `looksLikeClosedGenericTypeReference` exige exactamente `.` mas identificador,
    // que no puede empezar ninguna declaracion.
    //
    // Sin esto, `canStartTypeAt` decia "declaracion", `parseVariableDeclaration` leia el tipo y
    // pedia un nombre de variable, y el error era "expected variable name, found '.'": un
    // diagnostico que no menciona ni genericos ni miembros estaticos y manda a mirar donde no es.
    if (looksLikeLocalFunction())
    {
        // NS304-T04-F06: funcion local `type name (...)` baja a lambda con tipo natural.
        return parseLocalFunctionAsLambda();
    }
    if ((atContextualKeyword("pin") &&
         (peek().kind() == TokenKind::KwRaii ||
          (peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref"))) ||
        at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVolatile) || at(TokenKind::KwVar) ||
        (atContextualKeyword("ref") &&
         (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
          peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii)) ||
        (looksLikeVariableDeclaration() && !looksLikeClosedGenericTypeReference()))
    {
        return parseVariableDeclaration(true);
    }

    const std::size_t beginIndex = index_;
    ast::AstNode* expression = parseExpression();
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExpressionStatement,
        expression != nullptr ? expression->range() : current().range());
    statement->setFirst(expression);
    expect(TokenKind::Semicolon, "';'");
    statement->setRange(tokenRangeFrom(beginIndex));
    return statement;
}

ast::AstNode* Parser::parseDeconstructionPattern()
{
    const SourceRange bindingStart = current().range();
    std::string bindingMode = "value";
    if (atContextualKeyword("ref"))
    {
        consume();
        bindingMode = "ref";
    }
    else if (consumeIf(TokenKind::KwMove))
    {
        bindingMode = "move";
    }

    if (at(TokenKind::Identifier))
    {
        const lexer::Token& name = consume();
        ast::AstNode* binding = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IdentifierExpression,
            bindingMode == "value" ? name.range() : join(bindingStart, name.range()));
        binding->setText(std::string(name.lexeme()));
        binding->setAuxiliaryText(std::move(bindingMode));
        return binding;
    }

    if (bindingMode != "value")
    {
        expect(TokenKind::Identifier, "deconstruction binding name after binding mode");
        return makeErrorExpression();
    }

    if (!at(TokenKind::LeftParen))
    {
        expect(TokenKind::LeftParen, "'(' in deconstruction pattern");
        return makeErrorExpression();
    }

    const SourceRange open = consume().range();
    ast::AstNode* tuple = arena_.make<ast::AstNode>(ast::AstNodeKind::TupleExpression, open);
    if (at(TokenKind::RightParen))
    {
        const SourceRange close = consume().range();
        tuple->setRange(join(open, close));
        return tuple;
    }

    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier) && !at(TokenKind::KwMove) && !at(TokenKind::LeftParen))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected deconstruction binding name, '_', or nested pattern");
            consume();
        }
        else
        {
            tuple->addChild(parseDeconstructionPattern());
        }
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightParen, "')' after deconstruction pattern");
    if (index_ != 0)
        tuple->setRange(SourceRange(open.begin(), tokens_[index_ - 1].range().end()));
    return tuple;
}

namespace
{
bool isInlineAssemblyRegisterName(std::string_view name) noexcept
{
    if (name.empty()) return false;
    for (const unsigned char byte : name)
    {
        if (std::isalnum(byte) == 0 && byte != '_') return false;
    }
    return true;
}

std::string canonicalInlineAssemblyRegister(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte)
    {
        return static_cast<char>(std::tolower(byte));
    });
    return value;
}

std::string_view trimAssemblyText(std::string_view text) noexcept
{
    const std::size_t begin = text.find_first_not_of(" \t\v\f");
    if (begin == std::string_view::npos) return {};
    const std::size_t end = text.find_last_not_of(" \t\v\f");
    return text.substr(begin, end - begin + 1);
}

bool isInlineAssemblyBranchMnemonic(std::string_view mnemonic) noexcept
{
    static const std::unordered_set<std::string_view> branches = {
        "jmp", "je", "jne", "jz", "jnz", "ja", "jae", "jb", "jbe", "jg", "jge",
        "jl", "jle", "jo", "jno", "js", "jns", "jp", "jnp", "jc", "jnc",
        "jcxz", "jecxz", "jrcxz", "loop", "loope", "loopne"};
    return branches.contains(mnemonic);
}

std::string_view fixedInlineAssemblyRegister(std::string_view constraint) noexcept
{
    const std::size_t open = constraint.find('{');
    if (open == std::string_view::npos || constraint.back() != '}') return {};
    return constraint.substr(open + 1, constraint.size() - open - 2);
}

bool isInlineAssemblyStackPointer(std::string_view name) noexcept
{
    return name == "rsp" || name == "esp" || name == "sp" || name == "spl";
}

bool isWindowsX64NonvolatileRegister(std::string_view name) noexcept
{
    static constexpr std::array<std::string_view, 28> names = {
        "rbx", "ebx", "bx", "bl", "bh",
        "rbp", "ebp", "bp", "bpl",
        "rdi", "edi", "di", "dil",
        "rsi", "esi", "si", "sil",
        "r12", "r12d", "r12w", "r12b",
        "r13", "r13d", "r13w", "r13b",
        "r14", "r15", "xmm6"};
    if (std::find(names.begin(), names.end(), name) != names.end()) return true;
    if (name.starts_with("r14") || name.starts_with("r15")) return true;
    if (name.starts_with("xmm"))
    {
        const std::string_view ordinal = name.substr(3);
        return ordinal == "7" || ordinal == "8" || ordinal == "9" || ordinal == "10" ||
            ordinal == "11" || ordinal == "12" || ordinal == "13" || ordinal == "14" ||
            ordinal == "15";
    }
    return false;
}

bool isInlineAssemblyConstraint(std::string_view constraint, bool output) noexcept
{
    if (!output)
    {
        if (constraint == "r" || constraint == "i" || constraint == "m") return true;
        return constraint.size() >= 3 && constraint.front() == '{' &&
            constraint.back() == '}' &&
            isInlineAssemblyRegisterName(constraint.substr(1, constraint.size() - 2));
    }

    if (constraint == "=r" || constraint == "+r" ||
        constraint == "=m" || constraint == "+m") return true;
    if (constraint.size() < 4 || (constraint.front() != '=' && constraint.front() != '+') ||
        constraint[1] != '{' || constraint.back() != '}')
    {
        return false;
    }
    return isInlineAssemblyRegisterName(constraint.substr(2, constraint.size() - 3));
}
}

ast::AstNode* Parser::parseStaticAssertStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();

    std::string message;
    if (consumeIf(TokenKind::Comma))
    {
        if (at(TokenKind::StringLiteral))
        {
            message = std::get<std::string>(consume().value());
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "static_assert message must be a string literal");
        }
    }
    expect(TokenKind::RightParen, "')'");
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StaticAssertStatement,
        tokenRangeFrom(beginIndex));
    statement->setFirst(condition);
    statement->setText(std::move(message));
    return statement;
}

ast::AstNode* Parser::parseInlineAssemblyStatement()
{
    const std::size_t beginIndex = index_;
    consume(); // contextual `asm`

    std::string targetMetadata;
    std::string dialectMetadata;
    if (consumeIf(TokenKind::LeftParen))
    {
        do
        {
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "asm header option name");
                break;
            }
            const lexer::Token& optionToken = consume();
            const std::string option(optionToken.lexeme());
            if (!expect(TokenKind::Equal, "'=' after asm header option")) break;
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "asm header option value");
                break;
            }
            const lexer::Token& valueToken = consume();
            const std::string value(valueToken.lexeme());

            if (option == "target" && value == "x64")
            {
                targetMetadata = std::string(target::TargetInfo::WindowsX64().llvmTriple());
            }
            else if (option == "dialect" && value == "intel")
            {
                dialectMetadata = "intel";
            }
            else
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
                    diagnostics::DiagnosticSeverity::Error,
                    valueToken.range(),
                    "unsupported asm " + option + " value '" + value + "'");
            }
        }
        while (consumeIf(TokenKind::Comma));
        expect(TokenKind::RightParen, "')' after asm header");
    }
    expect(TokenKind::LeftBrace, "'{' after asm");

    // Item 288: the lexer owns the outer-brace depth and hands the parser exact physical-line
    // records. The parent retains the complete body byte-for-byte. Non-blank records become child
    // nodes whose range starts at the first significant byte, so subsequent operand diagnostics
    // can name the real instruction column without re-scanning the source file.
    std::string exactBody;
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::InlineAssemblyStatement,
        tokenRangeFrom(beginIndex));
    if (!targetMetadata.empty() || !dialectMetadata.empty())
    {
        statement->setAuxiliaryText(
            "target:" + targetMetadata + ";dialect:" + dialectMetadata);
    }
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const lexer::Token& token = current();
        exactBody += token.lexeme();
        if (token.kind() == TokenKind::AssemblyText)
        {
            const std::string_view raw = token.lexeme();
            const std::size_t first = raw.find_first_not_of(" \t\v\f");
            if (first != std::string_view::npos)
            {
                const std::size_t last = raw.find_last_not_of(" \t\v\f");
                const SourceRange significantRange(
                    SourceLocation(token.range().begin().fileId(),
                        token.range().begin().offset() + static_cast<std::uint32_t>(first)),
                    SourceLocation(token.range().end().fileId(),
                        token.range().begin().offset() + static_cast<std::uint32_t>(last + 1)));
                ast::AstNode* line = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::InlineAssemblyLine,
                    significantRange);
                line->setText(std::string(raw.substr(first, last - first + 1)));
                statement->addChild(line);
            }
        }
        consume();
    }

    expect(TokenKind::RightBrace, "'}' to close asm block");
    statement->setRange(tokenRangeFrom(beginIndex));
    statement->setText(std::move(exactBody));
    validateInlineAssemblyControlFlow(statement);
    bool hasOperandClauses = false;
    if (atContextualKeyword("out"))
    {
        parseInlineAssemblyOutputClause(statement);
        hasOperandClauses = true;
    }
    if (at(TokenKind::KwIn))
    {
        parseInlineAssemblyInputClause(statement);
        hasOperandClauses = true;
    }
    if (atContextualKeyword("clobber"))
    {
        parseInlineAssemblyClobberClause(statement);
        hasOperandClauses = true;
    }
    if (hasOperandClauses)
    {
        expect(TokenKind::Semicolon, "';' after asm operand clauses");
        statement->setRange(tokenRangeFrom(beginIndex));
    }
    validateInlineAssemblyPlaceholders(statement);
    return statement;
}

void Parser::validateInlineAssemblyPlaceholders(ast::AstNode* statement)
{
    std::size_t inputCount = 0;
    std::size_t outputCount = 0;
    for (const ast::AstNode* child : statement->children())
    {
        inputCount += child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand ? 1U : 0U;
        outputCount += child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand ? 1U : 0U;
    }

    for (const ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        for (std::size_t index = 0; index < code.size(); ++index)
        {
            if (code[index] != '$') continue;
            if (index + 1 < code.size() && code[index + 1] == '$')
            {
                ++index;
                continue;
            }

            const bool output = code.substr(index + 1, 3) == "out";
            std::size_t cursor = index + (output ? 4 : 1);
            bool resolved = false;
            if (!output && cursor < code.size() &&
                (std::isalpha(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_'))
            {
                const std::size_t nameBegin = cursor++;
                while (cursor < code.size() &&
                    (std::isalnum(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_'))
                {
                    ++cursor;
                }
                const std::string_view name = code.substr(nameBegin, cursor - nameBegin);
                resolved = std::any_of(statement->children().begin(), statement->children().end(),
                    [name](const ast::AstNode* child)
                    {
                        return child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand &&
                            child->first() != nullptr && child->first()->text() == name;
                    });
            }
            else
            {
                std::size_t ordinal = 0;
                const std::size_t digitBegin = cursor;
                while (cursor < code.size() &&
                    std::isdigit(static_cast<unsigned char>(code[cursor])))
                {
                    ordinal = ordinal * 10 + static_cast<std::size_t>(code[cursor] - '0');
                    ++cursor;
                }
                const std::size_t bound = output ? outputCount : inputCount;
                resolved = cursor != digitBegin && ordinal < bound;
            }
            const bool malformedSuffix = cursor < code.size() &&
                (std::isalnum(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_');
            if (!resolved || malformedSuffix)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidInlineAssemblyPlaceholder,
                    diagnostics::DiagnosticSeverity::Error,
                    line->range(),
                    output
                        ? "asm output placeholder does not name a declared out operand"
                        : "asm input placeholder does not name a declared in operand");
                break;
            }
            index = cursor - 1;
        }
    }
}

void Parser::validateInlineAssemblyControlFlow(ast::AstNode* statement)
{
    std::unordered_set<std::string> labels;
    for (ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        code = trimAssemblyText(code);
        if (code.empty() || code.back() != ':') continue;
        const std::string label = canonicalInlineAssemblyRegister(
            std::string(trimAssemblyText(code.substr(0, code.size() - 1))));
        if (!isInlineAssemblyRegisterName(label) || !labels.insert(label).second)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm label must be a unique identifier within its block");
            continue;
        }
        line->setAuxiliaryText("label:" + label);
    }

    for (ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine ||
            line->auxiliaryText().starts_with("label:")) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        code = trimAssemblyText(code);
        if (code.empty()) continue;
        const std::size_t separator = code.find_first_of(" \t");
        const std::string mnemonic = canonicalInlineAssemblyRegister(
            std::string(code.substr(0, separator)));
        std::string_view target = separator == std::string_view::npos
            ? std::string_view{} : trimAssemblyText(code.substr(separator + 1));
        if (mnemonic == "ret" || mnemonic == "retq" || mnemonic.starts_with("iret") ||
            mnemonic == "call" || mnemonic == "callq")
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm call/return instructions cannot escape NativeSharp structured control flow");
            continue;
        }
        const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
        const std::string_view requiredFeature = targetInfo.requiredAssemblyFeature(mnemonic);
        if (!targetInfo.supportsCpuFeature(requiredFeature))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm instruction '" + mnemonic + "' requires CPU feature '" +
                    std::string(requiredFeature) + "' outside baseline " +
                    std::string(targetInfo.cpuBaselineName()));
            continue;
        }
        if (!isInlineAssemblyBranchMnemonic(mnemonic)) continue;
        const std::string canonicalTarget = canonicalInlineAssemblyRegister(std::string(target));
        if (!isInlineAssemblyRegisterName(canonicalTarget) || !labels.contains(canonicalTarget))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm branch target must be a label declared in the same block");
            continue;
        }
        line->setAuxiliaryText("branch:" + mnemonic + ":" + canonicalTarget);
    }
}

void Parser::parseInlineAssemblyOutputClause(ast::AstNode* statement)
{
    consume(); // contextual `out`
    if (!expect(TokenKind::LeftParen, "'(' after asm out")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm out clause requires at least one output target");
        consume();
        return;
    }

    std::uint64_t ordinal = 0;
    do
    {
        const std::size_t operandBegin = index_;
        ast::AstNode* target = parseUnaryExpression();
        ast::AstNode* operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::InlineAssemblyOutputOperand,
            tokenRangeFrom(operandBegin));
        operand->setFirst(target);
        operand->setIntegerValue(ordinal++);
        bool explicitConstraint = false;
        operand->setAuxiliaryText(
            parseInlineAssemblyConstraint(true, explicitConstraint));
        operand->setFlag(explicitConstraint);
        operand->setRange(tokenRangeFrom(operandBegin));
        statement->addChild(operand);
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm output targets");
}

void Parser::parseInlineAssemblyInputClause(ast::AstNode* statement)
{
    consume(); // `in`
    if (!expect(TokenKind::LeftParen, "'(' after asm in")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm in clause requires at least one input expression");
        consume();
        return;
    }

    std::uint64_t ordinal = 0;
    do
    {
        const std::size_t operandBegin = index_;
        ast::AstNode* expression = parseAssignmentExpression();
        ast::AstNode* operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::InlineAssemblyInputOperand,
            tokenRangeFrom(operandBegin));
        operand->setFirst(expression);
        operand->setIntegerValue(ordinal++);
        bool explicitConstraint = false;
        operand->setAuxiliaryText(
            parseInlineAssemblyConstraint(false, explicitConstraint));
        operand->setFlag(explicitConstraint);
        operand->setRange(tokenRangeFrom(operandBegin));
        statement->addChild(operand);
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm input operands");
}

void Parser::parseInlineAssemblyClobberClause(ast::AstNode* statement)
{
    consume(); // contextual `clobber`
    if (!expect(TokenKind::LeftParen, "'(' after asm clobber")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm clobber clause requires at least one register name");
        consume();
        return;
    }

    std::unordered_set<std::string> seen;
    std::uint64_t ordinal = 0;
    do
    {
        if (!at(TokenKind::StringLiteral))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "asm register clobber must be a string literal");
            if (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile)) consume();
            continue;
        }

        const lexer::Token& registerToken = consume();
        const std::string registerName = canonicalInlineAssemblyRegister(
            std::get<std::string>(registerToken.value()));
        const bool specialClobber = registerName == "cc" || registerName == "memory";
        bool valid = specialClobber || isInlineAssemblyRegisterName(registerName);
        if (!valid)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                diagnostics::DiagnosticSeverity::Error,
                registerToken.range(),
                "asm clobber requires a register atom, 'cc', or 'memory'");
        }
        else if (!specialClobber && isInlineAssemblyStackPointer(registerName))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                diagnostics::DiagnosticSeverity::Error,
                registerToken.range(),
                "asm cannot clobber the stack pointer; its value and alignment must survive the block");
            valid = false;
        }
        else if (!specialClobber &&
            !target::TargetInfo::WindowsX64().isAssemblyRegister(registerName))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
                diagnostics::DiagnosticSeverity::Error,
                registerToken.range(),
                "unknown x64 register '" + registerName + "'");
            valid = false;
        }
        else if (!seen.insert(registerName).second)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                diagnostics::DiagnosticSeverity::Error,
                registerToken.range(),
                "duplicate asm register clobber '" + registerName + "'");
            valid = false;
        }

        if (valid && !specialClobber)
        {
            for (const ast::AstNode* child : statement->children())
            {
                if (child->kind() != ast::AstNodeKind::InlineAssemblyInputOperand &&
                    child->kind() != ast::AstNodeKind::InlineAssemblyOutputOperand)
                {
                    continue;
                }
                const std::string fixed = canonicalInlineAssemblyRegister(
                    std::string(fixedInlineAssemblyRegister(child->auxiliaryText())));
                if (!fixed.empty() && fixed == registerName)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                        diagnostics::DiagnosticSeverity::Error,
                        registerToken.range(),
                        "asm register '" + registerName +
                            "' cannot be both a fixed operand and a clobber");
                    valid = false;
                    break;
                }
            }
        }

        if (valid)
        {
            ast::AstNode* clobber = arena_.make<ast::AstNode>(
                ast::AstNodeKind::InlineAssemblyClobber,
                registerToken.range());
            clobber->setText(registerName);
            clobber->setIntegerValue(ordinal++);
            clobber->setAuxiliaryText(
                registerName == "cc" ? "flags" :
                registerName == "memory" ? "memory" : "register");
            clobber->setFlag(
                !specialClobber && isWindowsX64NonvolatileRegister(registerName));
            statement->addChild(clobber);
        }
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm clobbers");
}

std::string Parser::parseInlineAssemblyConstraint(
    bool output,
    bool& explicitConstraint)
{
    explicitConstraint = consumeIf(TokenKind::Colon);
    const std::string defaultConstraint = output ? "=r" : "r";
    if (!explicitConstraint) return defaultConstraint;

    if (!at(TokenKind::StringLiteral))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm operand constraint must be a string literal");
        return defaultConstraint;
    }

    const lexer::Token& constraintToken = consume();
    const std::string constraint = std::get<std::string>(constraintToken.value());
    if (!isInlineAssemblyConstraint(constraint, output))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            output
                ? "asm output constraint must be '=r', '+r', '=m', '+m', '={register}', or '+{register}'"
                : "asm input constraint must be 'r', 'i', 'm', or '{register}'");
        return defaultConstraint;
    }
    const std::string fixedRegister = canonicalInlineAssemblyRegister(
        std::string(fixedInlineAssemblyRegister(constraint)));
    if (!fixedRegister.empty() && isInlineAssemblyStackPointer(fixedRegister))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            "asm cannot bind an operand to the stack pointer");
        return defaultConstraint;
    }
    if (!fixedRegister.empty() &&
        !target::TargetInfo::WindowsX64().isAssemblyRegister(fixedRegister))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            "unknown x64 register '" + fixedRegister + "'");
        return defaultConstraint;
    }
    return constraint;
}

ast::AstNode* Parser::parseVariableDeclarator(
    ast::TypeSyntax* type,
    bool cloneType,
    bool isConst,
    bool isRaii,
    bool isVolatile,
    bool isReferenceLocal,
    bool isPinned)
{
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "variable name");
        return nullptr;
    }

    const std::size_t beginIndex = index_;
    const lexer::Token& name = consume();
    ast::AstNode* variable = arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement,
        name.range());
    variable->setTypeSyntax(cloneType ? cloneParsedType(arena_, type) : type);
    variable->setText(std::string(name.lexeme()));
    variable->setFlag(isConst);
    if (isRaii) variable->addModifier(TokenKind::KwRaii);
    if (isVolatile) variable->addModifier(TokenKind::KwVolatile);
    if (isReferenceLocal)
        variable->setAuxiliaryText(isPinned ? "local:pinned-ref" : "local:ref");
    else if (isPinned) variable->setAuxiliaryText("local:pinned");

    if (consumeIf(TokenKind::Equal))
    {
        if (isReferenceLocal)
        {
            expectContextualKeyword("ref", "'ref' before the reference-local initializer");
        }
        variable->setFirst(
            at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    }
    else if (isReferenceLocal)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "reference-local declaration requires '= ref <storage>'");
    }
    else if (isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const declaration requires an initializer");
    }
    variable->setRange(tokenRangeFrom(beginIndex));
    return variable;
}

ast::AstNode* Parser::parseVariableDeclaration(bool expectSemicolon, bool isStaticLocal)
{
    const std::size_t beginIndex = index_;
    const bool isPinnedReferenceSyntax = atContextualKeyword("pin") &&
        peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref";
    const bool isPinned = isPinnedReferenceSyntax ||
        (atContextualKeyword("pin") && peek().kind() == TokenKind::KwRaii);
    if (isPinned) consume();
    const bool isReferenceLocal = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
         peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii);
    if (isReferenceLocal) consume();
    const bool isRaii = consumeIf(TokenKind::KwRaii);
    const bool isVolatile = consumeIf(TokenKind::KwVolatile);
    const bool isConst = consumeIf(TokenKind::KwConst);
    if (isRaii && isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[beginIndex].range(),
            "raii locals cannot also be const");
    }
    // `var` is a full reserved keyword (see TokenKind::KwVar), not contextual like C# -- "var"
    // was unused as an identifier anywhere in the pre-existing codebase, so there is no
    // disambiguation to do. Deliberately bypasses parseType() entirely: a marker TypeSyntax with
    // baseKind == TokenKind::KwVar is constructed directly instead, which is why `var` is
    // syntactically illegal anywhere parseType() is called directly (fields, parameters, return
    // types, casts, generic arguments) -- the correct v1 scope, local-variable type inference
    // only. This also means `owned var`/`borrowed var` are not supported in v1: those qualifiers
    // are consumed by parseType()'s own prefix logic, which this path never reaches -- a
    // deliberate, documented limitation. `raii var`/`const var` DO work, since raii/const are
    // consumed above, before type-parsing begins.
    ast::TypeSyntax* type;
    if (at(TokenKind::KwVar))
    {
        if (isReferenceLocal)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "reference locals require an explicit element type");
        }
        const lexer::Token& varToken = consume();
        type = arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        type = parseType();
    }

    ast::AstNode* variable = parseVariableDeclarator(
        type, false, isConst, isRaii, isVolatile, isReferenceLocal, isPinned);
    if (variable == nullptr)
    {
        synchronizeStatement();
        return nullptr;
    }
    if (isStaticLocal) variable->addModifier(TokenKind::KwStatic);
    ast::AstNode* declarationList = nullptr;
    while (consumeIf(TokenKind::Comma))
    {
        ast::AstNode* declarator = parseVariableDeclarator(
            type, true, isConst, isRaii, isVolatile, isReferenceLocal, isPinned);
        if (declarator == nullptr)
        {
            synchronizeStatement();
            break;
        }
        if (isStaticLocal) declarator->addModifier(TokenKind::KwStatic);
        if (declarationList == nullptr)
        {
            declarationList = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement,
                variable->range());
            declarationList->setAuxiliaryText("declaration-list");
            declarationList->addChild(variable);
        }
        declarationList->addChild(declarator);
    }

    if (expectSemicolon)
    {
        expect(TokenKind::Semicolon, "';'");
    }
    ast::AstNode* const result = declarationList == nullptr ? variable : declarationList;
    result->setRange(tokenRangeFrom(beginIndex));
    return result;
}

namespace
{
const ast::AstNode* conditionDeclarationBinding(
    ast::AstNode* declaration,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (declaration == nullptr)
    {
        return nullptr;
    }

    const ast::AstNode* binding = declaration;
    if (declaration->kind() == ast::AstNodeKind::BlockStatement &&
        declaration->auxiliaryText() == "declaration-list")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a condition declaration must declare exactly one local");
        if (declaration->children().empty())
        {
            return nullptr;
        }
        binding = declaration->children().front();
    }
    return binding;
}
}

// C7 "aggregate initializer". The C form, deliberately: `{}`, `{1, 2}`, a trailing comma, and
// nested braces for a nested aggregate (`Matrix m = { {1, 0}, {0, 1} };`). Semantic analysis is
// what decides whether the target type accepts one and how the elements line up with its fields
// or elements -- the parser only records the shape.
ast::AstNode* Parser::parseAggregateInitializer()
{
    const std::size_t beginIndex = index_;
    const lexer::Token& open = consume();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::AggregateInitializerExpression, open.range());
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        ast::AstNode* element = nullptr;
        if (at(TokenKind::DotDot))
        {
            const SourceRange spread = consume().range();
            ast::AstNode* value = parseExpression();
            element = arena_.make<ast::AstNode>(
                ast::AstNodeKind::SpreadElementExpression,
                join(spread, value != nullptr ? value->range() : spread));
            element->setTokenKind(TokenKind::DotDot);
            element->setFirst(value);
        }
        else if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Equal)
        {
            const std::string memberName(current().lexeme());
            consume();
            consume(); // '='
            element = at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression();
            if (element != nullptr) element->setArgumentName(memberName);
        }
        else
        {
            element = at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression();
        }
        if (element == nullptr)
        {
            break;
        }
        node->addChild(element);
        if (!consumeIf(TokenKind::Comma))
        {
            break;
        }
    }
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

bool Parser::statementAlwaysDiverges(const ast::AstNode* statement) const
{
    if (statement == nullptr) return false;
    switch (statement->kind())
    {
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ThrowStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
            return true;
        case ast::AstNodeKind::BlockStatement:
            return !statement->children().empty() &&
                statementAlwaysDiverges(statement->children().back());
        case ast::AstNodeKind::IfStatement:
            return statement->second() != nullptr && statement->third() != nullptr &&
                statementAlwaysDiverges(statement->second()) &&
                statementAlwaysDiverges(statement->third());
        default:
            return false;
    }
}

ast::AstNode* Parser::parseConditionalPatternStatement()
{
    enum class Form
    {
        If,
        While,
        LetElse
    };

    const std::size_t beginIndex = index_;
    Form form = Form::LetElse;
    SourceRange featureRange = current().range();
    if (at(TokenKind::KwIf))
    {
        form = Form::If;
        featureRange = consume().range();
        expect(TokenKind::LeftParen, "'(' after 'if'");
        expectContextualKeyword("let", "'let' in conditional pattern");
    }
    else if (at(TokenKind::KwWhile))
    {
        form = Form::While;
        featureRange = consume().range();
        expect(TokenKind::LeftParen, "'(' after 'while'");
        expectContextualKeyword("let", "'let' in conditional pattern");
    }
    else
    {
        expectContextualKeyword("let", "'let'");
    }

    Pattern pattern = parsePattern(false);
    expect(TokenKind::Equal, "'=' between conditional pattern and source expression");
    ast::AstNode* sourceExpression = parseAssignmentExpression();
    if (form != Form::LetElse) expect(TokenKind::RightParen, "')' after conditional pattern");

    const auto collectBindings = [&](const Pattern& root)
    {
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::function<void(const Pattern&)> visit = [&](const Pattern& currentPattern)
        {
            if (currentPattern.kind == PatternKind::Binding)
            {
                bindings.emplace_back(currentPattern.name, currentPattern.bindingMode);
                return;
            }
            for (const Pattern& child : currentPattern.children) visit(child);
        };
        visit(root);
        std::ranges::sort(bindings, [](const auto& left, const auto& right)
        {
            if (left.first != right.first) return left.first < right.first;
            return left.second < right.second;
        });
        return bindings;
    };
    std::function<void(const Pattern&, bool)> validateBindingModes =
        [&](const Pattern& currentPattern, bool preservesAddress)
    {
        if (currentPattern.kind == PatternKind::Binding &&
            currentPattern.bindingMode != PatternBindingMode::Value && !preservesAddress)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                currentPattern.range,
                "ref/move pattern binding cannot cross a tuple deconstruction or variant "
                "payload copy; bind the aggregate first");
        }
        const bool childPreservesAddress = preservesAddress &&
            currentPattern.kind != PatternKind::Tuple && currentPattern.kind != PatternKind::Variant;
        for (const Pattern& child : currentPattern.children)
            validateBindingModes(child, childPreservesAddress);
    };
    std::function<void(const Pattern&)> validateOrBindings = [&](const Pattern& currentPattern)
    {
        if (currentPattern.kind == PatternKind::Or && !currentPattern.children.empty())
        {
            const auto expected = collectBindings(currentPattern.children.front());
            bool identical = true;
            for (std::size_t index = 1; index < currentPattern.children.size(); ++index)
                identical = identical && collectBindings(currentPattern.children[index]) == expected;
            if (!identical)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    currentPattern.range,
                    "OR-pattern alternatives must bind identical names with identical "
                    "value/ref/move modes");
            }
        }
        for (const Pattern& child : currentPattern.children) validateOrBindings(child);
    };
    validateBindingModes(pattern, true);
    validateOrBindings(pattern);

    ast::AstNode* successBody = nullptr;
    ast::AstNode* failureBody = nullptr;
    if (form == Form::If)
    {
        successBody = parseEmbeddedStatementBlock();
        if (consumeIf(TokenKind::KwElse))
        {
            if (at(TokenKind::KwIf))
            {
                const bool conditional = peek().kind() == TokenKind::LeftParen &&
                    peek(2).kind() == TokenKind::Identifier && peek(2).lexeme() == "let";
                failureBody = conditional
                    ? parseConditionalPatternStatement()
                    : parseIfStatement();
            }
            else
            {
                failureBody = parseEmbeddedStatementBlock();
            }
        }
    }
    else if (form == Form::While)
    {
        successBody = parseEmbeddedStatementBlock();
    }
    else
    {
        expect(TokenKind::KwElse, "'else' after let pattern initializer");
        failureBody = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
        if (failureBody == nullptr)
            expect(TokenKind::LeftBrace, "'{' to open the divergent let-else body");
        if (!statementAlwaysDiverges(failureBody))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                failureBody != nullptr ? failureBody->range() : featureRange,
                "the else body of a let-else statement must diverge with return, throw, break, "
                "continue, or an if whose branches both diverge");
        }
        expect(TokenKind::Semicolon, "';' after let-else statement");
    }

    const SourceRange statementRange = tokenRangeFrom(beginIndex);
    const std::string temporary =
        "$__nsc_conditional_pattern_" + std::to_string(nextMatchTemporary_++);
    const std::string matched = temporary + "_matched";
    const auto bindings = collectBindings(pattern);
    const bool hasReferenceBinding = std::ranges::any_of(bindings, [](const auto& binding)
    {
        return binding.second == PatternBindingMode::Reference ||
                binding.second == PatternBindingMode::ReferenceMutable;
    });
    const bool hasMoveBinding = std::ranges::any_of(bindings, [](const auto& binding)
    {
        return binding.second == PatternBindingMode::Move;
    });

    const auto addPrelude = [&](ast::AstNode* destination)
    {
        ast::TypeSyntax* inferred = arena_.make<ast::TypeSyntax>(
            featureRange, TokenKind::KwVar, "var");
        ast::AstNode* declaration = makeVariableDeclaration(
            inferred, temporary, sourceExpression, false, !hasReferenceBinding && !hasMoveBinding);
        if (hasReferenceBinding) declaration->setAuxiliaryText("local:ref");
        destination->addChild(declaration);
        ast::TypeSyntax* boolType = arena_.make<ast::TypeSyntax>(
            featureRange, TokenKind::KwBool, "bool");
        destination->addChild(makeVariableDeclaration(
            boolType,
            matched,
            makeBooleanLiteral(featureRange, false),
            false,
            false));
    };
    const auto makeNotMatched = [&]()
    {
        ast::AstNode* result = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, statementRange);
        result->setTokenKind(TokenKind::Bang);
        result->setFirst(makeIdentifierExpression(statementRange, matched));
        return result;
    };
    const auto makeSuccess = [&](bool includeBody)
    {
        ast::AstNode* body = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, statementRange);
        body->addChild(makeExpressionStatement(makeAssignmentExpression(
            makeIdentifierExpression(statementRange, matched),
            makeBooleanLiteral(statementRange, true))));
        if (includeBody && successBody != nullptr)
            for (ast::AstNode* child : successBody->children()) body->addChild(child);
        return body;
    };
    PatternExpressionFactory root = [&, statementRange]
    {
        return makeIdentifierExpression(statementRange, temporary);
    };

    if (form == Form::While)
    {
        ast::AstNode* iteration = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, statementRange);
        addPrelude(iteration);
        iteration->addChild(lowerPattern(pattern, root, makeSuccess(true), temporary));
        ast::AstNode* stopBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, statementRange);
        stopBody->addChild(arena_.make<ast::AstNode>(
            ast::AstNodeKind::BreakStatement, statementRange));
        ast::AstNode* stop = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, statementRange);
        stop->setFirst(makeNotMatched());
        stop->setSecond(stopBody);
        iteration->addChild(stop);

        ast::AstNode* loop = arena_.make<ast::AstNode>(
            ast::AstNodeKind::WhileStatement, statementRange);
        loop->setFirst(makeBooleanLiteral(featureRange, true));
        loop->setSecond(iteration);
        return loop;
    }

    ast::AstNode* lowered = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    addPrelude(lowered);
    lowered->addChild(lowerPattern(
        pattern,
        root,
        makeSuccess(form == Form::If),
        temporary,
        form != Form::LetElse));

    if (failureBody != nullptr)
    {
        ast::AstNode* failure = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, statementRange);
        failure->setFirst(makeNotMatched());
        failure->setSecond(failureBody);
        lowered->addChild(failure);
    }

    if (form == Form::LetElse)
    {
        emitPatternBindings(pattern, root, lowered, temporary);
        lowered->setAuxiliaryText("conditional-pattern-inline");
    }
    return lowered;
}

ast::AstNode* Parser::parseIfStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* conditionDeclaration = nullptr;
    ast::AstNode* condition = nullptr;
    if (looksLikeConditionDeclaration())
    {
        conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* thenBlock = parseEmbeddedStatementBlock();

    ast::AstNode* elseNode = nullptr;
    if (consumeIf(TokenKind::KwElse))
    {
        if (at(TokenKind::KwIf))
        {
            elseNode = parseIfStatement();
        }
        else if (at(TokenKind::LeftBrace))
        {
            elseNode = parseBlock();
        }
        else
        {
            elseNode = parseEmbeddedStatementBlock();
        }
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IfStatement, tokenRangeFrom(beginIndex));
    node->setFirst(condition);
    node->setSecond(thenBlock);
    node->setThird(elseNode);
    if (conditionDeclaration != nullptr)
    {
        ast::AstNode* lowered = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            tokenRangeFrom(beginIndex));
        lowered->setAuxiliaryText("condition-declaration");
        lowered->addChild(conditionDeclaration);
        lowered->addChild(node);
        return lowered;
    }
    return node;
}

ast::AstNode* Parser::parseEmbeddedStatementBlock()
{
    if (at(TokenKind::LeftBrace)) return parseBlock();

    // In the embedded-statement grammar a lone semicolon is the empty statement. Represent it
    // as the same empty block downstream already understands; this keeps flow, cleanup, ranges,
    // and dangling-else handling identical without introducing a backend-only AST node.
    if (at(TokenKind::Semicolon))
    {
        const SourceRange range = consume().range();
        return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    }

    // T29 keeps every downstream invariant intact: the parser accepts one embedded statement,
    // then gives semantic analysis/HIR the same BlockStatement shape produced by explicit braces.
    // Nested `if` is parsed before it is wrapped, so its `else` binds to the nearest open `if`.
    ast::AstNode* statement = parseStatement();
    if (statement != nullptr &&
        (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
         statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement ||
         (statement->kind() == ast::AstNodeKind::BlockStatement &&
          statement->auxiliaryText() == "conditional-pattern-inline")))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "a declaration used as an if or else body requires an explicit '{ ... }' block");
    }
    const SourceRange range = statement != nullptr ? statement->range() : current().range();
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    if (statement != nullptr) block->addChild(statement);
    return block;
}

ast::AstNode* Parser::parseWhileStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* conditionDeclaration = nullptr;
    ast::AstNode* condition = nullptr;
    if (looksLikeConditionDeclaration())
    {
        conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after while condition");
    }

    const SourceRange statementRange = tokenRangeFrom(beginIndex);
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::WhileStatement, statementRange);
    if (conditionDeclaration == nullptr)
    {
        node->setFirst(condition);
        node->setSecond(body);
        return node;
    }

    // A while condition declaration is evaluated once per iteration. Lowering it outside the
    // loop would retain stale state and delay cleanup until after every iteration. This is the
    // same canonical shape used by `while let`: an infinite loop whose iteration scope owns the
    // declaration, exits when the declared bool is false, and then executes the original body.
    ast::AstNode* iteration = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    iteration->setAuxiliaryText("condition-declaration-iteration");
    iteration->addChild(conditionDeclaration);

    ast::AstNode* stopCondition = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, condition->range());
    stopCondition->setTokenKind(TokenKind::Bang);
    stopCondition->setFirst(condition);
    ast::AstNode* stopBody = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    stopBody->addChild(arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, condition->range()));
    ast::AstNode* stop = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, statementRange);
    stop->setFirst(stopCondition);
    stop->setSecond(stopBody);
    iteration->addChild(stop);
    if (body != nullptr)
    {
        iteration->addChild(body);
    }

    node->setFirst(makeBooleanLiteral(statementRange, true));
    node->setSecond(iteration);
    return node;
}

// `do { ... } while (condition);`
//
// The node carries its operands in the SAME slots as a while statement -- first() is the
// condition, second() is the body -- even though the source writes them the other way round.
// That is deliberate: everything upstream of the backend (the body model's loop analysis, the
// ownership flow across the back edge, the HIR walk) treats the two loops identically, and the
// only real difference is that the body runs before the first test. Keeping the slots aligned
// means those passes share one implementation instead of two that must be kept in step.
//
// The trailing `;` is required, as in C and C#.
ast::AstNode* Parser::parseDoWhileStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after 'do'");
    }
    expect(TokenKind::KwWhile, "'while' after the do-loop body");
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();
    expect(TokenKind::RightParen, "')'");
    expect(TokenKind::Semicolon, "';' after the do-while condition");

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::DoWhileStatement, tokenRangeFrom(beginIndex));
    node->setFirst(condition);
    node->setSecond(body);
    return node;
}

bool Parser::atSwitchLabel() const noexcept
{
    if (at(TokenKind::KwCase)) return true;
    if (!at(TokenKind::KwDefault)) return false;
    return index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind() == TokenKind::Colon;
}

// `switch (governing) { case a: case b: ...  default: ... }`
//
// SHAPE. The statement node keeps the governing expression in first() and one SwitchSection per
// section in children(), in source order. A section keeps its labels first and its statements
// after, with the label count in integerValue() -- so no consumer ever has to tell the two runs
// apart by inspecting node kinds, which is the sort of thing that silently stops working the day
// somebody adds a statement kind.
//
// NO FALL-THROUGH. A section that reaches its end leaves the switch; it does not slide into the
// next one. This is the C# rule rather than the C one, and it is the whole reason `case 1: case 2:`
// is expressed as two labels on ONE section instead of an empty section falling into the next:
// grouping is what people actually want out of fall-through, and it is the only part of it that
// never produces the accidental-fall-through bug. `break` stays legal and means the same thing --
// leave the switch now -- so existing habits keep working.
//
// A `case` value is parsed as a full expression. Whether it is a CONSTANT, whether it converts to
// the governing type, and whether it duplicates another label are semantic questions, answered in
// the body model where the constant folder and the type table live -- not here, where the answer
// would have to be guessed from syntax.
ast::AstNode* Parser::parseSwitchStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'(' after 'switch'");
    ast::AstNode* conditionDeclaration = nullptr;
    ast::AstNode* governing = nullptr;
    if (looksLikeConditionDeclaration())
    {
        conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            governing = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        governing = parseExpression();
    }
    expect(TokenKind::RightParen, "')' after the switch governing expression");
    expect(TokenKind::LeftBrace, "'{' to open the switch body");

    ast::AstNode* node =
        arena_.make<ast::AstNode>(ast::AstNodeKind::SwitchStatement, tokenRangeFrom(beginIndex));
    node->setFirst(governing);

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!atSwitchLabel())
        {
            // Statements can only live inside a section. Report once, then drop the token so the
            // loop makes progress -- without this the parser spins here on malformed input.
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected 'case' or 'default' at the start of a switch section");
            consume();
            continue;
        }

        const std::size_t sectionBegin = index_;
        ast::AstNode* section =
            arena_.make<ast::AstNode>(ast::AstNodeKind::SwitchSection, current().range());

        std::uint64_t labelCount = 0;
        while (atSwitchLabel())
        {
            const std::size_t labelBegin = index_;
            const bool isDefault = at(TokenKind::KwDefault);
            consume();
            if (!isDefault && at(TokenKind::LeftBracket))
            {
                reportDeferredLanguage3(
                    diagnostics_, current().range(), language::Language3Feature::ListPatterns);
            }
            ast::AstNode* value = isDefault ? nullptr : parseExpression();
            expect(TokenKind::Colon, isDefault ? "':' after 'default'" : "':' after the case value");

            ast::AstNode* label =
                arena_.make<ast::AstNode>(ast::AstNodeKind::SwitchLabel, tokenRangeFrom(labelBegin));
            label->setFlag(isDefault);
            label->setFirst(value);
            section->addChild(label);
            ++labelCount;
        }
        section->setIntegerValue(labelCount);

        while (!atSwitchLabel() && !at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            const std::size_t before = index_;
            ast::AstNode* statement = parseStatement();
            if (statement != nullptr)
            {
                if (statement->kind() == ast::AstNodeKind::BlockStatement &&
                    statement->auxiliaryText() == "conditional-pattern-inline")
                {
                    for (ast::AstNode* child : statement->children()) section->addChild(child);
                }
                else
                {
                    section->addChild(statement);
                }
            }
            if (index_ == before)
            {
                consume();
            }
        }

        section->setRange(tokenRangeFrom(sectionBegin));
        node->addChild(section);
    }

    expect(TokenKind::RightBrace, "'}' to close the switch");
    node->setRange(tokenRangeFrom(beginIndex));
    if (conditionDeclaration != nullptr)
    {
        ast::AstNode* lowered = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, node->range());
        lowered->setAuxiliaryText("condition-declaration");
        lowered->addChild(conditionDeclaration);
        lowered->addChild(node);
        return lowered;
    }
    return node;
}

Parser::Pattern Parser::parsePatternPrimary(bool implicitBinding)
{
    const auto literalStartsHere = [&]() noexcept
    {
        const TokenKind kind = current().kind();
        return kind == TokenKind::IntegerLiteral || kind == TokenKind::FloatingLiteral ||
            kind == TokenKind::CharacterLiteral || kind == TokenKind::StringLiteral ||
            kind == TokenKind::KwTrue || kind == TokenKind::KwFalse || kind == TokenKind::KwNull ||
            (kind == TokenKind::Minus && peek().kind() == TokenKind::IntegerLiteral);
    };
    const auto parseSignedIntegerLiteral = [&]()
        -> std::pair<ast::AstNode*, std::optional<std::int64_t>>
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
    };

    Pattern result;
    const std::size_t patternBegin = index_;
    const SourceRange start = current().range();
    if (at(TokenKind::Identifier) && current().lexeme() == "_")
    {
        consume();
        result.kind = PatternKind::Wildcard;
        result.range = start;
        return result;
    }
    if (consumeIf(TokenKind::Bang))
    {
        result.kind = PatternKind::Not;
        result.children.push_back(parsePattern(true));
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    if (consumeIf(TokenKind::LeftBracket))
    {
        result.kind = PatternKind::List;
        while (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile))
        {
            if (consumeIf(TokenKind::DotDot))
            {
                if (result.restIndex.has_value())
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        tokens_[index_ - 1].range(),
                        "a list pattern accepts at most one rest element");
                }
                else
                {
                    result.restIndex = result.children.size();
                }

                Pattern rest;
                if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket))
                {
                    rest = parsePattern(true);
                }
                else
                {
                    rest.kind = PatternKind::Wildcard;
                    rest.range = tokens_[index_ - 1].range();
                }
                result.children.push_back(std::move(rest));
            }
            else
            {
                result.children.push_back(parsePattern(true));
            }

            if (!consumeIf(TokenKind::Comma)) break;
        }
        expect(TokenKind::RightBracket, "']' after list pattern");
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    if (at(TokenKind::KwVar) || at(TokenKind::KwMove) ||
        atContextualKeyword("copy") || atContextualKeyword("ref"))
    {
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
    if (at(TokenKind::Less) || at(TokenKind::LessEqual) ||
        at(TokenKind::Greater) || at(TokenKind::GreaterEqual))
    {
        result.kind = PatternKind::Relational;
        result.name = std::string(consume().lexeme());
        if (literalStartsHere() || at(TokenKind::Identifier))
        {
            result.firstValue = parsePrimaryExpression();
        }
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
    if (literalStartsHere())
    {
        result.kind = PatternKind::Literal;
        if (at(TokenKind::Minus))
        {
            auto [literal, value] = parseSignedIntegerLiteral();
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
                auto [upper, value] = parseSignedIntegerLiteral();
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
    if (consumeIf(TokenKind::LeftParen))
    {
        if (consumeIf(TokenKind::RightParen))
        {
            result.kind = PatternKind::Tuple;
            result.range = tokenRangeFrom(patternBegin);
            return result;
        }
        Pattern first = parsePattern(true);
        if (!consumeIf(TokenKind::Comma))
        {
            expect(TokenKind::RightParen, "')' after grouped pattern");
            first.range = tokenRangeFrom(patternBegin);
            return first;
        }
        result.kind = PatternKind::Tuple;
        result.children.push_back(std::move(first));
        while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
        {
            result.children.push_back(parsePattern(true));
            if (!consumeIf(TokenKind::Comma)) break;
        }
        expect(TokenKind::RightParen, "')' after tuple pattern");
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    const auto parseStructBody = [&](Pattern& pattern)
    {
        pattern.kind = PatternKind::Struct;
        expect(TokenKind::LeftBrace, "'{' to open struct pattern");
        std::unordered_set<std::string> fields;
        while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "field name in struct pattern");
                break;
            }
            const lexer::Token& field = consume();
            const std::string fieldName(field.lexeme());
            if (!fields.insert(fieldName).second)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    field.range(),
                    "duplicate field '" + fieldName + "' in struct pattern");
            }
            pattern.fieldNames.push_back(fieldName);
            if (consumeIf(TokenKind::Colon))
            {
                pattern.children.push_back(parsePattern(true));
            }
            else
            {
                Pattern shorthand;
                shorthand.kind = PatternKind::Binding;
                shorthand.name = fieldName;
                shorthand.range = field.range();
                pattern.children.push_back(std::move(shorthand));
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
        expect(TokenKind::RightBrace, "'}' after struct pattern");
    };
    if (at(TokenKind::LeftBrace))
    {
        parseStructBody(result);
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    if (at(TokenKind::Identifier))
    {
        const lexer::Token& name = consume();
        result.name = std::string(name.lexeme());
        if (at(TokenKind::LeftBrace))
        {
            parseStructBody(result);
            result.range = tokenRangeFrom(patternBegin);
            return result;
        }
        if (consumeIf(TokenKind::LeftParen))
        {
            // NS304-T03-F04: un payload es un variant tag; dos o mas son deconstruccion posicional.
            std::vector<Pattern> payloads;
            if (!at(TokenKind::RightParen))
            {
                while (true)
                {
                    payloads.push_back(parsePattern(true));
                    if (!consumeIf(TokenKind::Comma)) break;
                }
            }
            expect(TokenKind::RightParen, "')' after variant/positional payload pattern");
            result.kind = payloads.size() <= 1U ? PatternKind::Variant : PatternKind::Positional;
            result.children = std::move(payloads);
        }
        else
        {
            result.kind = implicitBinding ? PatternKind::Binding : PatternKind::Variant;
        }
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected wildcard, literal, binding, tuple, struct, variant, range, or list pattern");
    if (!at(TokenKind::EndOfFile)) consume();
    result.range = start;
    return result;
}

Parser::Pattern Parser::parseAndPattern(bool implicitBinding)
{
    Pattern first = parsePatternPrimary(implicitBinding);
    if (!at(TokenKind::Amp)) return first;
    Pattern result;
    result.kind = PatternKind::And;
    result.range = first.range;
    result.children.push_back(std::move(first));
    while (consumeIf(TokenKind::Amp))
        result.children.push_back(parsePatternPrimary(implicitBinding));
    result.range = SourceRange(
        result.children.front().range.begin(), result.children.back().range.end());
    return result;
}

Parser::Pattern Parser::parsePattern(bool implicitBinding)
{
    Pattern first = parseAndPattern(implicitBinding);
    if (!at(TokenKind::Pipe)) return first;
    Pattern result;
    result.kind = PatternKind::Or;
    result.range = first.range;
    result.children.push_back(std::move(first));
    while (consumeIf(TokenKind::Pipe))
        result.children.push_back(parseAndPattern(implicitBinding));
    result.range = SourceRange(
        result.children.front().range.begin(), result.children.back().range.end());
    return result;
}

bool Parser::patternHasBindings(const Pattern& pattern) const
{
    if (pattern.kind == PatternKind::Binding) return true;
    return std::ranges::any_of(pattern.children, [this](const Pattern& child)
    {
        return patternHasBindings(child);
    });
}

std::vector<Parser::Pattern> Parser::expandPatternAlternatives(const Pattern& pattern) const
{
    if (pattern.kind == PatternKind::Or)
    {
        std::vector<Pattern> alternatives;
        for (const Pattern& child : pattern.children)
        {
            std::vector<Pattern> expanded = expandPatternAlternatives(child);
            alternatives.insert(
                alternatives.end(),
                std::make_move_iterator(expanded.begin()),
                std::make_move_iterator(expanded.end()));
        }
        return alternatives;
    }

    std::vector<Pattern> expanded{pattern};
    expanded.front().children.clear();
    for (const Pattern& child : pattern.children)
    {
        const std::vector<Pattern> childAlternatives = expandPatternAlternatives(child);
        std::vector<Pattern> product;
        product.reserve(expanded.size() * childAlternatives.size());
        for (const Pattern& prefix : expanded)
        {
            for (const Pattern& alternative : childAlternatives)
            {
                Pattern copy = prefix;
                copy.children.push_back(alternative);
                product.push_back(std::move(copy));
            }
        }
        expanded = std::move(product);
    }
    return expanded;
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
    if (pattern.kind == PatternKind::Binding || pattern.kind == PatternKind::Tuple ||
        pattern.kind == PatternKind::List)
        return false;
    return std::ranges::all_of(
        pattern.children,
        [this](const Pattern& child) { return patternIsConditionOnly(child); });
}

ast::AstNode* Parser::makePatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    switch (pattern.kind)
    {
        case PatternKind::Wildcard:
            return makeBooleanLiteral(pattern.range, true);
        case PatternKind::Literal:
            return makeBinaryExpression(TokenKind::EqualEqual, source(), pattern.firstValue);
        case PatternKind::Range:
            return makeBinaryExpression(
                TokenKind::AmpAmp,
                makeBinaryExpression(TokenKind::GreaterEqual, source(), pattern.firstValue),
                makeBinaryExpression(
                    pattern.name == "..=" ? TokenKind::LessEqual : TokenKind::Less,
                    source(),
                    pattern.secondValue));
        case PatternKind::Relational:
        {
            TokenKind op = TokenKind::Less;
            if (pattern.name == "<=") op = TokenKind::LessEqual;
            else if (pattern.name == ">") op = TokenKind::Greater;
            else if (pattern.name == ">=") op = TokenKind::GreaterEqual;
            return makeBinaryExpression(op, source(), pattern.firstValue);
        }
        case PatternKind::Struct:
        {
            ast::AstNode* result = makeBooleanLiteral(pattern.range, true);
            for (std::size_t index = 0; index < pattern.children.size(); ++index)
            {
                const std::string field = pattern.fieldNames[index];
                PatternExpressionFactory member = [&, source, field]
                {
                    return makeMemberExpression(source(), field);
                };
                result = makeBinaryExpression(
                    TokenKind::AmpAmp,
                    result,
                    makePatternCondition(pattern.children[index], member));
            }
            return result;
        }
        case PatternKind::Variant:
        {
            ast::AstNode* test = makeZeroArgCallExpression(
                makeMemberExpression(source(), "Is" + pattern.name));
            if (pattern.children.empty()) return test;
            PatternExpressionFactory payload = [&, source, caseName = pattern.name]
            {
                ast::AstNode* extract = makeZeroArgCallExpression(
                    makeMemberExpression(source(), "__nsc_variant_extract_" + caseName));
                extract->setAuxiliaryText("variant-pattern-extract");
                return extract;
            };
            return makeBinaryExpression(
                TokenKind::AmpAmp,
                test,
                makePatternCondition(pattern.children.front(), payload));
        }
        case PatternKind::Or:
        {
            ast::AstNode* result = makePatternCondition(pattern.children.front(), source);
            for (std::size_t index = 1; index < pattern.children.size(); ++index)
                result = makeBinaryExpression(
                    TokenKind::PipePipe,
                    result,
                    makePatternCondition(pattern.children[index], source));
            return result;
        }
        case PatternKind::Positional:
        {
            // NS304-T03-F04: `source.Deconstruct()` devuelve una tupla; el match siempre aplica
            // (el tipo tiene Deconstruct) y la deconstruccion/binding ocurre en emitPatternBindings.
            return makeBooleanLiteral(pattern.range, true);
        }
        case PatternKind::List:
        {
            const std::size_t restOffset = pattern.restIndex.value_or(pattern.children.size());
            const std::size_t fixedCount = pattern.children.size() -
                (pattern.restIndex.has_value() ? 1U : 0U);
            ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
            lengthMember->setAuxiliaryText("list-pattern-length");
            ast::AstNode* condition = makeBinaryExpression(
                pattern.restIndex.has_value() ? TokenKind::GreaterEqual : TokenKind::EqualEqual,
                lengthMember,
                makeIntegerLiteralExpression(pattern.range, fixedCount));
            for (std::size_t index = 0; index < pattern.children.size(); ++index)
            {
                if (pattern.restIndex == index) continue;
                ast::AstNode* elementIndex = nullptr;
                if (!pattern.restIndex.has_value() || index < restOffset)
                {
                    elementIndex = makeIntegerLiteralExpression(pattern.range, index);
                }
                else
                {
                    const std::size_t fromEnd = pattern.children.size() - index;
                    ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
                    lengthMember->setAuxiliaryText("list-pattern-length");
                    elementIndex = makeBinaryExpression(
                        TokenKind::Minus,
                        lengthMember,
                        makeIntegerLiteralExpression(pattern.range, fromEnd));
                }
                PatternExpressionFactory element = [&, source, elementIndex]
                {
                    return makeIndexExpression(source(), cloneParsedNode(arena_, elementIndex));
                };
                condition = makeBinaryExpression(
                    TokenKind::AmpAmp,
                    condition,
                    makePatternCondition(pattern.children[index], element));
            }
            return condition;
        }
        case PatternKind::Binding:
            return makeBooleanLiteral(pattern.range, true);
        case PatternKind::Tuple:
            return makeBooleanLiteral(pattern.range, false);
        case PatternKind::Not:
        {
            ast::AstNode* negated = makePatternCondition(pattern.children.front(), source);
            ast::AstNode* unary = arena_.make<ast::AstNode>(
                ast::AstNodeKind::UnaryExpression, pattern.range);
            unary->setTokenKind(TokenKind::Bang);
            unary->setFirst(negated);
            return unary;
        }
        case PatternKind::And:
        {
            ast::AstNode* result = makePatternCondition(pattern.children.front(), source);
            for (std::size_t index = 1; index < pattern.children.size(); ++index)
                result = makeBinaryExpression(
                    TokenKind::AmpAmp,
                    result,
                    makePatternCondition(pattern.children[index], source));
            return result;
        }
    }
    return makeBooleanLiteral(pattern.range, false);
}

ast::AstNode* Parser::lowerPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    if (pattern.kind == PatternKind::Wildcard) return continuation;
    if (pattern.kind == PatternKind::Binding)
    {
        if (!emitBindings) return continuation;
        ast::AstNode* block = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        ast::AstNode* declaration = makeVariableDeclaration(
            type,
            pattern.name,
            source(),
            false,
            pattern.bindingMode == PatternBindingMode::Value ||
                pattern.bindingMode == PatternBindingMode::Copy);
        if (pattern.bindingMode == PatternBindingMode::Reference ||
            pattern.bindingMode == PatternBindingMode::ReferenceMutable)
            declaration->setAuxiliaryText("local:ref");
        block->addChild(declaration);
        block->addChild(continuation);
        return block;
    }
    if (pattern.kind == PatternKind::Or && !patternIsConditionOnly(pattern))
    {
        ast::AstNode* block = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        const std::string selectedName = std::string(temporaryPrefix) + "_or_selected_" +
            std::to_string(nextMatchTemporary_++);
        ast::TypeSyntax* boolType = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwBool, "bool");
        block->addChild(makeVariableDeclaration(
            boolType,
            selectedName,
            makeBooleanLiteral(pattern.range, false),
            false,
            false));

        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            ast::AstNode* selectedBody = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, pattern.children[index].range);
            selectedBody->addChild(makeExpressionStatement(makeAssignmentExpression(
                makeIdentifierExpression(pattern.children[index].range, selectedName),
                makeBooleanLiteral(pattern.children[index].range, true))));
            selectedBody->addChild(index + 1U == pattern.children.size()
                ? continuation
                : cloneParsedNode(arena_, continuation));
            ast::AstNode* matcher = lowerPattern(
                pattern.children[index], source, selectedBody, temporaryPrefix, emitBindings);

            ast::AstNode* notSelected = arena_.make<ast::AstNode>(
                ast::AstNodeKind::UnaryExpression, pattern.children[index].range);
            notSelected->setTokenKind(TokenKind::Bang);
            notSelected->setFirst(makeIdentifierExpression(
                pattern.children[index].range, selectedName));
            ast::AstNode* guarded = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IfStatement, pattern.children[index].range);
            guarded->setFirst(notSelected);
            ast::AstNode* guardedBody = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, pattern.children[index].range);
            guardedBody->addChild(matcher);
            guarded->setSecond(guardedBody);
            block->addChild(guarded);
        }
        return block;
    }
    if (pattern.kind == PatternKind::Tuple)
    {
        ast::AstNode* block = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
        deconstruction->setFirst(source());
        std::vector<std::string> elementNames;
        elementNames.reserve(pattern.children.size());
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = std::string(temporaryPrefix) + "_tuple_" +
                std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
            elementNames.push_back(name);
            deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
        }
        block->addChild(deconstruction);
        ast::AstNode* next = continuation;
        for (std::size_t index = pattern.children.size(); index > 0; --index)
        {
            const std::string name = elementNames[index - 1];
            PatternExpressionFactory element = [&, name]
            {
                return makeIdentifierExpression(pattern.range, name);
            };
            next = lowerPattern(
                pattern.children[index - 1], element, next, temporaryPrefix, emitBindings);
        }
        block->addChild(next);
        return block;
    }
    if (pattern.kind == PatternKind::Struct)
    {
        ast::AstNode* next = continuation;
        for (std::size_t index = pattern.children.size(); index > 0; --index)
        {
            const std::string field = pattern.fieldNames[index - 1];
            PatternExpressionFactory member = [&, source, field]
            {
                return makeMemberExpression(source(), field);
            };
            next = lowerPattern(
                pattern.children[index - 1], member, next, temporaryPrefix, emitBindings);
        }
        return next;
    }
    if (pattern.kind == PatternKind::List)
    {
        ast::AstNode* thenBlock = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        if (emitBindings)
            emitPatternBindings(pattern, source, thenBlock, temporaryPrefix);
        thenBlock->addChild(continuation);
        ast::AstNode* decision = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, pattern.range);
        decision->setFirst(makePatternCondition(pattern, source));
        decision->setSecond(thenBlock);
        return decision;
    }
    if (pattern.kind == PatternKind::Positional)
    {
        // NS304-T03-F04: como el patron Tuple/List, la deconstruccion + binding van en el thenBlock.
        ast::AstNode* thenBlock = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        if (emitBindings)
            emitPatternBindings(pattern, source, thenBlock, temporaryPrefix);
        thenBlock->addChild(continuation);
        ast::AstNode* decision = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, pattern.range);
        decision->setFirst(makePatternCondition(pattern, source));
        decision->setSecond(thenBlock);
        return decision;
    }
    if (pattern.kind == PatternKind::Variant)
    {
        ast::AstNode* test = makeZeroArgCallExpression(
            makeMemberExpression(source(), "Is" + pattern.name));
        ast::AstNode* thenBlock = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        if (pattern.children.empty() || pattern.children.front().kind == PatternKind::Wildcard)
        {
            thenBlock->addChild(continuation);
        }
        else
        {
            const std::string payloadName = std::string(temporaryPrefix) + "_payload_" +
                std::to_string(nextMatchTemporary_++);
            ast::AstNode* extract = makeZeroArgCallExpression(
                makeMemberExpression(source(), "__nsc_variant_extract_" + pattern.name));
            extract->setAuxiliaryText("variant-pattern-extract");
            ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
                pattern.range, TokenKind::KwVar, "var");
            thenBlock->addChild(makeVariableDeclaration(type, payloadName, extract, false, true));
            PatternExpressionFactory payload = [&, payloadName]
            {
                return makeIdentifierExpression(pattern.range, payloadName);
            };
            thenBlock->addChild(lowerPattern(
                pattern.children.front(), payload, continuation, temporaryPrefix, emitBindings));
        }
        ast::AstNode* decision = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, pattern.range);
        decision->setFirst(test);
        decision->setSecond(thenBlock);
        return decision;
    }
    ast::AstNode* decision = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(makePatternCondition(pattern, source));
    decision->setSecond(continuation);
    return decision;
}

void Parser::emitPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    if (pattern.kind == PatternKind::Or && patternHasBindings(pattern))
    {
        const std::vector<Pattern> alternatives = expandPatternAlternatives(pattern);
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::function<void(const Pattern&)> collect = [&](const Pattern& current)
        {
            if (current.kind == PatternKind::Binding)
            {
                if (std::ranges::none_of(bindings, [&](const auto& binding)
                    {
                        return binding.first == current.name;
                    }))
                    bindings.emplace_back(current.name, current.bindingMode);
                return;
            }
            for (const Pattern& child : current.children) collect(child);
        };
        if (!alternatives.empty()) collect(alternatives.front());

        for (const auto& [name, mode] : bindings)
        {
            ast::AstNode* selector = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MatchExpression, pattern.range);
            selector->setFirst(source());
            selector->setText(std::string(temporaryPrefix) + "_or_binding_" +
                std::to_string(nextMatchTemporary_++));
            for (const Pattern& alternative : alternatives)
            {
                ast::AstNode* arm = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::MatchArmExpression, alternative.range);
                PatternExpressionFactory matchValue = [&, range = alternative.range]
                {
                    return arena_.make<ast::AstNode>(
                        ast::AstNodeKind::MatchValueExpression, range);
                };
                arm->setFirst(makePatternCondition(alternative, matchValue));
                emitPatternBindings(alternative, matchValue, arm, temporaryPrefix);
                arm->setThird(makeIdentifierExpression(alternative.range, name));
                arm->setFlag(mode == PatternBindingMode::Reference ||
                    mode == PatternBindingMode::ReferenceMutable);
                selector->addChild(arm);
            }

            ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
                pattern.range, TokenKind::KwVar, "var");
            ast::AstNode* declaration = makeVariableDeclaration(
                type,
                name,
                selector,
                false,
                mode == PatternBindingMode::Value || mode == PatternBindingMode::Copy);
            if (mode == PatternBindingMode::Reference ||
                mode == PatternBindingMode::ReferenceMutable)
                declaration->setAuxiliaryText("local:ref");
            destination->addChild(declaration);
        }
        return;
    }
    if (pattern.kind == PatternKind::Binding)
    {
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        ast::AstNode* declaration = makeVariableDeclaration(
            type,
            pattern.name,
            source(),
            false,
            pattern.bindingMode == PatternBindingMode::Value ||
                pattern.bindingMode == PatternBindingMode::Copy);
        if (pattern.bindingMode == PatternBindingMode::Reference ||
            pattern.bindingMode == PatternBindingMode::ReferenceMutable)
            declaration->setAuxiliaryText("local:ref");
        destination->addChild(declaration);
        return;
    }
    if (pattern.kind == PatternKind::Tuple)
    {
        ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
        deconstruction->setFirst(source());
        std::vector<std::string> elementNames;
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = std::string(temporaryPrefix) + "_tuple_" +
                std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
            elementNames.push_back(name);
            deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
        }
        destination->addChild(deconstruction);
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = elementNames[index];
            PatternExpressionFactory element = [&, name]
            {
                return makeIdentifierExpression(pattern.range, name);
            };
            emitPatternBindings(
                pattern.children[index], element, destination, temporaryPrefix);
        }
        return;
    }
    if (pattern.kind == PatternKind::Positional)
    {
        // NS304-T03-F04: deconstruye el valor nominal via Deconstruct() (bindDeconstructionSource
        // resuelve el Deconstruct y devuelve la tupla).
        ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
        deconstruction->setFirst(source());
        std::vector<std::string> elementNames;
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = std::string(temporaryPrefix) + "_pos_" +
                std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
            elementNames.push_back(name);
            deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
        }
        destination->addChild(deconstruction);
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = elementNames[index];
            PatternExpressionFactory element = [&, name]
            {
                return makeIdentifierExpression(pattern.range, name);
            };
            emitPatternBindings(
                pattern.children[index], element, destination, temporaryPrefix);
        }
        return;
    }
    if (pattern.kind == PatternKind::Struct)
    {
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string field = pattern.fieldNames[index];
            PatternExpressionFactory member = [&, source, field]
            {
                return makeMemberExpression(source(), field);
            };
            emitPatternBindings(
                pattern.children[index], member, destination, temporaryPrefix);
        }
        return;
    }
    if (pattern.kind == PatternKind::List)
    {
        const std::size_t restOffset = pattern.restIndex.value_or(pattern.children.size());
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            if (pattern.restIndex == index)
            {
                PatternExpressionFactory rest = [&, source]
                {
                    const std::size_t fixedCount = pattern.children.size() - 1U;
                    ast::AstNode* call = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::CallExpression, pattern.children[index].range);
                    call->setFirst(makeMemberExpression(source(), "Slice"));
                    call->setAuxiliaryText("list-pattern-rest");
                    call->addChild(makeIntegerLiteralExpression(pattern.range, restOffset));
                    ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
                    lengthMember->setAuxiliaryText("list-pattern-length");
                    call->addChild(makeBinaryExpression(
                        TokenKind::Minus,
                        lengthMember,
                        makeIntegerLiteralExpression(pattern.range, fixedCount)));
                    return call;
                };
                emitPatternBindings(
                    pattern.children[index], rest, destination, temporaryPrefix);
                continue;
            }
            PatternExpressionFactory element = [&, source, index, restOffset]
            {
                ast::AstNode* elementIndex = nullptr;
                if (!pattern.restIndex.has_value() || index < restOffset)
                {
                    elementIndex = makeIntegerLiteralExpression(pattern.range, index);
                }
                else
                {
                    const std::size_t fromEnd = pattern.children.size() - index;
                    ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
                    lengthMember->setAuxiliaryText("list-pattern-length");
                    elementIndex = makeBinaryExpression(
                        TokenKind::Minus,
                        lengthMember,
                        makeIntegerLiteralExpression(pattern.range, fromEnd));
                }
                return makeIndexExpression(source(), elementIndex);
            };
            emitPatternBindings(
                pattern.children[index], element, destination, temporaryPrefix);
        }
        return;
    }
    if (pattern.kind == PatternKind::Variant && !pattern.children.empty())
    {
        const std::string payloadName = std::string(temporaryPrefix) + "_payload_" +
            std::to_string(nextMatchTemporary_++);
        ast::AstNode* extract = makeZeroArgCallExpression(
            makeMemberExpression(source(), "__nsc_variant_extract_" + pattern.name));
        extract->setAuxiliaryText("variant-pattern-extract");
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        destination->addChild(makeVariableDeclaration(type, payloadName, extract, false, true));
        PatternExpressionFactory payload = [&, payloadName]
        {
            return makeIdentifierExpression(pattern.range, payloadName);
        };
        emitPatternBindings(
            pattern.children.front(), payload, destination, temporaryPrefix);
    }
}

ast::AstNode* Parser::parseMatchStatement()
{
    struct ParsedArm final
    {
        Pattern pattern;
        SourceRange range;
        ast::AstNode* guard = nullptr;
        ast::AstNode* body = nullptr;
    };

    const std::size_t beginIndex = index_;
    const lexer::Token& matchKeyword = consume();
    expect(TokenKind::LeftParen, "'(' after 'match'");
    ast::AstNode* governing = parseExpression();
    expect(TokenKind::RightParen, "')' after match governing expression");
    expect(TokenKind::LeftBrace, "'{' to open match arms");

    const auto bindingModeName = [](PatternBindingMode mode) -> std::string_view
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
    };

    const auto collectBindings = [&](const Pattern& root)
    {
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::function<void(const Pattern&)> visit = [&](const Pattern& pattern)
        {
            if (pattern.kind == PatternKind::Binding)
            {
                bindings.emplace_back(pattern.name, pattern.bindingMode);
                return;
            }
            for (const Pattern& child : pattern.children) visit(child);
        };
        visit(root);
        std::ranges::sort(bindings, [](const auto& left, const auto& right)
        {
            if (left.first != right.first) return left.first < right.first;
            return left.second < right.second;
        });
        return bindings;
    };
    std::function<bool(const Pattern&)> isCatchAll = [&](const Pattern& pattern)
    {
        if (pattern.kind == PatternKind::Wildcard || pattern.kind == PatternKind::Binding) return true;
        if (pattern.kind != PatternKind::Or) return false;
        return std::ranges::any_of(pattern.children, isCatchAll);
    };
    std::function<void(const Pattern&, bool)> validateBindingModes =
        [&](const Pattern& pattern, bool preservesAddress)
    {
        if (pattern.kind == PatternKind::Binding &&
            pattern.bindingMode != PatternBindingMode::Value && !preservesAddress)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                pattern.range,
                std::string(bindingModeName(pattern.bindingMode)) +
                    " pattern binding cannot cross a tuple deconstruction or variant payload "
                    "copy; bind the aggregate by ref/move first");
            return;
        }
        const bool childPreservesAddress = preservesAddress &&
            pattern.kind != PatternKind::Tuple && pattern.kind != PatternKind::Variant;
        for (const Pattern& child : pattern.children)
            validateBindingModes(child, childPreservesAddress);
    };
    std::function<void(const Pattern&)> validateOrBindings = [&](const Pattern& pattern)
    {
        if (pattern.kind == PatternKind::Or && !pattern.children.empty())
        {
            const auto expected = collectBindings(pattern.children.front());
            for (std::size_t index = 1; index < pattern.children.size(); ++index)
            {
                if (collectBindings(pattern.children[index]) != expected)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        pattern.range,
                        "OR-pattern alternatives must bind identical names with identical "
                        "value/ref/move modes");
                    break;
                }
            }
        }
        for (const Pattern& child : pattern.children) validateOrBindings(child);
    };

    std::vector<ParsedArm> arms;
    bool sawCatchAll = false;
    std::vector<std::pair<std::int64_t, std::int64_t>> coveredIntegerRanges;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t armBegin = index_;
        ParsedArm arm;
        arm.pattern = parsePattern(false);
        arm.range = arm.pattern.range;
        validateBindingModes(arm.pattern, true);
        validateOrBindings(arm.pattern);
        if (atContextualKeyword("when"))
        {
            consume();
            // The arm's `=>` is a delimiter, not the start of a single-parameter lambda.
            // Guards are pure conditions: assignments and lambda literals are intentionally not
            // part of this grammar, so the conditional-expression level is the exact boundary.
            arm.guard = parseConditionalExpression();
        }
        const bool guardAlwaysTrue = arm.guard == nullptr ||
            (arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
             arm.guard->tokenKind() == TokenKind::KwTrue);
        const bool guardAlwaysFalse = arm.guard != nullptr &&
            arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
            arm.guard->tokenKind() == TokenKind::KwFalse;
        if (guardAlwaysFalse)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                arm.guard->range(),
                "match arm is unreachable because its guard is the constant false");
        }
        const bool catchAll = isCatchAll(arm.pattern);
        if (sawCatchAll)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                arm.range,
                "match arm is unreachable after a catch-all wildcard or binding arm");
        }
        if (catchAll && guardAlwaysTrue) sawCatchAll = true;

        std::vector<std::pair<std::int64_t, std::int64_t>> currentRanges;
        std::function<bool(const Pattern&)> collectIntervals = [&](const Pattern& pattern)
        {
            if (pattern.kind == PatternKind::Literal && pattern.firstInteger.has_value() &&
                *pattern.firstInteger != std::numeric_limits<std::int64_t>::max())
            {
                currentRanges.emplace_back(*pattern.firstInteger, *pattern.firstInteger + 1);
                return true;
            }
            if (pattern.kind == PatternKind::Range && pattern.firstInteger.has_value() &&
                pattern.secondInteger.has_value() && *pattern.firstInteger < *pattern.secondInteger)
            {
                currentRanges.emplace_back(*pattern.firstInteger, *pattern.secondInteger);
                return true;
            }
            if (pattern.kind == PatternKind::Or)
            {
                bool complete = true;
                for (const Pattern& child : pattern.children)
                    complete = collectIntervals(child) && complete;
                return complete;
            }
            return false;
        };
        if (collectIntervals(arm.pattern))
        {
            for (std::size_t index = 0; index < currentRanges.size(); ++index)
            {
                const auto interval = currentRanges[index];
                const bool overlapsEarlier = std::ranges::any_of(
                    coveredIntegerRanges,
                    [&](const auto& covered)
                    {
                        return interval.first < covered.second && covered.first < interval.second;
                    });
                bool overlapsSameArm = false;
                for (std::size_t earlier = 0; earlier < index; ++earlier)
                {
                    const auto covered = currentRanges[earlier];
                    if (interval.first < covered.second && covered.first < interval.second)
                    {
                        overlapsSameArm = true;
                        break;
                    }
                }
                if (overlapsEarlier || overlapsSameArm)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        arm.range,
                        "integer literal/range pattern overlaps an earlier alternative");
                    break;
                }
            }
            if (guardAlwaysTrue)
                coveredIntegerRanges.insert(
                    coveredIntegerRanges.end(), currentRanges.begin(), currentRanges.end());
        }
        expect(TokenKind::FatArrow, "'=>' after match pattern");
        if (at(TokenKind::LeftBrace))
        {
            arm.body = parseBlock();
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "a match arm requires a block body");
            arm.body = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, current().range());
            synchronizeStatement();
        }
        arm.range = tokenRangeFrom(armBegin);
        arms.push_back(std::move(arm));
        consumeIf(TokenKind::Comma);
    }
    expect(TokenKind::RightBrace, "'}' to close match arms");

    if (arms.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            matchKeyword.range(),
            "a match statement requires at least one arm");
    }
    // Early lowering deliberately uses only ordinary statement/expression nodes. This makes the
    // evaluation and allocation contract visible to all existing dumps: the governing expression
    // appears once, and every branch reads the hidden local.
    const SourceRange matchRange = tokenRangeFrom(beginIndex);
    ast::AstNode* lowered = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, matchRange);
    ast::TypeSyntax* inferred = arena_.make<ast::TypeSyntax>(
        matchKeyword.range(), TokenKind::KwVar, "var");
    const std::string temporary =
        "$__nsc_match_" + std::to_string(nextMatchTemporary_++);
    // Keep the source-level arm coverage attached to the lowered block. Semantic analysis knows
    // the governing type (including variants reconstructed from NSMETA), while the parser knows
    // the pattern shape. This compact bridge lets the canonical closed-domain algorithm combine
    // both facts without introducing a second match AST/backend representation.
    std::string coverage = "match-coverage:v1:";
    std::function<void(const Pattern&, std::vector<std::string>&, bool&, bool&,
        std::optional<std::size_t>&, bool&)> coveredCases =
        [&](const Pattern& pattern, std::vector<std::string>& cases, bool& wildcard, bool& partial,
            std::optional<std::size_t>& listLength, bool& listRest)
    {
        if (pattern.kind == PatternKind::Wildcard || pattern.kind == PatternKind::Binding)
        {
            wildcard = true;
            return;
        }
        if (pattern.kind == PatternKind::Variant)
        {
            cases.push_back(pattern.name);
            if (!pattern.children.empty() && !isCatchAll(pattern.children.front())) partial = true;
            return;
        }
        if (pattern.kind == PatternKind::List)
        {
            const std::size_t fixedCount = pattern.children.size() -
                (pattern.restIndex.has_value() ? 1U : 0U);
            listLength = fixedCount;
            listRest = pattern.restIndex.has_value();
            for (std::size_t index = 0; index < pattern.children.size(); ++index)
            {
                if (pattern.restIndex.has_value() && *pattern.restIndex == index) continue;
                if (!isCatchAll(pattern.children[index])) { partial = true; break; }
            }
            return;
        }
        if (pattern.kind == PatternKind::Or)
            for (const Pattern& child : pattern.children)
                coveredCases(child, cases, wildcard, partial, listLength, listRest);
    };
    for (std::size_t armIndex = 0; armIndex < arms.size(); ++armIndex)
    {
        if (armIndex != 0) coverage += ';';
        std::vector<std::string> cases;
        bool wildcard = false;
        bool partial = false;
        std::optional<std::size_t> listLength;
        bool listRest = false;
        coveredCases(arms[armIndex].pattern, cases, wildcard, partial, listLength, listRest);
        const bool guardRestrictsCoverage = arms[armIndex].guard != nullptr &&
            !(arms[armIndex].guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
              arms[armIndex].guard->tokenKind() == TokenKind::KwTrue);
        if (listLength.has_value())
        {
            if (partial || guardRestrictsCoverage) coverage += '~';
            coverage += '#' + std::to_string(*listLength);
            if (listRest) coverage += '+';
            continue;
        }
        if (wildcard)
        {
            if (guardRestrictsCoverage) coverage += '~';
            coverage += '*';
            continue;
        }
        std::ranges::sort(cases);
        cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
        if (cases.empty())
        {
            coverage += '?';
            continue;
        }
        if (partial || guardRestrictsCoverage)
            coverage += '~';
        for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
        {
            if (caseIndex != 0) coverage += '|';
            coverage += cases[caseIndex];
        }
    }
    lowered->setText(temporary);
    lowered->setAuxiliaryText(std::move(coverage));
    const bool hasReferenceBinding = std::ranges::any_of(
        arms,
        [&](const ParsedArm& arm)
        {
            const auto bindings = collectBindings(arm.pattern);
            return std::ranges::any_of(bindings, [](const auto& binding)
            {
                return binding.second == PatternBindingMode::Reference ||
                binding.second == PatternBindingMode::ReferenceMutable;
            });
        });
    const bool hasMoveBinding = std::ranges::any_of(
        arms,
        [&](const ParsedArm& arm)
        {
            const auto bindings = collectBindings(arm.pattern);
            return std::ranges::any_of(bindings, [](const auto& binding)
            {
                return binding.second == PatternBindingMode::Move;
            });
        });
    ast::AstNode* governingDeclaration = makeVariableDeclaration(
        inferred, temporary, governing, false, !hasReferenceBinding && !hasMoveBinding);
    if (hasReferenceBinding) governingDeclaration->setAuxiliaryText("local:ref");
    lowered->addChild(governingDeclaration);

    const std::string matched = temporary + "_selected";
    ast::TypeSyntax* boolType = arena_.make<ast::TypeSyntax>(
        matchKeyword.range(), TokenKind::KwBool, "bool");
    lowered->addChild(makeVariableDeclaration(
        boolType, matched, makeBooleanLiteral(matchKeyword.range(), false), false, false));

    const auto makeNotMatched = [&]() -> ast::AstNode*
    {
        ast::AstNode* result = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, matchRange);
        result->setTokenKind(TokenKind::Bang);
        result->setFirst(makeIdentifierExpression(matchRange, matched));
        return result;
    };
    for (const ParsedArm& arm : arms)
    {
        ast::AstNode* selectedBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, arm.range);
        selectedBody->addChild(makeExpressionStatement(makeAssignmentExpression(
            makeIdentifierExpression(arm.range, matched), makeBooleanLiteral(arm.range, true))));
        for (ast::AstNode* statement : arm.body->children()) selectedBody->addChild(statement);

        PatternExpressionFactory root = [&, armRange = arm.range]
        {
            return makeIdentifierExpression(armRange, temporary);
        };
        ast::AstNode* continuation = selectedBody;
        if (arm.guard != nullptr)
        {
            ast::AstNode* guarded = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IfStatement, arm.guard->range());
            guarded->setFirst(arm.guard);
            guarded->setSecond(selectedBody);
            ast::AstNode* guardedBlock = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, arm.guard->range());
            guardedBlock->addChild(guarded);
            continuation = guardedBlock;
        }
        ast::AstNode* matcher = lowerPattern(
            arm.pattern, root, continuation, temporary);
        ast::AstNode* guardedBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, arm.range);
        guardedBody->addChild(matcher);
        ast::AstNode* guard = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, arm.range);
        guard->setFirst(makeNotMatched());
        guard->setSecond(guardedBody);
        lowered->addChild(guard);
    }
    return lowered;
}

ast::AstNode* Parser::parseMatchExpression()
{
    const std::size_t beginIndex = index_;
    const lexer::Token& matchKeyword = consume();
    expect(TokenKind::LeftParen, "'(' after 'match'");
    ast::AstNode* governing = parseExpression();
    expect(TokenKind::RightParen, "')' after match governing expression");
    expect(TokenKind::LeftBrace, "'{' to open match expression arms");

    ast::AstNode* match = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MatchExpression, matchKeyword.range());
    match->setFirst(governing);
    const std::string temporary = "$__nsc_match_expr_" +
        std::to_string(nextMatchTemporary_++);
    match->setText(temporary);

    const auto isCatchAll = [&](const Pattern& root)
    {
        std::function<bool(const Pattern&)> visit = [&](const Pattern& pattern)
        {
            if (pattern.kind == PatternKind::Wildcard || pattern.kind == PatternKind::Binding)
                return true;
            return pattern.kind == PatternKind::Or &&
                std::ranges::any_of(pattern.children, visit);
        };
        return visit(root);
    };
    const auto collectBindings = [&](const Pattern& root)
    {
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::function<void(const Pattern&)> visit = [&](const Pattern& pattern)
        {
            if (pattern.kind == PatternKind::Binding)
                bindings.emplace_back(pattern.name, pattern.bindingMode);
            for (const Pattern& child : pattern.children) visit(child);
        };
        visit(root);
        std::ranges::sort(bindings);
        return bindings;
    };
    const auto validateOrBindings = [&](const Pattern& root)
    {
        std::function<void(const Pattern&)> visit = [&](const Pattern& pattern)
        {
            if (pattern.kind == PatternKind::Or && !pattern.children.empty())
            {
                const auto expected = collectBindings(pattern.children.front());
                for (std::size_t index = 1; index < pattern.children.size(); ++index)
                {
                    if (collectBindings(pattern.children[index]) != expected)
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            pattern.range,
                            "OR-pattern alternatives in a match expression must bind identical "
                            "names with identical value/ref/move modes");
                        break;
                    }
                }
            }
            for (const Pattern& child : pattern.children) visit(child);
        };
        visit(root);
    };

    std::string coverage = "match-coverage:v1:";
    bool firstArm = true;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t armBegin = index_;
        Pattern pattern = parsePattern(false);
        validateOrBindings(pattern);
        ast::AstNode* guard = nullptr;
        if (atContextualKeyword("when"))
        {
            consume();
            guard = parseConditionalExpression();
        }
        expect(TokenKind::FatArrow, "'=>' after match expression pattern");
        const bool referenceResult = atContextualKeyword("ref");
        if (referenceResult) consume();
        ast::AstNode* value = parseExpression();

        const std::vector<Pattern> alternatives = patternHasBindings(pattern)
            ? expandPatternAlternatives(pattern)
            : std::vector<Pattern>{pattern};
        for (std::size_t alternativeIndex = 0;
             alternativeIndex < alternatives.size();
             ++alternativeIndex)
        {
            const Pattern& alternative = alternatives[alternativeIndex];
            ast::AstNode* arm = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MatchArmExpression, tokenRangeFrom(armBegin));
            PatternExpressionFactory source = [&, range = alternative.range]
            {
                return arena_.make<ast::AstNode>(ast::AstNodeKind::MatchValueExpression, range);
            };
            arm->setFirst(makePatternCondition(alternative, source));
            arm->setSecond(alternativeIndex == 0U ? guard : cloneParsedNode(arena_, guard));
            arm->setThird(alternativeIndex == 0U ? value : cloneParsedNode(arena_, value));
            arm->setFlag(referenceResult);
            emitPatternBindings(alternative, source, arm, temporary);
            match->addChild(arm);

            if (!firstArm) coverage += ';';
            firstArm = false;
            std::vector<std::string> cases;
            bool wildcard = false;
            bool partial = false;
            std::optional<std::size_t> listLength;
            bool listRest = false;
            std::function<void(const Pattern&)> collectCoverage = [&](const Pattern& current)
            {
                if (current.kind == PatternKind::Wildcard ||
                    current.kind == PatternKind::Binding)
                {
                    wildcard = true;
                    return;
                }
                if (current.kind == PatternKind::Variant)
                {
                    cases.push_back(current.name);
                    if (!current.children.empty() && !isCatchAll(current.children.front()))
                        partial = true;
                    return;
                }
                if (current.kind == PatternKind::List)
                {
                    const std::size_t fixedCount = current.children.size() -
                        (current.restIndex.has_value() ? 1U : 0U);
                    listLength = fixedCount;
                    listRest = current.restIndex.has_value();
                    for (std::size_t index = 0; index < current.children.size(); ++index)
                    {
                        if (current.restIndex.has_value() && *current.restIndex == index) continue;
                        if (!isCatchAll(current.children[index])) { partial = true; break; }
                    }
                    return;
                }
                if (current.kind == PatternKind::Or)
                    for (const Pattern& child : current.children) collectCoverage(child);
            };
            collectCoverage(alternative);
            const bool guardRestrictsCoverage = guard != nullptr &&
                !(guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                  guard->tokenKind() == TokenKind::KwTrue);
            if (listLength.has_value())
            {
                if (partial || guardRestrictsCoverage) coverage += '~';
                coverage += '#' + std::to_string(*listLength);
                if (listRest) coverage += '+';
            }
            else if (wildcard)
            {
                if (guardRestrictsCoverage) coverage += '~';
                coverage += '*';
            }
            else if (cases.empty()) coverage += '?';
            else
            {
                std::ranges::sort(cases);
                cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
                if (partial || guardRestrictsCoverage) coverage += '~';
                for (std::size_t index = 0; index < cases.size(); ++index)
                {
                    if (index != 0) coverage += '|';
                    coverage += cases[index];
                }
            }
        }
        if (!consumeIf(TokenKind::Comma) && !at(TokenKind::RightBrace))
            expect(TokenKind::Comma, "',' between match expression arms");
    }
    expect(TokenKind::RightBrace, "'}' to close match expression arms");
    if (match->children().empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            matchKeyword.range(),
            "a match expression requires at least one arm");
    }
    match->setAuxiliaryText(std::move(coverage));
    match->setRange(tokenRangeFrom(beginIndex));
    return match;
}

ast::AstNode* Parser::parseLoopExpression()
{
    const std::size_t beginIndex = index_;
    consume();

    std::string label;
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::LeftBrace)
        label = std::string(consume().lexeme());

    if (!at(TokenKind::LeftBrace))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected '{' after loop expression" +
                (label.empty() ? std::string{} : " label '" + label + "'"));
    }
    ast::AstNode* body = parseBlock();
    ast::AstNode* loop = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LoopExpression, tokenRangeFrom(beginIndex));
    loop->setFirst(body);
    loop->setText(std::move(label));
    return loop;
}

ast::AstNode* Parser::parseComprehensionYieldStatement()
{
    const std::size_t beginIndex = index_;
    const SourceRange yieldRange = consume().range();
    const ComprehensionContext& context = comprehensionContexts_.back();

    if (consumeIf(TokenKind::KwBreak))
    {
        expect(TokenKind::Semicolon, "';' after 'yield break'");
        ast::AstNode* stop = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BreakStatement, tokenRangeFrom(beginIndex));
        stop->setText(context.loopLabel);
        ast::AstNode* result = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, yieldRange);
        result->setFirst(makeIdentifierExpression(yieldRange, context.builderName));
        result->setAuxiliaryText("comprehension-result");
        stop->setFirst(result);
        return stop;
    }

    consumeIf(TokenKind::KwReturn);
    ast::AstNode* value = parseExpression();
    expect(TokenKind::Semicolon, "';' after comprehension yield");
    ast::AstNode* add = makeZeroArgCallExpression(makeMemberExpression(
        makeIdentifierExpression(yieldRange, context.builderName), "Add"));
    if (value != nullptr) add->addChild(value);
    add->setAuxiliaryText("comprehension-builder-add");
    return makeExpressionStatement(add);
}

ast::AstNode* Parser::parseComprehensionExpression()
{
    const std::size_t beginIndex = index_;
    const SourceRange keywordRange = consume().range();
    expect(TokenKind::LeftParen, "'(' after comprehend");
    ast::AstNode* builder = parseExpression();
    expect(TokenKind::RightParen, "')' after comprehension builder");

    const std::size_t ordinal = nextComprehensionTemporary_++;
    ComprehensionContext context{
        "$__comprehension_builder_" + std::to_string(ordinal),
        "$__comprehension_loop_" + std::to_string(ordinal)};
    comprehensionContexts_.push_back(context);
    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected '{' after comprehension builder");
        body = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, keywordRange);
    }
    comprehensionContexts_.pop_back();

    const SourceRange range = tokenRangeFrom(beginIndex);
    ast::AstNode* loopBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    ast::TypeSyntax* inferred = arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVar, std::string("var"));
    ast::AstNode* builderDeclaration = makeVariableDeclaration(
        inferred, context.builderName, builder, false);
    builderDeclaration->setAuxiliaryText("local:comprehension-builder");
    loopBody->addChild(builderDeclaration);
    loopBody->addChild(body);

    ast::AstNode* finish = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, range);
    finish->setText(context.loopLabel);
    ast::AstNode* result = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, range);
    result->setFirst(makeIdentifierExpression(range, context.builderName));
    result->setAuxiliaryText("comprehension-result");
    finish->setFirst(result);
    loopBody->addChild(finish);

    ast::AstNode* loop = arena_.make<ast::AstNode>(ast::AstNodeKind::LoopExpression, range);
    loop->setText(context.loopLabel);
    loop->setAuxiliaryText("comprehension:explicit-builder");
    loop->setFirst(loopBody);
    return loop;
}

ast::AstNode* Parser::parseForStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");

    const auto parseExpressionList = [&]() -> ast::AstNode*
    {
        ast::AstNode* first = parseExpression();
        if (!at(TokenKind::Comma)) return first;

        ast::AstNode* sequence = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            first != nullptr ? first->range() : current().range());
        if (first != nullptr) sequence->addChild(makeExpressionStatement(first));
        while (consumeIf(TokenKind::Comma))
        {
            ast::AstNode* expression = parseExpression();
            if (expression != nullptr) sequence->addChild(makeExpressionStatement(expression));
        }
        sequence->setRange(tokenRangeFrom(beginIndex));
        return sequence;
    };

    ast::AstNode* initializer = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        if (at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVolatile) ||
            at(TokenKind::KwVar) || canStartTypeAt(index_))
        {
            initializer = parseVariableDeclaration(false);
        }
        else
        {
            initializer = parseExpressionList();
        }
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* condition = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        condition = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* increment = nullptr;
    if (!at(TokenKind::RightParen))
    {
        increment = parseExpressionList();
    }
    expect(TokenKind::RightParen, "')'");

    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after for clause");
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, tokenRangeFrom(beginIndex));
    node->setFirst(initializer);
    node->setSecond(condition);
    node->setThird(increment);
    node->addChild(body);
    return node;
}

// ---------------------------------------------------------------------------------------------
// foreach -- Stage 52.x, requested by Alberto separately from the Stage 52 closure round.
//
// DESIGN (documented in detail in 52_x_nativesharp_foreach.md; summarized here for anyone
// reading the parser code directly):
//
// NativeSharp deliberately does not require an `IEnumerable<T>`/`IEnumerator<T>` protocol for
// iteration. Generic interfaces are supported, but a universal enumeration contract would also
// need a universal object/value story that this native language does not promise. So `foreach`
// here is implemented exactly the
// way the real C# LANGUAGE SPECIFICATION actually defines it (not the common misconception):
// a compile-time, structural ("duck-typed") pattern -- an accessible zero-argument
// `GetEnumerator()` method returning something with `bool MoveNext()` and a zero-argument
// `Current()` accessor -- resolved entirely through ORDINARY member/overload lookup, the exact
// same lookup that resolves any other `.Method()` call in this compiler. No interfaces, no
// runtime dispatch, no reflection: everything is decided once, at compile time, per call site
// (consistent with `restricciones_arquitectonicas.md`'s "no reflection, no dynamic dispatch by
// name" rule) -- and it composes for free with the array case, which uses indexing/`.Length`
// instead of a method pattern.
//
// IMPLEMENTATION STRATEGY -- desugaring to already-existing, already-tested constructs:
//
// Rather than inventing new HIR/MIR/LLVM node kinds for foreach's control flow (the highest-risk
// part of any loop feature -- see WINE_EXECUTION_FINDINGS_STAGE52.md for how much subtlety
// already lives in this compiler's loop/RAII/cleanup-region lowering), `parseForeachStatement`
// desugars `foreach (T x in expr) body` at PARSE TIME into an ordinary nested-block AST built
// from the exact same node kinds (VariableDeclarationStatement, WhileStatement, MemberExpression,
// CallExpression, IndexExpression, BinaryExpression, AssignmentExpression) any hand-written
// NativeSharp program already produces -- using the SAME arena_.make<...>() calls the rest of
// this parser uses, not a separate synthesis mechanism. This means every later pipeline stage
// (ProjectMethodBodyModel's ownership/RAII-across-back-edge analysis, ProjectOopHirModel's HIR
// lowering, ProjectOopMirModel, ProjectOopLlvmLowerer) needs ZERO new code to handle a foreach
// loop's actual control flow -- it looks, to every one of those passes, exactly like a
// hand-written `while` loop, because it IS one, just synthesized instead of typed by hand.
//
// The one piece parsing genuinely cannot decide is array-mode vs. enumerator-pattern-mode --
// that requires the collection expression's resolved TYPE, which does not exist until semantic
// analysis (ProjectMethodBodyModel) runs. So the parser builds BOTH candidate desugarings
// unconditionally (structurally -- no type information needed to construct an AST node, only to
// analyze one) and stores them as two children of a new, thin `ForeachStatement` AST node:
//   first()  = "var __foreach_source = <original collection expression>;" -- evaluates the
//              collection exactly once (matching real C# foreach semantics), regardless of mode.
//   second() = the array-mode candidate block (index-based loop over __foreach_source[i]).
//   third()  = the enumerator-pattern candidate block (GetEnumerator/MoveNext/Current loop).
// `ProjectMethodBodyModel::analyzeForeach` (semantic analysis) is the ONLY place that ever
// decides which candidate is real: it resolves __foreach_source's type, and if it is an array
// type analyzes ONLY second(), otherwise attempts third() -- the discarded candidate is never
// visited by any analysis pass, so it can never contribute a spurious diagnostic. Both hidden
// locals (`__foreach_source`, and the array/pattern candidate's own hidden index-or-enumerator
// local) are declared with `var`, reusing this session's own `var` type-inference feature, so
// their concrete types (`builtin:int` for the index; whatever `GetEnumerator()` returns for the
// enumerator) never need to be known or spelled out at parse time either.
//
// The enumerator candidate marks its hidden local for type-directed lifetime handling. Semantic
// analysis has the resolved GetEnumerator() return type: reference enumerators become RAII owners
// exactly as before, while trivial struct enumerators remain values in the current stack frame.
// The parser cannot make that choice because it deliberately has no type information.
ast::AstNode* Parser::makeIdentifierExpression(const SourceRange& range, std::string name)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, range);
    node->setText(std::move(name));
    return node;
}

ast::AstNode* Parser::makeIntegerLiteralExpression(const SourceRange& range, std::uint64_t value)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, range);
    node->setTokenKind(TokenKind::IntegerLiteral);
    node->setText(std::to_string(value));
    node->setIntegerValue(value);
    return node;
}

ast::AstNode* Parser::makeMemberExpression(ast::AstNode* receiver, std::string memberName)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::MemberExpression, receiver->range());
    node->setFirst(receiver);
    node->setText(std::move(memberName));
    return node;
}

ast::AstNode* Parser::makeZeroArgCallExpression(ast::AstNode* callee)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::CallExpression, callee->range());
    node->setFirst(callee);
    return node;
}

ast::AstNode* Parser::makeIndexExpression(ast::AstNode* receiver, ast::AstNode* index)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IndexExpression, receiver->range());
    node->setFirst(receiver);
    node->setSecond(index);
    node->addChild(index);
    return node;
}

ast::AstNode* Parser::makeBinaryExpression(TokenKind operatorKind, ast::AstNode* left, ast::AstNode* right)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BinaryExpression, join(left->range(), right->range()));
    node->setTokenKind(operatorKind);
    node->setFirst(left);
    node->setSecond(right);
    return node;
}

ast::AstNode* Parser::makeAssignmentExpression(ast::AstNode* target, ast::AstNode* value)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression, join(target->range(), value->range()));
    node->setTokenKind(TokenKind::Equal);
    node->setFirst(target);
    node->setSecond(value);
    return node;
}

ast::AstNode* Parser::makeExpressionStatement(ast::AstNode* expression)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ExpressionStatement, expression->range());
    node->setFirst(expression);
    return node;
}

ast::AstNode* Parser::makeVariableDeclaration(
    ast::TypeSyntax* type, std::string name, ast::AstNode* initializer, bool isRaii, bool isConst)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement, initializer->range());
    node->setTypeSyntax(type);
    node->setText(std::move(name));
    if (isRaii) node->addModifier(TokenKind::KwRaii);
    // `flag()` is the same bit an ordinary `const T x = ...;` local declaration sets via
    // `variable->setFlag(isConst)` (see parseVariableDeclarationStatement) and
    // ProjectMethodBodyModel::analyzeVariable reads back as `local.isConst` -- reusing the exact
    // same mechanism ordinary locals already use for immutability, rather than inventing a
    // foreach-specific "readonly" concept (P0.3 of the closure review).
    node->setFlag(isConst);
    node->setFirst(initializer);
    return node;
}

ast::AstNode* Parser::parseForeachStatement()
{
    const std::size_t beginIndex = index_;
    // Reserve the identity before parsing the body so a nested foreach receives a different
    // namespace without changing the names already chosen for its enclosing loop.
    const std::string temporaryPrefix =
        "$__foreach_" + std::to_string(nextMatchTemporary_++);
    const std::string sourceName = temporaryPrefix + "_source";
    const std::string indexName = temporaryPrefix + "_index";
    const std::string enumeratorName = temporaryPrefix + "_enumerator";
    const std::string rangeBeginName = temporaryPrefix + "_range_begin";
    const std::string rangeEndName = temporaryPrefix + "_range_end";
    const std::string rangeStepName = temporaryPrefix + "_range_step";
    const std::string rangeCursorName = temporaryPrefix + "_range_cursor";
    const std::string elementTemporaryName = temporaryPrefix + "_element";
    const std::string patternTemporaryPrefix = temporaryPrefix + "_pattern";
    consume(); // foreach
    expect(TokenKind::LeftParen, "'('");

    // NativeSharp 3.0 T09-F01. The iteration binding mode is part of the foreach clause rather
    // than inferred from Current/indexer shape:
    //
    //   foreach (T value in source)          // value snapshot
    //   foreach (ref T value in source)      // readonly alias
    //   foreach (ref mut T value in source)  // exclusive mutable alias
    //
    // `ref`/`mut` stay contextual. The synthesized declaration below then
    // travels through the ordinary reference-local pipeline, so addressability, exact type,
    // lifetime, exclusivity and LLVM pointer lowering are shared with source ref locals.
    const bool elementByReference = atContextualKeyword("ref");
    bool elementMutableReference = false;
    if (elementByReference)
    {
        consume();
        if (atContextualKeyword("mut"))
        {
            elementMutableReference = true;
            consume();
        }
    }

    // Element declaration: `raii` (optional, same grammar as an ordinary local), then a real
    // type or `var`, then the element name -- mirrors parseVariableDeclaration's own prefix
    // handling exactly, since this element genuinely IS an ordinary local once desugared.
    //
    // `const` is deliberately no longer accepted here (closure-review fix, P0.4 of
    // STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md: a keyword that parsed successfully and
    // was then silently discarded -- `const bool elementIsConst = consumeIf(...); (void)
    // elementIsConst;` -- is worse than not accepting it at all, since it silently promises a
    // guarantee it never delivers). Real C# does not accept `const` on a foreach iteration
    // variable either -- the variable is simply ALWAYS non-reassignable, unconditionally, which
    // is exactly what NativeSharp now does too (see the element declarations below: both
    // candidates pass `elementIsConst = true` unconditionally to makeVariableDeclaration, P0.3).
    // `foreach (const var x in ...)` is therefore now a plain syntax error ("expected type or
    // 'var'", since `const` is consumed by nothing here and parseType()/the `var` check both
    // reject it) rather than a silently-ignored no-op -- an honest failure instead of a false
    // promise.
    const bool elementIsRaii = consumeIf(TokenKind::KwRaii);
    if (elementByReference && elementIsRaii)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a foreach reference binding aliases source storage and cannot also be raii");
    }
    ast::TypeSyntax* elementType;
    if (at(TokenKind::KwVar))
    {
        const lexer::Token& varToken = consume();
        elementType = arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        elementType = parseType();
    }

    const bool elementDeconstruction = at(TokenKind::LeftParen);
    Pattern elementPattern;
    std::string elementName;
    if (elementDeconstruction)
    {
        elementPattern = parsePattern(true);
        elementName = elementTemporaryName;
    }
    else if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "foreach element name");
        synchronizeStatement();
        return nullptr;
    }
    else
    {
        elementName = std::string(consume().lexeme());
    }

    expect(TokenKind::KwIn, "'in'");
    // P0.2 of the closure review: the collection expression is parsed TWICE from the same token
    // range -- once here (`collection`, embedded in the REAL, semantically-analyzed
    // `sourceDecl` below) and once more, immediately after resetting `index_` back to this
    // point, into a completely separate, structurally-identical-but-distinct AST subtree
    // (`collectionProbe`). `collectionProbe` is never lowered or executed -- it exists only so
    // `ProjectMethodBodyModel::analyzeForeach` can run a throwaway `analyzeExpression` probe on
    // it (with a scratch, discarded DiagnosticEngine) to learn the collection's ownership shape
    // BEFORE deciding whether `sourceDecl` needs to be `raii`. Probing the SAME node twice would
    // double-register that node's bindings in the semantic model (name lookups, call bindings,
    // ...); probing a genuinely separate, unreferenced copy instead has zero effect on the real
    // analysis path. This is the same "parse-time desugaring, decide later once types are known"
    // philosophy the array-vs-enumerator candidate split already uses, applied one layer earlier.
    const std::size_t collectionStart = index_;
    ast::AstNode* collection = parseExpression();
    const std::size_t collectionEnd = index_;
    index_ = collectionStart;
    ast::AstNode* collectionProbe = parseExpression();
    index_ = collectionEnd;
    expect(TokenKind::RightParen, "')'");

    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after foreach clause");
    }

    const SourceRange range = tokenRangeFrom(beginIndex);
    auto varMarker = [&]() -> ast::TypeSyntax*
    {
        return arena_.make<ast::TypeSyntax>(range, TokenKind::KwVar, std::string("var"));
    };
    const auto configureElementBinding = [&](ast::AstNode* declaration)
    {
        if (!elementByReference || declaration == nullptr) return;
        declaration->setAuxiliaryText(
            elementMutableReference ? "local:foreach-ref-mut" : "local:foreach-ref");
    };
    const auto emitElementBindings = [&](ast::AstNode* destination)
    {
        if (!elementDeconstruction || destination == nullptr) return;
        PatternExpressionFactory source = [&]()
        {
            return makeIdentifierExpression(range, elementName);
        };
        emitPatternBindings(elementPattern, source, destination, patternTemporaryPrefix);
    };

    // A bounded integer RangeExpression is a storage-free foreach source. Lower it immediately
    // to ordinary scalar locals plus a ForStatement so HIR/MIR/LLVM reuse the verified loop,
    // arithmetic, break and continue paths. Begin, end, and step are bound exactly once from left
    // to right. No array, list, or enumerator is materialized.
    if (collection != nullptr && collection->kind() == ast::AstNodeKind::RangeExpression)
    {
        if (elementByReference)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                collection->range(),
                "foreach reference iteration requires stable source storage; an integer range "
                "produces values");
        }
        ast::AstNode* begin = collection->first();
        ast::AstNode* end = collection->second();
        ast::AstNode* step = collection->third();
        if (begin == nullptr)
            begin = makeIntegerLiteralExpression(range, 0);
        if (end == nullptr)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidIndex,
                diagnostics::DiagnosticSeverity::Error,
                collection->range(),
                "foreach integer range requires a finite end endpoint");
            end = makeIntegerLiteralExpression(range, 0);
        }
        if (begin->kind() == ast::AstNodeKind::FromEndExpression ||
            end->kind() == ast::AstNodeKind::FromEndExpression)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidIndex,
                diagnostics::DiagnosticSeverity::Error,
                collection->range(),
                "a from-end endpoint requires a sized receiver and cannot bound a foreach integer range");
        }
        if (step == nullptr) step = makeIntegerLiteralExpression(range, 1);

        ast::AstNode* beginDecl = makeVariableDeclaration(
            varMarker(), rangeBeginName, begin, false, true);
        ast::AstNode* endDecl = makeVariableDeclaration(
            varMarker(), rangeEndName, end, false, true);
        ast::AstNode* stepDecl = makeVariableDeclaration(
            varMarker(), rangeStepName, step, false, true);
        ast::AstNode* cursorDecl = makeVariableDeclaration(
            varMarker(), rangeCursorName,
            makeIdentifierExpression(range, rangeBeginName), false);

        const auto id = [&](std::string name)
        {
            return makeIdentifierExpression(range, std::move(name));
        };
        ast::AstNode* ascending = makeBinaryExpression(
            TokenKind::AmpAmp,
            makeBinaryExpression(TokenKind::Greater, id(rangeStepName),
                makeIntegerLiteralExpression(range, 0)),
            makeBinaryExpression(collection->flag() ? TokenKind::LessEqual : TokenKind::Less,
                id(rangeCursorName), id(rangeEndName)));
        ast::AstNode* descending = makeBinaryExpression(
            TokenKind::AmpAmp,
            makeBinaryExpression(TokenKind::Less, id(rangeStepName),
                makeIntegerLiteralExpression(range, 0)),
            makeBinaryExpression(collection->flag() ? TokenKind::GreaterEqual : TokenKind::Greater,
                id(rangeCursorName), id(rangeEndName)));
        ast::AstNode* condition = makeBinaryExpression(TokenKind::PipePipe, ascending, descending);
        ast::AstNode* increment = makeAssignmentExpression(
            id(rangeCursorName),
            makeBinaryExpression(
                TokenKind::Plus,
                id(rangeCursorName),
                id(rangeStepName)));
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeIdentifierExpression(range, rangeCursorName),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureElementBinding(elementDecl);
        ast::AstNode* forBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        emitElementBindings(forBody);
        forBody->addChild(body);
        ast::AstNode* loop = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, range);
        loop->setFirst(cursorDecl);
        loop->setSecond(condition);
        loop->setThird(increment);
        loop->addChild(forBody);
        ast::AstNode* lowered = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        lowered->setAuxiliaryText(collection->flag()
            ? "foreach:integer-range-inclusive" : "foreach:integer-range-exclusive");
        lowered->addChild(beginDecl);
        lowered->addChild(endDecl);
        lowered->addChild(stepDecl);

        // Dynamic steps retain the same contract as compile-time steps. Constant zero and
        // wrong-direction ranges were diagnosed while parsing; this assertion prevents a value
        // computed at runtime from silently turning an invalid range into an empty sequence.
        ast::AstNode* validDirection = makeBinaryExpression(
            TokenKind::PipePipe,
            makeBinaryExpression(
                TokenKind::AmpAmp,
                makeBinaryExpression(TokenKind::Less, id(rangeBeginName),
                    id(rangeEndName)),
                makeBinaryExpression(TokenKind::Greater, id(rangeStepName),
                    makeIntegerLiteralExpression(range, 0))),
            makeBinaryExpression(
                TokenKind::PipePipe,
                makeBinaryExpression(
                    TokenKind::AmpAmp,
                    makeBinaryExpression(TokenKind::Greater, id(rangeBeginName),
                        id(rangeEndName)),
                    makeBinaryExpression(TokenKind::Less, id(rangeStepName),
                        makeIntegerLiteralExpression(range, 0))),
                makeBinaryExpression(TokenKind::EqualEqual, id(rangeBeginName),
                    id(rangeEndName))));
        ast::AstNode* nonZero = makeBinaryExpression(
            TokenKind::BangEqual, id(rangeStepName),
            makeIntegerLiteralExpression(range, 0));
        ast::AstNode* assertion = makeZeroArgCallExpression(id("assert"));
        assertion->setAuxiliaryText("builtin-assert");
        assertion->addChild(makeBinaryExpression(TokenKind::AmpAmp, nonZero, validDirection));
        lowered->addChild(makeExpressionStatement(assertion));
        lowered->addChild(loop);
        return lowered;
    }

    // first(): `var $__foreach_<id>_source = <collection>;` -- evaluates the collection expression
    // exactly once, regardless of which candidate below ends up chosen. Whether this ends up
    // `raii` is decided by ProjectMethodBodyModel::analyzeForeach (P0.2, see above) using the
    // `collectionProbe` copy stashed as this ForeachStatement node's own extra child below --
    // NOT here, since the parser has no type information yet. Built as plain (non-raii) `var`
    // here; analyzeForeach adds the `raii` modifier onto THIS node in place when the probe finds
    // the source is a genuinely owned-returning expression.
    //
    // The `$` in every synthesized identifier below (P0.1 of the closure review, "higiene de
    // temporales"): NativeSharp's lexer only ever accepts `[A-Za-z_][A-Za-z0-9_]*` for a
    // user-written identifier (see Lexer.cpp's isIdentifierStart/isIdentifierPart) -- `$` can
    // never appear in ANY identifier a real NativeSharp source file could lex. Every name
    // synthesized by this function therefore contains `$`, which makes it categorically
    // impossible for user source (however it's spelled) to ever resolve to, shadow, or reassign
    // one of these compiler-internal locals through ordinary name lookup -- true lexical hygiene,
    // not merely an "unlikely enough" naming convention. `$` is also a legal, unquoted LLVM IR
    // identifier character, so this needs no companion change anywhere in HIR/MIR/LLVM lowering
    // or in the debug names those layers emit.
    ast::AstNode* sourceDecl = makeVariableDeclaration(varMarker(), sourceName, collection, false);

    // second(): the array-mode candidate.
    //   for (int __foreach_index = 0; __foreach_index < __foreach_source.Length; __foreach_index = __foreach_index + 1) {
    //       <ElementType> <name> = __foreach_source[__foreach_index];
    //       { body }
    //   }
    //
    // This is built as a REAL ForStatement (first()=init, second()=condition, third()=increment,
    // body=the single child block) rather than a hand-spliced `while` with the increment tacked on
    // as a trailing statement inside the body. That distinction is not cosmetic: a `continue`
    // inside a WhileStatement's body jumps straight to the condition check, so a trailing
    // increment statement placed after the (possibly `continue`-skipped) loop body would simply
    // never run whenever the caller's body used `continue` -- an infinite loop on the same index,
    // caught by the Wine execution smoke test in 52_x_nativesharp_foreach.md. A ForStatement's
    // `continue` already jumps to the DEDICATED increment slot instead (see
    // ProjectOopHirModel::lowerStatement's ForStatement case and
    // ProjectOopLlvmLowerer::lowerFor's `incBlock`, both pre-existing and unmodified by foreach),
    // so building this candidate out of the SAME node kind real `for` loops use gets correct
    // continue semantics for free, with no foreach-specific control-flow code anywhere.
    ast::AstNode* arrayCandidate;
    {
        ast::TypeSyntax* intType = arena_.make<ast::TypeSyntax>(range, TokenKind::KwInt, std::string("int"));
        ast::AstNode* indexDecl = makeVariableDeclaration(
            intType, indexName, makeIntegerLiteralExpression(range, 0), false);

        ast::AstNode* condition = makeBinaryExpression(
            TokenKind::Less,
            makeIdentifierExpression(range, indexName),
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Length"));

        ast::AstNode* increment = makeAssignmentExpression(
            makeIdentifierExpression(range, indexName),
            makeBinaryExpression(
                TokenKind::Plus,
                makeIdentifierExpression(range, indexName),
                makeIntegerLiteralExpression(range, 1)));

        // isConst=true (P0.3 of the closure review): the foreach element is unconditionally
        // read-only, exactly like real C#'s iteration variable. Reuses the SAME enforcement
        // mechanism an ordinary `const T x = ...;` local already relies on (setFlag(isConst) at
        // parse time here, read back as local.isConst in ProjectMethodBodyModel::analyzeVariable,
        // which later gates `result.assignable`) -- no new machinery, just a different call site
        // feeding the existing flag.
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeIndexExpression(
                makeIdentifierExpression(range, sourceName),
                makeIdentifierExpression(range, indexName)),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureElementBinding(elementDecl);

        ast::AstNode* forBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        emitElementBindings(forBody);
        forBody->addChild(body);

        ast::AstNode* forNode = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, range);
        forNode->setFirst(indexDecl);
        forNode->setSecond(condition);
        forNode->setThird(increment);
        forNode->addChild(forBody);

        arrayCandidate = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        arrayCandidate->addChild(forNode);
    }

    // NativeSharp.Core Span<T>/ReadOnlySpan<T> are compiler-recognized borrowed views. Their
    // iteration uses Length() once per loop condition and Data()[index] for the element place.
    // Data() is a raw pointer, so the loop proof is the only bounds contract: no redundant
    // managed-array/indexer guard is emitted for every element. ReadOnlySpan mutation is rejected
    // semantically once the resolved source type is known.
    ast::AstNode* spanCandidate;
    {
        ast::TypeSyntax* intType = arena_.make<ast::TypeSyntax>(
            range, TokenKind::KwInt, std::string("int"));
        ast::AstNode* indexDecl = makeVariableDeclaration(
            intType, indexName, makeIntegerLiteralExpression(range, 0), false);
        ast::AstNode* lengthMember = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Length"));
        ast::AstNode* condition = makeBinaryExpression(
            TokenKind::Less,
            makeIdentifierExpression(range, indexName),
            lengthMember);
        ast::AstNode* increment = makeAssignmentExpression(
            makeIdentifierExpression(range, indexName),
            makeBinaryExpression(
                TokenKind::Plus,
                makeIdentifierExpression(range, indexName),
                makeIntegerLiteralExpression(range, 1)));
        ast::AstNode* dataCall = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Data"));
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeIndexExpression(
                dataCall,
                makeIdentifierExpression(range, indexName)),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureElementBinding(elementDecl);
        ast::AstNode* forBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        emitElementBindings(forBody);
        forBody->addChild(body);
        ast::AstNode* forNode = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ForStatement, range);
        forNode->setFirst(indexDecl);
        forNode->setSecond(condition);
        forNode->setThird(increment);
        forNode->addChild(forBody);
        spanCandidate = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, range);
        spanCandidate->addChild(forNode);
    }

    // third(): the enumerator-pattern candidate.
    //   raii var $__foreach_<id>_enumerator = $__foreach_<id>_source.GetEnumerator();
    //   while ($__foreach_<id>_enumerator.MoveNext()) {
    //       <ElementType> <name> = $__foreach_<id>_enumerator.Current;
    //       { body }
    //   }
    //
    // `Current` is read here as a plain property access (P1.1 of the closure review), not a
    // zero-arg method call: real C#'s enumerator pattern (`IEnumerator<T>.Current`) is a
    // get-only PROPERTY, and NativeSharp has supported properties since Stage 38, so the
    // previous `Current()`-as-method desugaring was an unnecessary deviation from the structural
    // convention this feature is supposed to mirror. Every enumerator class this compiler ships
    // or documents (ListEnumerator<T>, examples/project_stage52x_foreach's CountUpToEnumerator)
    // is updated alongside this change to declare `Current` as a property instead of a method --
    // see lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns.
    ast::AstNode* patternCandidate;
    {
        ast::AstNode* getEnumeratorCall = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "GetEnumerator"));
        ast::AstNode* enumeratorDecl = makeVariableDeclaration(
            varMarker(), enumeratorName, getEnumeratorCall, false);
        // This spelling cannot come from source: it is compiler-owned metadata on the synthetic
        // declaration. ProjectMethodBodyModel turns it into RAII only when the inferred type is
        // reference-shaped, avoiding a heap-only contract for value enumerators.
        enumeratorDecl->setAuxiliaryText("local:foreach-enumerator");

        ast::AstNode* condition = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, enumeratorName), "MoveNext"));

        // isConst=true here too (P0.3) -- both candidates must agree on the element's
        // read-only-ness, since exactly one of them is the one actually analyzed/lowered and the
        // other is discarded; the user-visible semantics must not depend on which was chosen.
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeMemberExpression(makeIdentifierExpression(range, enumeratorName), "Current"),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureElementBinding(elementDecl);

        ast::AstNode* whileBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        whileBody->addChild(elementDecl);
        emitElementBindings(whileBody);
        whileBody->addChild(body);

        ast::AstNode* whileNode = arena_.make<ast::AstNode>(ast::AstNodeKind::WhileStatement, range);
        whileNode->setFirst(condition);
        whileNode->setSecond(whileBody);

        patternCandidate = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        patternCandidate->addChild(enumeratorDecl);
        patternCandidate->addChild(whileNode);
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ForeachStatement, range);
    node->setFirst(sourceDecl);
    node->setSecond(arrayCandidate);
    node->setThird(patternCandidate);
    // Fourth association (P0.2): the throwaway, never-lowered probe copy of the collection
    // expression, attached as an ordinary extra child so ProjectMethodBodyModel::analyzeForeach
    // can reach it via node->children()[0] to decide sourceDecl's raii-ness. See the long comment
    // above collectionProbe's construction for why this needs to be a SEPARATE parse rather than
    // a second analysis pass over `collection` itself.
    node->addChild(collectionProbe);
    node->addChild(spanCandidate);
    node->setAuxiliaryText(elementByReference
        ? (elementMutableReference ? "foreach:ref-mut" : "foreach:ref")
        : "foreach:value");
    return node;
}

ast::AstNode* Parser::parseReturnStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    const bool isReferenceReturn = atContextualKeyword("ref") && peek().kind() != TokenKind::Semicolon;
    if (isReferenceReturn) consume();
    ast::AstNode* value = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        value = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ReturnStatement, tokenRangeFrom(beginIndex));
    node->setFirst(value);
    node->setReference(isReferenceReturn);
    return node;
}

ast::AstNode* Parser::parseDeferStatement()
{
    const std::size_t beginIndex = index_;
    const SourceRange deferRange = consume().range();

    struct Capture final
    {
        bool byReference = false;
        bool receiver = false;
        std::string name;
        SourceRange range;
    };
    std::vector<Capture> captures;
    std::unordered_set<std::string> names;

    expect(TokenKind::LeftBracket, "'[' after defer");
    if (!at(TokenKind::RightBracket))
    {
        while (true)
        {
            bool byReference = false;
            if (atContextualKeyword("ref"))
            {
                byReference = true;
                consume();
            }
            else if (at(TokenKind::KwMove) || atContextualKeyword("move"))
            {
                consume();
            }
            else
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedToken,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "defer capture requires explicit 'ref' or 'move'");
            }

            Capture capture;
            capture.byReference = byReference;
            capture.range = current().range();
            if (at(TokenKind::KwThis))
            {
                capture.receiver = true;
                capture.name = "this";
                consume();
                if (!byReference)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        capture.range,
                        "defer can capture this only by reference");
                }
            }
            else if (at(TokenKind::Identifier))
            {
                capture.name = std::string(consume().lexeme());
            }
            else
            {
                expect(TokenKind::Identifier, "defer capture name");
                capture.name = "__invalid_defer_capture";
            }

            if (!names.insert(capture.name).second)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    capture.range,
                    "duplicate defer capture '" + capture.name + "'");
            }
            captures.push_back(std::move(capture));
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightBracket, "']' after defer captures");

    ast::AstNode* body = nullptr;
    if (at(TokenKind::LeftBrace)) body = parseBlock();
    else
    {
        expect(TokenKind::LeftBrace, "'{' after defer capture list");
        body = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, deferRange);
    }

    std::string allowedNames;
    for (std::size_t index = 0; index < captures.size(); ++index)
    {
        if (index != 0) allowedNames.push_back(',');
        allowedNames += captures[index].name;
    }
    body->setAuxiliaryText("defer-capture-boundary");
    body->setText(std::move(allowedNames));

    const std::size_t deferOrdinal = nextDeferTemporary_++;
    const auto varType = [&](SourceRange range)
    {
        return arena_.make<ast::TypeSyntax>(range, TokenKind::KwVar, std::string("var"));
    };
    const auto moveExpression = [&](SourceRange range, std::string name)
    {
        ast::AstNode* move = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression, range);
        move->setFirst(makeIdentifierExpression(range, std::move(name)));
        move->setAuxiliaryText("defer-value-capture");
        return move;
    };

    ast::AstNode* registration = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, deferRange);
    registration->setAuxiliaryText("defer-registration");
    ast::AstNode* cleanup = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, body->range());

    for (const Capture& capture : captures)
    {
        if (capture.receiver) continue;
        const std::string hidden = "__nsc_defer_capture_" +
            std::to_string(deferOrdinal) + "_" + capture.name;
        if (!capture.byReference)
        {
            ast::AstNode* outer = makeVariableDeclaration(
                varType(capture.range), hidden,
                moveExpression(capture.range, capture.name), false);
            outer->setAuxiliaryText("local:defer-value-capture");
            registration->addChild(outer);
        }

        ast::AstNode* innerInitializer = capture.byReference
            ? makeIdentifierExpression(capture.range, capture.name)
            : moveExpression(capture.range, hidden);
        ast::AstNode* inner = makeVariableDeclaration(
            varType(capture.range), capture.name, innerInitializer, false);
        inner->setAuxiliaryText(
            capture.byReference ? "local:ref" : "local:defer-value-capture");
        cleanup->addChild(inner);
    }
    cleanup->addChild(body);

    ast::AstNode* protectedBlock = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, deferRange);
    ast::AstNode* lowered = arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement, tokenRangeFrom(beginIndex));
    lowered->setFirst(protectedBlock);
    lowered->setSecond(cleanup);
    lowered->setAuxiliaryText("defer-lowered");
    registration->addChild(lowered);
    registration->setRange(tokenRangeFrom(beginIndex));
    return registration;
}

ast::AstNode* Parser::parseTryStatement()
{
    const std::size_t beginIndex = index_;
    consume();

    ast::AstNode* tryBlock = nullptr;
    if (at(TokenKind::LeftBrace))
    {
        tryBlock = parseBlock();
    }
    else
    {
        expect(TokenKind::LeftBrace, "'{' after try");
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement,
        tokenRangeFrom(beginIndex));
    node->setFirst(tryBlock);

    while (at(TokenKind::KwCatch))
    {
        node->addChild(parseCatchClause());
    }

    bool hasFinally = false;
    if (consumeIf(TokenKind::KwFinally))
    {
        hasFinally = true;
        ast::AstNode* finallyBlock = nullptr;
        if (at(TokenKind::LeftBrace))
        {
            finallyBlock = parseBlock();
        }
        else
        {
            expect(TokenKind::LeftBrace, "'{' after finally");
        }
        node->setSecond(finallyBlock);
    }

    // A protected block is not a complete try statement on its own. Report the missing handler
    // without consuming the following token so normal statement recovery remains in control.
    if (node->children().empty() && !hasFinally)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected 'catch' or 'finally' after try block");
    }

    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseCatchClause()
{
    const std::size_t beginIndex = index_;
    consume();

    ast::AstNode* clause = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CatchClause,
        tokenRangeFrom(beginIndex));

    if (consumeIf(TokenKind::LeftParen))
    {
        if (canStartTypeAt(index_))
        {
            clause->setTypeSyntax(parseType());
        }
        else
        {
            expect(TokenKind::Identifier, "exception type");
        }

        // The exception name is optional. `catch(Type)` observes only the dynamic type while
        // `catch(Type value)` additionally introduces a local in the catch scope.
        if (at(TokenKind::Identifier))
            clause->setText(std::string(consume().lexeme()));

        expect(TokenKind::RightParen, "')' after catch declaration");
    }
    else
    {
        clause->setFlag(true);
    }

    // Catch filters are contextual so existing identifiers named `when` remain
    // source-compatible outside this exact position. Keep the predicate separate from the
    // handler body: first() is the body and second() is the optional boolean filter.
    if (atContextualKeyword("when"))
    {
        consume();
        expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
        clause->setSecond(parseExpression());
        expect(TokenKind::RightParen, "')' after catch filter expression");
    }

    if (at(TokenKind::LeftBrace))
    {
        clause->setFirst(parseBlock());
    }
    else
    {
        expect(TokenKind::LeftBrace, "'{' after catch");
    }

    clause->setRange(tokenRangeFrom(beginIndex));
    return clause;
}

ast::AstNode* Parser::parseThrowStatement()
{
    const std::size_t beginIndex = index_;
    consume();

    ast::AstNode* value = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        value = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ThrowStatement,
        tokenRangeFrom(beginIndex));
    node->setFirst(value);
    return node;
}

ast::AstNode* Parser::parseUsingStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'(' after using");

    ast::TypeSyntax* resourceType = nullptr;
    if (canStartTypeAt(index_))
    {
        resourceType = parseType();
    }
    else
    {
        expect(TokenKind::Identifier, "resource type");
    }

    std::string resourceName;
    if (at(TokenKind::Identifier))
    {
        resourceName = std::string(consume().lexeme());
    }
    else
    {
        expect(TokenKind::Identifier, "resource variable name");
    }

    ast::AstNode* initializer = nullptr;
    if (expect(TokenKind::Equal, "'=' in using resource declaration"))
    {
        initializer = parseExpression();
    }
    expect(TokenKind::RightParen, "')' after using resource declaration");

    ast::AstNode* body = nullptr;
    if (at(TokenKind::LeftBrace))
    {
        body = parseBlock();
    }
    else
    {
        expect(TokenKind::LeftBrace, "'{' after using declaration");
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingStatement,
        tokenRangeFrom(beginIndex));
    node->setTypeSyntax(resourceType);
    node->setText(std::move(resourceName));
    node->setFirst(initializer);
    node->setSecond(body);
    return node;
}

ast::AstNode* Parser::parseExpression()
{
    return parseAssignmentExpression();
}

ast::AstNode* Parser::parseAssignmentExpression()
{
    if (looksLikeLambdaExpression())
    {
        return parseLambdaExpression();
    }

    ast::AstNode* left = parseConditionalExpression();
    if (!lexer::isAssignmentOperator(current().kind()))
    {
        return left;
    }

    const TokenKind operation = consume().kind();
    ast::AstNode* right = parseAssignmentExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression,
        join(left->range(), right->range()));
    node->setTokenKind(operation);
    node->setFirst(left);
    node->setSecond(right);
    return node;
}

// `condition ? whenTrue : whenFalse`, between assignment and logical-or in precedence, and
// RIGHT-associative -- so `a ? b : c ? d : e` is `a ? b : (c ? d : e)`, and the false branch
// accepts an assignment (`c ? x : y = z` is `c ? x : (y = z)`), which is what C# does.
//
// The true branch parses as a full expression because it sits between `?` and `:`, where a
// nested conditional cannot be ambiguous.
ast::AstNode* Parser::parseConditionalExpression()
{
    ast::AstNode* condition = parseNullCoalescingExpression();
    if (!at(TokenKind::Question))
    {
        return condition;
    }

    consume();
    ast::AstNode* whenTrue = parseExpression();
    expect(TokenKind::Colon, "':' in a conditional expression");
    ast::AstNode* whenFalse = parseAssignmentExpression();

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConditionalExpression,
        join(condition->range(), whenFalse != nullptr ? whenFalse->range() : condition->range()));
    node->setFirst(condition);
    node->setSecond(whenTrue);
    node->setThird(whenFalse);
    return node;
}

ast::AstNode* Parser::parseNullCoalescingExpression()
{
    ast::AstNode* left = parseRangeExpression();
    if (!at(TokenKind::QuestionQuestion)) return left;
    const SourceRange operation = consume().range();
    ast::AstNode* right = parseNullCoalescingExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NullCoalescingExpression,
        join(left->range(), right != nullptr ? right->range() : operation));
    node->setTokenKind(TokenKind::QuestionQuestion);
    node->setFirst(left);
    node->setSecond(right);
    return node;
}

// Integer ranges are ordinary expressions as well as index operands. Keeping the
// operator below null-coalescing but above logical-or lets `foreach (... in begin..end)` reach the
// same RangeExpression node that slicing already uses. Bracket parsing continues to handle the
// fully open `[..]` spelling; this level admits an omitted endpoint only at a delimiter so it
// never consumes a statement or argument that follows the range.
ast::AstNode* Parser::parseRangeExpression()
{
    // A range delimiter may open the expression. Do not ask the primary-expression path to
    // manufacture an operand for `..end`; the missing begin endpoint is part of the range shape.
    ast::AstNode* begin = nullptr;
    if (!at(TokenKind::DotDot) && !at(TokenKind::DotDotEqual))
        begin = parseLogicalOrExpression();
    if (!at(TokenKind::DotDot) && !at(TokenKind::DotDotEqual)) return begin;

    const lexer::Token& delimiter = consume();
    const SourceRange operation = delimiter.range();
    const bool inclusive = delimiter.kind() == TokenKind::DotDotEqual;
    ast::AstNode* end = nullptr;
    if (!at(TokenKind::EndOfFile) && !at(TokenKind::RightParen) && !at(TokenKind::RightBracket) &&
        !at(TokenKind::Comma) && !at(TokenKind::Semicolon))
    {
        end = parseLogicalOrExpression();
    }
    ast::AstNode* step = nullptr;
    if (atContextualKeyword("step"))
    {
        const SourceRange stepKeyword = consume().range();
        if (at(TokenKind::RightParen) || at(TokenKind::RightBracket) ||
            at(TokenKind::Comma) || at(TokenKind::Semicolon))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error,
                stepKeyword,
                "expected an integral expression after range step");
        }
        else
        {
            step = parseLogicalOrExpression();
        }
    }
    if (inclusive && end == nullptr)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            operation,
            "an inclusive range requires an end endpoint");
    }
    ast::AstNode* range = arena_.make<ast::AstNode>(
        ast::AstNodeKind::RangeExpression,
        SourceRange(begin != nullptr ? begin->range().begin() : operation.begin(),
            step != nullptr ? step->range().end() :
            (end != nullptr ? end->range().end() : operation.end())));
    range->setTokenKind(delimiter.kind());
    range->setFirst(begin);
    range->setSecond(end);
    range->setThird(step);
    range->setFlag(inclusive);

    const std::optional<std::int64_t> constantBegin = signedIntegerLiteral(begin);
    const std::optional<std::int64_t> constantEnd = signedIntegerLiteral(end);
    const std::optional<std::int64_t> constantStep = step != nullptr
        ? signedIntegerLiteral(step) : std::optional<std::int64_t>(1);
    if (constantBegin.has_value() && constantEnd.has_value() && constantStep.has_value())
    {
        std::string rangeError;
        if (!language::lazyRangeCount(
                {*constantBegin, *constantEnd, *constantStep, inclusive}, rangeError).has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                range->range(),
                rangeError);
        }
    }
    return range;
}

bool Parser::looksLikeLambdaExpression() const noexcept
{
    const auto looksLikeParameterHead = [&](std::size_t begin) noexcept
    {
        const auto hasFatArrowBeforeBoundary = [&](std::size_t cursor) noexcept
        {
            for (; cursor < tokens_.size(); ++cursor)
            {
                const TokenKind kind = tokens_[cursor].kind();
                if (kind == TokenKind::FatArrow) return true;
                if (kind == TokenKind::Semicolon || kind == TokenKind::LeftBrace ||
                    kind == TokenKind::RightBrace || kind == TokenKind::EndOfFile)
                    return false;
            }
            return false;
        };
        if (begin >= tokens_.size()) return false;
        // T08-F04.  `<T>(T value) => ...` is unambiguous only after the complete generic head
        // and parameter list have been seen.  Do not treat an arbitrary less-than expression as
        // a lambda: find the matching `>` first and then reuse the ordinary parameter probe.
        if (tokens_[begin].kind() == TokenKind::Less)
        {
            std::size_t cursor = begin + 1;
            std::size_t depth = 1;
            while (cursor < tokens_.size())
            {
                const TokenKind kind = tokens_[cursor].kind();
                if (kind == TokenKind::Less) ++depth;
                else if (kind == TokenKind::Greater && --depth == 0)
                {
                    begin = cursor + 1;
                    break;
                }
                else if (kind == TokenKind::Semicolon || kind == TokenKind::FatArrow)
                {
                    return false;
                }
                ++cursor;
            }
            if (depth != 0 || begin >= tokens_.size()) return false;
        }
        if (tokens_[begin].kind() == TokenKind::Identifier)
        {
            if (begin + 1 >= tokens_.size()) return false;
            if (tokens_[begin + 1].kind() == TokenKind::FatArrow) return true;
            return tokens_[begin + 1].kind() == TokenKind::Arrow &&
                hasFatArrowBeforeBoundary(begin + 2);
        }
        if (tokens_[begin].kind() != TokenKind::LeftParen) return false;

        // E11: the old recognizer only admitted `(x, y) => ...`; consequently an explicitly
        // typed lambda was parsed as a parenthesized expression. `=>` after the matching ')'
        // is unambiguous. Track nesting so array/generic syntax cannot terminate the probe.
        std::size_t cursor = begin + 1;
        std::size_t parenthesisDepth = 1;
        std::size_t bracketDepth = 0;
        while (cursor < tokens_.size())
        {
            const TokenKind kind = tokens_[cursor].kind();
            if (kind == TokenKind::LeftParen) ++parenthesisDepth;
            else if (kind == TokenKind::RightParen)
            {
                if (--parenthesisDepth == 0)
                {
                    if (cursor + 1 >= tokens_.size()) return false;
                    if (tokens_[cursor + 1].kind() == TokenKind::FatArrow) return true;
                    if (tokens_[cursor + 1].kind() == TokenKind::Arrow)
                        return hasFatArrowBeforeBoundary(cursor + 2);
                    if (tokens_[cursor + 1].kind() != TokenKind::Identifier ||
                        tokens_[cursor + 1].lexeme() != "where")
                        return false;
                    // Constraint clauses contain types, commas and equality operators but no
                    // statement boundary. The first fat arrow closes the generic-lambda head.
                    return hasFatArrowBeforeBoundary(cursor + 2);
                }
            }
            else if (kind == TokenKind::LeftBracket) ++bracketDepth;
            else if (kind == TokenKind::RightBracket && bracketDepth != 0) --bracketDepth;
            else if (kind == TokenKind::Semicolon && parenthesisDepth == 1 && bracketDepth == 0)
                return false;
            ++cursor;
        }
        return false;
    };

    // NS304-T04-F05: `static` marca un lambda/funcion local sin captura del entorno.
    if (at(TokenKind::KwStatic) && index_ + 1 < tokens_.size() &&
        looksLikeParameterHead(index_ + 1))
    {
        return true;
    }
    // NS304-T04-F02: `noheap`/`unsafe` son prefijos de EFECTO de lambda. Se reconocen como
    // contextuales delante de una cabecera de parametros real; no se reservan como keywords.
    if ((at(TokenKind::KwNoHeap) || atContextualKeyword("unsafe")) &&
        index_ + 1 < tokens_.size() && looksLikeParameterHead(index_ + 1))
    {
        return true;
    }
    if (at(TokenKind::KwExtern) && index_ + 2 < tokens_.size() &&
        tokens_[index_ + 1].kind() == TokenKind::KwC &&
        looksLikeParameterHead(index_ + 2))
    {
        return true;
    }
    if (!at(TokenKind::LeftBracket)) return looksLikeParameterHead(index_);

    // T08-F03. A leading capture list is recognized only when the token after its matching ']'
    // is itself a complete lambda head. This keeps ordinary array/index expressions out of the
    // lambda parser without reserving copy/ref/mut as global keywords.
    std::size_t cursor = index_ + 1;
    std::size_t depth = 1;
    while (cursor < tokens_.size())
    {
        if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
        else if (tokens_[cursor].kind() == TokenKind::RightBracket && --depth == 0)
            return looksLikeParameterHead(cursor + 1);
        else if (tokens_[cursor].kind() == TokenKind::Semicolon) return false;
        ++cursor;
    }
    return false;
}

ast::AstNode* Parser::parseLambdaExpression()
{
    const std::size_t beginIndex = index_;
    auto* lambda = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LambdaExpression, tokens_[beginIndex].range());
    // NS304-T04-F05: `static` = lambda sin captura. Se guarda como sufijo "static" del
    // auxiliaryText para que bindLambdaToDelegate lo fuerce a captura vacia.
    bool staticLambda = false;
    if (at(TokenKind::KwStatic)) { staticLambda = true; consume(); }
    // NS304-T04-F02: prefijos de efecto `noheap`/`unsafe` y calling convention `extern C`.
    std::string effects;
    if (at(TokenKind::KwNoHeap)) { effects = "noheap"; consume(); }
    else if (atContextualKeyword("unsafe")) { effects = "unsafe"; consume(); }
    std::string callingConvention;
    if (at(TokenKind::KwExtern) && peek().kind() == TokenKind::KwC)
    {
        callingConvention = "C";
        consume();
        consume();
    }
    std::string captureContract;
    std::string attributeContract;
    if (consumeIf(TokenKind::LeftBracket))
    {
        if (!at(TokenKind::RightBracket) && !atContextualKeyword("copy") &&
            !atContextualKeyword("move") && !at(TokenKind::KwMove) &&
            !atContextualKeyword("ref"))
        {
            // NS304-T04-F02: attribute [Attr] on a lambda (not a capture list).
            std::string attributeName;
            if (at(TokenKind::Identifier))
                attributeName = std::string(consume().lexeme());
            expect(TokenKind::RightBracket, "']' after lambda attribute");
            attributeContract = "attribute:" + attributeName;
        }
        else
        {
            captureContract = "captures:";
            std::unordered_set<std::string> captureNames;
            if (!at(TokenKind::RightBracket))
            {
                while (true)
                {
                std::string mode;
                if (atContextualKeyword("copy"))
                {
                    consume();
                    mode = "copy";
                }
                else if (at(TokenKind::KwMove) || atContextualKeyword("move"))
                {
                    consume();
                    mode = "move";
                }
                else if (atContextualKeyword("ref"))
                {
                    consume();
                    if (atContextualKeyword("mut"))
                    {
                        consume();
                        mode = "mutref";
                    }
                    else
                    {
                        mode = "ref";
                    }
                }
                else
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedToken,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "expected capture mode 'copy', 'move', 'ref', or 'ref mut'");
                    if (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile)) consume();
                    mode = "invalid";
                }

                std::string name;
                SourceRange nameRange = current().range();
                if (at(TokenKind::Identifier))
                {
                    name = std::string(consume().lexeme());
                }
                else if (at(TokenKind::KwThis))
                {
                    name = "this";
                    consume();
                }
                else
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedToken,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "expected capture name or 'this'");
                    if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket) &&
                        !at(TokenKind::EndOfFile))
                        consume();
                }
                if (!name.empty() && !captureNames.insert(name).second)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        nameRange,
                        "duplicate lambda capture '" + name + "'");
                }
                if (!name.empty())
                {
                    if (captureContract.size() != std::string_view("captures:").size())
                        captureContract += ';';
                    captureContract += mode + "=" + name;
                }
                if (!consumeIf(TokenKind::Comma)) break;
                }
            }
            expect(TokenKind::RightBracket, "']' after lambda capture list");
        }
    }
    if (at(TokenKind::Less))
    {
        parseGenericParameterList(lambda);
    }
    std::vector<ast::AstNode*> parameters;
    if (at(TokenKind::Identifier))
    {
        const lexer::Token& parameter = consume();
        auto* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ParameterDeclaration, parameter.range());
        node->setText(std::string(parameter.lexeme()));
        parameters.push_back(node);
    }
    else
    {
        expect(TokenKind::LeftParen, "'('");
        if (!at(TokenKind::RightParen))
        {
            while (true)
            {
                // NS304-T04-F03: `(ref x, out y, in z)` — modificador de paso SIN tipo, inferido
                // del target fn*/delegate. El pass-mode viaja con el prefijo "pass:" (igual que
                // parseParameter) para que bindLambdaToDelegate lo coteje contra el target.
                const bool modifierParameter =
                    atContextualKeyword("ref") || atContextualKeyword("out") || at(TokenKind::KwIn);
                if (modifierParameter && peek().kind() == TokenKind::Identifier &&
                    (peek(2).kind() == TokenKind::Comma || peek(2).kind() == TokenKind::RightParen))
                {
                    const lexer::Token& modifier = consume();
                    const lexer::Token& parameter = consume();
                    auto* node = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::ParameterDeclaration, modifier.range());
                    node->setText(std::string(parameter.lexeme()));
                    node->setAuxiliaryText("pass:" + std::string(modifier.lexeme()));
                    parameters.push_back(node);
                }
                // `(x, y)` keeps contextual parameter types.  Every other spelling goes through
                // the ordinary parameter parser, making `(int x, ref int y)` use the same type,
                // modifier and pass-mode AST representation as a method/delegate declaration.
                else if (at(TokenKind::Identifier) &&
                    (peek().kind() == TokenKind::Comma || peek().kind() == TokenKind::RightParen))
                {
                    const lexer::Token& parameter = consume();
                    auto* node = arena_.make<ast::AstNode>(
                        ast::AstNodeKind::ParameterDeclaration, parameter.range());
                    node->setText(std::string(parameter.lexeme()));
                    parameters.push_back(node);
                }
                else
                {
                    ast::AstNode* parameter = parseParameter();
                    if (parameter != nullptr) parameters.push_back(parameter);
                }
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')'");
    }
    // `auto` is intentionally restricted to generic-lambda parameters.  Each occurrence is an
    // independent inferred type parameter; users who want equality write the explicit `<T>`
    // form twice.  Rewriting the TypeSyntax here lets every later generic substitution path use
    // the same representation as an explicitly named parameter.
    std::size_t automaticOrdinal = 0;
    for (ast::AstNode* parameter : parameters)
    {
        if (parameter == nullptr || parameter->typeSyntax() == nullptr ||
            parameter->typeSyntax()->baseName() != "auto")
            continue;
        const std::string name = "__lambda_auto_" + std::to_string(automaticOrdinal++);
        ast::GenericParameterSyntax inferred;
        inferred.name = name;
        inferred.kind = ast::GenericParameterSyntaxKind::Type;
        inferred.range = parameter->typeSyntax()->range();
        lambda->addGenericParameter(std::move(inferred));
        parameter->typeSyntax()->setBaseName(name);
    }
    if (!lambda->genericParameters().empty())
        parseGenericConstraintClauses(lambda);
    if (consumeIf(TokenKind::Arrow))
        lambda->setTypeSyntax(parseType());
    expect(TokenKind::FatArrow, "'=>'");

    lambda->setRange(tokenRangeFrom(beginIndex));
    std::string auxiliary = captureContract;
    if (staticLambda)
    {
        if (!auxiliary.empty()) auxiliary += ';';
        auxiliary += "static";
    }
    if (!effects.empty())
    {
        if (!auxiliary.empty()) auxiliary += ';';
        auxiliary += "effects:" + effects;
    }
    if (!callingConvention.empty())
    {
        if (!auxiliary.empty()) auxiliary += ';';
        auxiliary += "calling-convention:" + callingConvention;
    }
    if (!attributeContract.empty())
    {
        if (!auxiliary.empty()) auxiliary += ';';
        auxiliary += attributeContract;
    }
    lambda->setAuxiliaryText(std::move(auxiliary));
    for (ast::AstNode* parameter : parameters) lambda->addChild(parameter);
    if (at(TokenKind::LeftBrace))
    {
        lambda->setFlag(true);
        lambda->setFirst(parseBlock());
    }
    else
    {
        lambda->setFlag(false);
        lambda->setFirst(parseAssignmentExpression());
    }
    if (lambda->first() != nullptr)
    {
        lambda->setRange(SourceRange(tokens_[beginIndex].range().begin(), lambda->first()->range().end()));
    }
    return lambda;
}

ast::AstNode* Parser::parseLogicalOrExpression()
{
    return parseBinaryLevel(&Parser::parseLogicalAndExpression, kLogicalOr, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseLogicalAndExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseOrExpression, kLogicalAnd, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseOrExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseXorExpression, kBitwiseOr, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseXorExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseAndExpression, kBitwiseXor, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseAndExpression()
{
    return parseBinaryLevel(&Parser::parseEqualityExpression, kBitwiseAnd, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseEqualityExpression()
{
    return parseBinaryLevel(&Parser::parseRelationalExpression, kEquality, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseRelationalExpression()
{
    ast::AstNode* left = parseShiftExpression();
    while (contains(kRelational, current().kind()) || at(TokenKind::KwIs) || at(TokenKind::KwAs))
    {
        if (at(TokenKind::KwIs) || at(TokenKind::KwAs))
        {
            const TokenKind operation = consume().kind();
            ast::TypeSyntax* targetType = parseType();
            if (targetType == nullptr)
            {
                return left;
            }
            ast::AstNode* node = arena_.make<ast::AstNode>(
                operation == TokenKind::KwIs
                    ? ast::AstNodeKind::TypeTestExpression
                    : ast::AstNodeKind::SafeCastExpression,
                join(left->range(), targetType->range()));
            node->setTokenKind(operation);
            node->setTypeSyntax(targetType);
            node->setFirst(left);
            left = node;
            continue;
        }

        const TokenKind operation = consume().kind();
        ast::AstNode* right = parseShiftExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BinaryExpression,
            join(left->range(), right->range()));
        node->setTokenKind(operation);
        node->setFirst(left);
        node->setSecond(right);
        left = node;
    }
    return left;
}

ast::AstNode* Parser::parseShiftExpression()
{
    return parseBinaryLevel(&Parser::parseAdditiveExpression, kShift, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseAdditiveExpression()
{
    return parseBinaryLevel(&Parser::parseMultiplicativeExpression, kAdditive, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseMultiplicativeExpression()
{
    return parseBinaryLevel(&Parser::parseUnaryExpression, kMultiplicative, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseUnaryExpression()
{
    if (atContextualKeyword("await") && peek().kind() != TokenKind::Equal &&
        peek().kind() != TokenKind::LeftParen)
    {
        const SourceRange awaitRange = consume().range();
        // NS304-T05-F01: `await` ya es una expresion de primer nivel (AwaitExpression). La
        // transformacion productiva (state machine) es T05-F03; el Sema la rechaza hasta entonces.
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::AwaitExpression, awaitRange);
        node->setFirst(operand);
        return node;
    }

    if (at(TokenKind::Caret))
    {
        const SourceRange start = consume().range();
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FromEndExpression,
            join(start, operand != nullptr ? operand->range() : start));
        node->setTokenKind(TokenKind::Caret);
        node->setFirst(operand);
        return node;
    }

    if (at(TokenKind::KwMove))
    {
        const SourceRange start = consume().range();
        expect(TokenKind::LeftParen, "'(' after move");
        ast::AstNode* operand = parseExpression();
        expect(TokenKind::RightParen, "')' after move operand");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MoveExpression,
            join(start, tokens_[index_ - 1].range()));
        node->setFirst(operand);
        return node;
    }

    const TokenKind kind = current().kind();
    if (kind == TokenKind::Plus || kind == TokenKind::Minus || kind == TokenKind::Bang ||
        kind == TokenKind::Tilde || kind == TokenKind::Amp || kind == TokenKind::Star ||
        kind == TokenKind::PlusPlus || kind == TokenKind::MinusMinus)
    {
        const SourceRange start = consume().range();
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression,
            join(start, operand->range()));
        node->setTokenKind(kind);
        node->setFirst(operand);
        return node;
    }

    // T42 named casts are contextual spellings, not globally reserved words. This keeps existing
    // identifiers source-compatible while ensuring a malformed/invalid cast receives a
    // conversion diagnostic instead of "generic method template not found".
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Less &&
        (current().lexeme() == "static_cast" || current().lexeme() == "reinterpret_cast" ||
         current().lexeme() == "const_cast"))
    {
        const lexer::Token& castName = consume();
        expect(TokenKind::Less, "'<' after named cast");
        ast::TypeSyntax* type = parseType();
        expect(TokenKind::Greater, "'>' after named cast target type");
        expect(TokenKind::LeftParen, "'(' after named cast type");
        ast::AstNode* operand = parseExpression();
        expect(TokenKind::RightParen, "')' after named cast operand");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::CastExpression,
            join(castName.range(), tokens_[index_ - 1].range()));
        node->setTypeSyntax(type);
        node->setFirst(operand);
        node->setAuxiliaryText("cast:" + std::string(castName.lexeme()));
        return node;
    }

    if (at(TokenKind::LeftParen) && looksLikeParenthesizedType())
    {
        const SourceRange start = consume().range();
        ast::TypeSyntax* type = parseType();
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::CastExpression, join(start, operand->range()));
        node->setTypeSyntax(type);
        node->setFirst(operand);
        return node;
    }

    if (at(TokenKind::KwOffsetof))
    {
        const SourceRange start = consume().range();
        expect(TokenKind::LeftParen, "'('");
        ast::TypeSyntax* type = parseType();
        expect(TokenKind::Comma, "','");
        std::string fieldName;
        if (at(TokenKind::Identifier))
        {
            fieldName = std::string(consume().lexeme());
        }
        else
        {
            expect(TokenKind::Identifier, "field name");
        }
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::OffsetofExpression,
            join(start, tokens_[index_ - 1].range()));
        node->setTypeSyntax(type);
        node->setText(std::move(fieldName));
        return node;
    }

    if (at(TokenKind::KwSizeof) || at(TokenKind::KwAlignof))
    {
        const bool alignmentQuery = at(TokenKind::KwAlignof);
        const SourceRange start = consume().range();
        expect(TokenKind::LeftParen, "'('");
        if (canStartTypeAt(index_))
        {
            const std::size_t checkpoint = index_;
            std::size_t cursor = checkpoint;
            bool typeForm = true;

            if (lexer::isOwnershipQualifierKeyword(tokens_[cursor].kind()))
            {
                ++cursor;
            }

            if (tokens_[cursor].kind() == TokenKind::Identifier)
            {
                ++cursor;
                while (cursor + 1 < tokens_.size() &&
                       tokens_[cursor].kind() == TokenKind::Dot &&
                       tokens_[cursor + 1].kind() == TokenKind::Identifier)
                {
                    cursor += 2;
                }
            }
            else
            {
                ++cursor;
            }

            while (cursor < tokens_.size())
            {
                if (tokens_[cursor].kind() == TokenKind::Star)
                {
                    ++cursor;
                    continue;
                }

                if (tokens_[cursor].kind() == TokenKind::LeftBracket)
                {
                    int depth = 1;
                    ++cursor;
                            while (cursor < tokens_.size() && depth != 0)
                    {
                        if (tokens_[cursor].kind() == TokenKind::LeftBracket)
                        {
                            ++depth;
                        }
                        else if (tokens_[cursor].kind() == TokenKind::RightBracket)
                        {
                            --depth;
                        }
                        else if (tokens_[cursor].kind() == TokenKind::EndOfFile)
                        {
                            typeForm = false;
                            break;
                        }
                        ++cursor;
                    }

                    if (!typeForm || depth != 0)
                    {
                        typeForm = false;
                        break;
                    }
                    continue;
                }
                break;
            }

            typeForm = typeForm && cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::RightParen;
            if (typeForm)
            {
                ast::TypeSyntax* type = parseType();
                expect(TokenKind::RightParen, "')'");
                ast::AstNode* node = arena_.make<ast::AstNode>(
                    alignmentQuery ? ast::AstNodeKind::AlignofTypeExpression
                                   : ast::AstNodeKind::SizeofTypeExpression,
                    tokenRangeFrom(checkpoint));
                node->setTypeSyntax(type);
                node->setRange(join(start, tokens_[index_ - 1].range()));
                return node;
            }
        }

        ast::AstNode* expression = parseExpression();
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            alignmentQuery ? ast::AstNodeKind::AlignofExpression
                           : ast::AstNodeKind::SizeofExpression,
            join(start, tokens_[index_ - 1].range()));
        node->setFirst(expression);
        return node;
    }

    // Stage 52: default(T) -- unlike sizeof, C#'s default expression always takes a TYPE (never
    // a value expression), so this needs none of sizeof's type-vs-expression disambiguation:
    // just parse the parenthesized type directly, mirroring SizeofTypeExpression's shape.
    if (at(TokenKind::KwDefault))
    {
        const SourceRange start = consume().range();
        expect(TokenKind::LeftParen, "'('");
        ast::TypeSyntax* type = parseType();
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DefaultValueExpression,
            join(start, tokens_[index_ - 1].range()));
        node->setTypeSyntax(type);
        return node;
    }

    if (at(TokenKind::KwNew))
    {
        const SourceRange start = consume().range();

        // NS304-T09-F07: `new()` (target-typed) infiere el tipo del contexto objetivo. Se marca
        // en auxiliaryText y Sema lo resuelve contra el tipo objetivo activo.
        if (at(TokenKind::LeftParen) && peek().kind() == TokenKind::RightParen)
        {
            consume(); // '('
            consume(); // ')'
            ast::AstNode* creation = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ObjectCreationExpression, start);
            creation->setAuxiliaryText("target-typed");
            creation->setRange(join(start, tokens_[index_ - 1].range()));
            return creation;
        }

        // Item 317: placement construction follows the conventional unambiguous spelling
        // `new (storage) T(args)`. The placement expression is kept in first(), independently
        // from the ordered constructor arguments in children(), so item 318 can guarantee its
        // single evaluation without recovering structure from tokens.
        if (consumeIf(TokenKind::LeftParen))
        {
            // NS304-T09-F07: `new(args)` target-typed se desambigua de `new (storage) T(args)`
            // (placement): si tras el `)` que cierra viene un tipo, es placement.
            const bool placement = [&]()
            {
                std::size_t cursor = index_;
                int depth = 1;
                for (; cursor < tokens_.size(); ++cursor)
                {
                    const TokenKind kind = tokens_[cursor].kind();
                    if (kind == TokenKind::LeftParen) ++depth;
                    else if (kind == TokenKind::RightParen)
                    {
                        if (--depth == 0)
                        {
                            ++cursor;
                            break;
                        }
                    }
                }
                return cursor < tokens_.size() && canStartTypeAt(cursor);
            }();
            if (!placement)
            {
                ast::AstNode* creation = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::ObjectCreationExpression, start);
                creation->setAuxiliaryText("target-typed");
                if (!at(TokenKind::RightParen))
                {
                    while (true)
                    {
                        creation->addChild(parseArgument());
                        if (!consumeIf(TokenKind::Comma)) break;
                    }
                }
                expect(TokenKind::RightParen, "')' after target-typed new arguments");
                creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
                return creation;
            }
            ast::AstNode* storage = parseExpression();
            expect(TokenKind::RightParen, "')' after placement storage");
            ast::TypeSyntax* type = parseAllocationElementType();
            ast::AstNode* creation = arena_.make<ast::AstNode>(
                ast::AstNodeKind::PlacementConstructionExpression,
                start);
            creation->setFirst(storage);
            creation->setTypeSyntax(type);
            if (expect(TokenKind::LeftParen, "'(' after placement construction type"))
            {
                if (!at(TokenKind::RightParen))
                {
                    while (true)
                    {
                        creation->addChild(parseArgument());
                        if (!consumeIf(TokenKind::Comma)) break;
                    }
                }
                expect(TokenKind::RightParen, "')' after placement constructor arguments");
            }
            creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
            return creation;
        }

        ast::TypeSyntax* type = parseAllocationElementType();
        if (consumeIf(TokenKind::LeftParen))
        {
            ast::AstNode* creation = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ObjectCreationExpression,
                start);
            creation->setTypeSyntax(type);
            if (!at(TokenKind::RightParen))
            {
                while (true)
                {
                    creation->addChild(parseArgument());
                    if (!consumeIf(TokenKind::Comma))
                    {
                        break;
                    }
                }
            }
            expect(TokenKind::RightParen, "')'");
            creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
            return creation;
        }

        ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::NewExpression, start);
        node->setTypeSyntax(type);
        if (at(TokenKind::LeftBracket))
        {
            node->setFlag(true);
            std::vector<SourceRange> omittedDimensions;
            std::size_t countedDimensions = 0;
            while (consumeIf(TokenKind::LeftBracket))
            {
                const SourceRange left = tokens_[index_ - 1].range();
                if (at(TokenKind::RightBracket))
                {
                    const SourceRange right = consume().range();
                    omittedDimensions.push_back(join(left, right));
                    continue;
                }

                std::vector<ast::AstNode*> groupDimensions;
                groupDimensions.push_back(parseExpression());
                while (consumeIf(TokenKind::Comma)) groupDimensions.push_back(parseExpression());
                countedDimensions += groupDimensions.size();
                if (groupDimensions.size() > 1)
                {
                    if (node->first() != nullptr || !omittedDimensions.empty())
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::UnexpectedToken,
                            diagnostics::DiagnosticSeverity::Error,
                            groupDimensions.front()->range(),
                            "a rectangular allocation uses one counted bracket group");
                    }
                    node->setAuxiliaryText("range-array-allocation:" +
                        std::to_string(groupDimensions.size()));
                    node->setFirst(groupDimensions.front());
                    for (std::size_t dimension = 1; dimension < groupDimensions.size(); ++dimension)
                        node->addChild(groupDimensions[dimension]);
                }
                else if (node->first() == nullptr)
                    node->setFirst(groupDimensions.front());
                else
                    diagnostics_.report(
                        diagnostics::DiagnosticId::UnexpectedToken,
                        diagnostics::DiagnosticSeverity::Error,
                        groupDimensions.front()->range(),
                        "dynamic jagged allocation accepts exactly one counted dimension");
                expect(TokenKind::RightBracket, "']'");
            }

            // NSC-LIMIT-017. The final omitted outer dimension followed by an initializer is
            // `new T[] { ... }`; any earlier `[]` belongs to its ELEMENT type. In every other
            // allocation spelling, every `[]` is part of the element type, so both conventional
            // jagged forms are canonicalized identically:
            // `new T[count][]` and `new T[][count]` both allocate a `T[][]` whose element is T[].
            const bool inferredInitializer = countedDimensions == 0 &&
                !omittedDimensions.empty() && at(TokenKind::LeftBrace);
            if (inferredInitializer)
            {
                // The final omitted dimension is the inferred OUTER array; every preceding []
                // belongs to its element type. Thus `new T[][] { row }` is an inferred T[][]
                // whose elements are T[], rather than an allocation with two missing counts.
                for (std::size_t dimension = 1; dimension < omittedDimensions.size(); ++dimension)
                {
                    const SourceRange& range = omittedDimensions[dimension - 1];
                    type->addModifier(ast::TypeModifierSyntax{
                        ast::TypeModifierSyntaxKind::DynamicArray,
                        nullptr,
                        range});
                    type->setRange(join(type->range(), range));
                }
                consume();
                node->setAuxiliaryText("inferred-array-initializer");
                if (!at(TokenKind::RightBrace))
                {
                    while (true)
                    {
                        node->addChild(parseAssignmentExpression());
                        if (!consumeIf(TokenKind::Comma)) break;
                        if (at(TokenKind::RightBrace)) break;
                    }
                }
                expect(TokenKind::RightBrace, "'}'");
            }
            else
            {
                for (const SourceRange& range : omittedDimensions)
                {
                    type->addModifier(ast::TypeModifierSyntax{
                        ast::TypeModifierSyntaxKind::DynamicArray,
                        nullptr,
                        range});
                    type->setRange(join(type->range(), range));
                }
                if (countedDimensions == 0)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedExpression,
                        diagnostics::DiagnosticSeverity::Error,
                        node->range(),
                        "dynamic array allocation requires one count or an aggregate initializer");
                }
            }
        }
        node->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
        return node;
    }

    if (at(TokenKind::KwDelete))
    {
        const SourceRange start = consume().range();
        bool arrayForm = false;
        if (consumeIf(TokenKind::LeftBracket))
        {
            arrayForm = true;
            expect(TokenKind::RightBracket, "']'");
        }
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeleteExpression,
            join(start, operand->range()));
        node->setFlag(arrayForm);
        node->setFirst(operand);
        return node;
    }

    if (at(TokenKind::KwDestroy))
    {
        const SourceRange start = consume().range();
        ast::AstNode* operand = parseUnaryExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DestroyExpression,
            join(start, operand->range()));
        node->setFirst(operand);
        return node;
    }

    if (at(TokenKind::KwStackalloc))
    {
        const SourceRange start = consume().range();
        ast::TypeSyntax* type = parseAllocationElementType();
        expect(TokenKind::LeftBracket, "'['");
        ast::AstNode* count = parseExpression();
        expect(TokenKind::RightBracket, "']'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::StackallocExpression,
            join(start, tokens_[index_ - 1].range()));
        node->setTypeSyntax(type);
        node->setFirst(count);
        return node;
    }

    return parsePostfixExpression();
}

ast::AstNode* Parser::parseArgument()
{
    std::string argumentName;
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Colon)
    {
        argumentName = std::string(consume().lexeme());
        consume();
    }

    // A named by-reference argument is written `name: ref value`, as in C#.
    const bool refArgument = (atContextualKeyword("ref") || atContextualKeyword("out") ||
        at(TokenKind::KwIn)) && peek().kind() != TokenKind::Comma &&
        peek().kind() != TokenKind::RightParen;
    ast::AstNode* argument = nullptr;
    if (refArgument)
    {
        const lexer::Token& keyword = consume();
        const std::string spelling =
            keyword.kind() == TokenKind::KwIn ? "in" : std::string(keyword.lexeme());
        ast::AstNode* operand = parseAssignmentExpression();
        argument = arena_.make<ast::AstNode>(ast::AstNodeKind::UnaryExpression, keyword.range());
        argument->setTokenKind(TokenKind::Amp);
        argument->setFirst(operand);
        argument->setAuxiliaryText("pass:" + spelling);
        if (operand != nullptr) argument->setRange(join(keyword.range(), operand->range()));
    }
    else
    {
        argument = parseAssignmentExpression();
    }
    if (argument != nullptr && !argumentName.empty())
    {
        argument->setArgumentName(std::move(argumentName));
    }
    return argument;
}

ast::AstNode* Parser::parsePostfixExpression()
{
    // ID 131. `Caja<int>.Make(...)`: el receptor es un TIPO CERRADO, no una expresion. Se lee con
    // `parseType`, que es el MISMO camino que `new Caja<int>()` y que la declaracion de un local,
    // asi que la clave de tipo que salga de aqui es identica a la de esos dos por construccion --
    // no una segunda forma de escribir lo mismo que haya que mantener de acuerdo con la primera.
    //
    // El nodo sigue siendo un IdentifierExpression para que el resto del bucle de sufijos (`.`,
    // `[`, `(`) no tenga que saber nada nuevo; lo que lleva de mas es el TypeSyntax, que el
    // modelo de cuerpos usa para resolver el tipo cerrado en vez del nombre suelto.
    ast::AstNode* expression = nullptr;
    if (looksLikeClosedGenericTypeReference())
    {
        const SourceRange start = current().range();
        if (ast::TypeSyntax* closedType = parseType())
        {
            expression = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IdentifierExpression,
                join(start, tokens_[index_ - 1].range()));
            expression->setText(closedType->baseName());
            expression->setTypeSyntax(closedType);
        }
    }
    if (expression == nullptr)
    {
        expression = parsePrimaryExpression();
    }

    while (true)
    {
        std::vector<ast::TypeSyntax*> genericCallArguments;
        bool hasExplicitGenericArguments = false;
        if (looksLikeGenericCallTypeArguments())
        {
            hasExplicitGenericArguments = true;
            parseTypeArgumentList(genericCallArguments);
        }

        if (consumeIf(TokenKind::LeftParen))
        {
            ast::AstNode* call = arena_.make<ast::AstNode>(ast::AstNodeKind::CallExpression, expression->range());
            call->setFirst(expression);
            // Empty type packs make `F<>()` meaningful and different from inference in `F()`.
            // CallExpression does not otherwise use flag(), so preserve that syntactic fact
            // without manufacturing a sentinel TypeSyntax that could leak into ABI identity.
            call->setFlag(hasExplicitGenericArguments);
            for (ast::TypeSyntax* argument : genericCallArguments) call->addTypeSyntax(argument);
            if (!at(TokenKind::RightParen))
            {
                while (true)
                {
                    // C4: `f(ref x)` / `f(out x)` / `f(in x)`. The argument IS the address of the
                    // operand, so it is built as the address-of expression the callee's pointer
                    // parameter expects -- one shape, already certified -- and the keyword is
                    // recorded so the frontend can insist that the call site and the signature
                    // agree about it.
                    call->addChild(parseArgument());
                    if (!consumeIf(TokenKind::Comma))
                    {
                        break;
                    }
                }
            }
            expect(TokenKind::RightParen, "')'");
            call->setRange(SourceRange(expression->range().begin(), tokens_[index_ - 1].range().end()));
            // T07-F06 keeps the assertion family contextual: these spellings remain ordinary
            // identifiers everywhere except an unqualified call head. Preserve the selected
            // intrinsic on the call so later stages never infer it from a user member's name.
            if (expression->kind() == ast::AstNodeKind::IdentifierExpression &&
                expression->typeSyntax() == nullptr)
            {
                const std::string& intrinsic = expression->text();
                if (intrinsic == "assert" || intrinsic == "unreachable" ||
                    intrinsic == "assume" || intrinsic == "panic")
                {
                    call->setAuxiliaryText("builtin-" + intrinsic);
                }
            }
            expression = call;
            continue;
        }

        if (!genericCallArguments.empty())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "generic type arguments in an expression must be followed by a call in Stage 44");
            return expression;
        }

        if (consumeIf(TokenKind::LeftBracket))
        {
            std::vector<ast::AstNode*> indices;
            if (!at(TokenKind::RightBracket))
            {
                while (true)
                {
                    ast::AstNode* indexExpression = nullptr;
                    ast::AstNode* rangeStart = nullptr;
                    if (!at(TokenKind::DotDot)) rangeStart = parseAssignmentExpression();
                    if (at(TokenKind::DotDot))
                    {
                        const SourceRange rangeToken = consume().range();
                        ast::AstNode* rangeEnd = nullptr;
                        if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket))
                            rangeEnd = parseAssignmentExpression();
                        indexExpression = arena_.make<ast::AstNode>(
                            ast::AstNodeKind::RangeExpression,
                            SourceRange(
                                rangeStart != nullptr ? rangeStart->range().begin() : rangeToken.begin(),
                                rangeEnd != nullptr ? rangeEnd->range().end() : rangeToken.end()));
                        indexExpression->setTokenKind(TokenKind::DotDot);
                        indexExpression->setFirst(rangeStart);
                        indexExpression->setSecond(rangeEnd);
                    }
                    else
                    {
                        indexExpression = rangeStart;
                    }
                    if (indexExpression != nullptr) indices.push_back(indexExpression);
                    if (!consumeIf(TokenKind::Comma)) break;
                }
            }
            expect(TokenKind::RightBracket, "']'");
            ast::AstNode* indexNode = arena_.make<ast::AstNode>(
                ast::AstNodeKind::IndexExpression,
                SourceRange(expression->range().begin(), tokens_[index_ - 1].range().end()));
            indexNode->setFirst(expression);
            if (!indices.empty()) indexNode->setSecond(indices.front());
            for (ast::AstNode* indexExpression : indices) indexNode->addChild(indexExpression);
            expression = indexNode;
            continue;
        }

        if (at(TokenKind::QuestionBracket))
        {
            consume(); // '?['
            std::vector<ast::AstNode*> indices;
            if (!at(TokenKind::RightBracket))
            {
                while (true)
                {
                    ast::AstNode* indexExpression = nullptr;
                    ast::AstNode* rangeStart = nullptr;
                    if (!at(TokenKind::DotDot)) rangeStart = parseAssignmentExpression();
                    if (at(TokenKind::DotDot))
                    {
                        const SourceRange rangeToken = consume().range();
                        ast::AstNode* rangeEnd = nullptr;
                        if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket))
                            rangeEnd = parseAssignmentExpression();
                        indexExpression = arena_.make<ast::AstNode>(
                            ast::AstNodeKind::RangeExpression,
                            SourceRange(
                                rangeStart != nullptr ? rangeStart->range().begin() : rangeToken.begin(),
                                rangeEnd != nullptr ? rangeEnd->range().end() : rangeToken.end()));
                        indexExpression->setTokenKind(TokenKind::DotDot);
                        indexExpression->setFirst(rangeStart);
                        indexExpression->setSecond(rangeEnd);
                    }
                    else
                    {
                        indexExpression = rangeStart;
                    }
                    if (indexExpression != nullptr) indices.push_back(indexExpression);
                    if (!consumeIf(TokenKind::Comma)) break;
                }
            }
            expect(TokenKind::RightBracket, "']'");
            ast::AstNode* indexNode = arena_.make<ast::AstNode>(
                ast::AstNodeKind::NullConditionalIndexExpression,
                SourceRange(expression->range().begin(), tokens_[index_ - 1].range().end()));
            indexNode->setTokenKind(TokenKind::QuestionBracket);
            indexNode->setFirst(expression);
            if (!indices.empty()) indexNode->setSecond(indices.front());
            for (ast::AstNode* indexExpression : indices) indexNode->addChild(indexExpression);
            expression = indexNode;
            continue;
        }

        if (at(TokenKind::QuestionDot))
        {
            consume();
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "member name after '?.'");
                return expression;
            }
            const lexer::Token& member = consume();
            ast::AstNode* access = arena_.make<ast::AstNode>(
                ast::AstNodeKind::NullConditionalMemberExpression,
                SourceRange(expression->range().begin(), member.range().end()));
            access->setTokenKind(TokenKind::QuestionDot);
            access->setFirst(expression);
            access->setText(std::string(member.lexeme()));
            expression = access;
            continue;
        }

        if (at(TokenKind::Dot) || at(TokenKind::Arrow))
        {
            const TokenKind operation = consume().kind();
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "member name");
                return expression;
            }
            const lexer::Token& member = consume();
            const SourceRange access(expression->range().begin(), member.range().end());

            // IDs 80 y 83. `p->campo` ES `(*p).campo`: la misma direccion, el mismo acceso, el
            // mismo tipo. Antes se construia un PointerMemberExpression propio que el modelo
            // semantico rechazaba para llamadas y el HIR para campos -- dos negativas para una
            // forma que el lenguaje ya sabia bajar escrita de la otra manera.
            //
            // POR QUE AQUI Y NO EN EL HIR. Bajarlo abajo habria significado una segunda ruta de
            // acceso a miembro, paralela a la de `.`, que hay que mantener de acuerdo con ella en
            // sobrecarga, propiedades, indexadores, metodos virtuales y estructuras. Convertirlo
            // en el parser deja UNA sola ruta: todo lo que `(*p).x` sepa hacer hoy y manana,
            // `p->x` lo sabe automaticamente, y no hay forma de que las dos se separen.
            //
            // El rango del nodo es el del `p->campo` ORIGINAL, no el del `*p` sintetizado, para
            // que un diagnostico senale lo que la persona escribio.
            ast::AstNode* receiver = expression;
            if (operation == TokenKind::Arrow)
            {
                ast::AstNode* dereference = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::UnaryExpression, access);
                dereference->setTokenKind(TokenKind::Star);
                dereference->setFirst(expression);
                // Marca el `*` como SINTETIZADO. El modelo semantico la lee para que, si el lado
                // izquierdo no es un puntero, el diagnostico nombre `->` -- que es lo que la
                // persona escribio -- y no un `*` que nunca tecleo.
                dereference->setAuxiliaryText("arrow");
                receiver = dereference;
            }

            ast::AstNode* memberNode = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MemberExpression, access);
            memberNode->setFirst(receiver);
            memberNode->setText(std::string(member.lexeme()));
            expression = memberNode;
            continue;
        }

        if (at(TokenKind::PlusPlus) || at(TokenKind::MinusMinus))
        {
            const lexer::Token& operation = consume();
            ast::AstNode* postfix = arena_.make<ast::AstNode>(
                ast::AstNodeKind::PostfixExpression,
                SourceRange(expression->range().begin(), operation.range().end()));
            postfix->setTokenKind(operation.kind());
            postfix->setFirst(expression);
            expression = postfix;
            continue;
        }

        if (at(TokenKind::Question) && !questionBeginsConditional())
        {
            const lexer::Token& operation = consume();
            ast::AstNode* propagation = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ResultPropagationExpression,
                SourceRange(expression->range().begin(), operation.range().end()));
            propagation->setTokenKind(TokenKind::Question);
            propagation->setFirst(expression);
            expression = propagation;
            continue;
        }

        if (atContextualKeyword("with"))
        {
            expression = parseWithExpression(expression);
            continue;
        }

        break;
    }

    return expression;
}

bool Parser::questionBeginsConditional() const noexcept
{
    if (!at(TokenKind::Question)) return false;

    // `?` is both a postfix Result operator and the opener of `condition ? a : b`.  Looking only
    // at the following token is insufficient (`result? + 1` and `condition ? +1 : 0` share it),
    // so find the colon owned by this exact nesting level. Nested ternaries consume their own
    // colons; delimiters end the current expression. This keeps the existing conditional grammar
    // intact while allowing propagation to participate in every higher-precedence expression.
    std::size_t parentheses = 0;
    std::size_t brackets = 0;
    std::size_t braces = 0;
    std::size_t nestedQuestions = 0;
    for (std::size_t cursor = index_ + 1U; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        switch (kind)
        {
            case TokenKind::LeftParen: ++parentheses; break;
            case TokenKind::LeftBracket: ++brackets; break;
            case TokenKind::LeftBrace: ++braces; break;
            case TokenKind::RightParen:
                if (parentheses != 0) { --parentheses; break; }
                if (brackets == 0 && braces == 0) return false;
                break;
            case TokenKind::RightBracket:
                if (brackets != 0) { --brackets; break; }
                if (parentheses == 0 && braces == 0) return false;
                break;
            case TokenKind::RightBrace:
                if (braces != 0) { --braces; break; }
                if (parentheses == 0 && brackets == 0) return false;
                break;
            case TokenKind::Question:
                if (parentheses == 0 && brackets == 0 && braces == 0) ++nestedQuestions;
                break;
            case TokenKind::Colon:
                if (parentheses == 0 && brackets == 0 && braces == 0)
                {
                    if (nestedQuestions == 0) return true;
                    --nestedQuestions;
                }
                break;
            case TokenKind::Comma:
            case TokenKind::Semicolon:
            case TokenKind::EndOfFile:
                if (parentheses == 0 && brackets == 0 && braces == 0) return false;
                break;
            default: break;
        }
    }
    return false;
}

ast::AstNode* Parser::parseWithExpression(ast::AstNode* source)
{
    const SourceRange keyword = consume().range();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::WithExpression, join(source->range(), keyword));
    node->setFirst(source);
    if (!expect(TokenKind::LeftBrace, "'{' after with")) return node;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "member name in with expression");
            break;
        }
        const std::string name(current().lexeme());
        consume();
        expect(TokenKind::Equal, "'=' after with member name");
        ast::AstNode* value = parseAssignmentExpression();
        if (value != nullptr)
        {
            value->setArgumentName(name);
            node->addChild(value);
        }
        if (!consumeIf(TokenKind::Comma)) break;
        if (at(TokenKind::RightBrace)) break;
    }
    expect(TokenKind::RightBrace, "'}' after with expression");
    node->setRange(SourceRange(source->range().begin(), tokens_[index_ - 1].range().end()));
    return node;
}

ast::AstNode* Parser::parseNameOfExpression()
{
    // NS304-T10-F07: `nameof(x)` es una constante de cadena de tiempo de compilación igual al
    // nombre del último identificador/miembro de `x`. Se baja a un StringLiteralExpression en el
    // parser, de modo que reutiliza toda la maquinaria existente de literales de cadena.
    const SourceRange begin = current().range();
    consume(); // `nameof`
    expect(TokenKind::LeftParen, "'(' after 'nameof'");

    std::string name;
    // `nameof(Generic<int>)` / `nameof(Space.Type<T>)`: the argument is a closed generic type
    // reference. parseExpression would misread `Generic < int` (the `<` reads as a comparison),
    // so resolve the type here and take its base name.
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Less)
    {
        if (ast::TypeSyntax* type = parseType())
        {
            name = type->baseName();
        }
    }
    else
    {
        ast::AstNode* argument = parseExpression();
        const ast::AstNode* cursor = argument;
        while (cursor != nullptr && name.empty())
        {
            switch (cursor->kind())
            {
                case ast::AstNodeKind::IdentifierExpression:
                    name = cursor->text();
                    break;
                case ast::AstNodeKind::MemberExpression:
                case ast::AstNodeKind::PointerMemberExpression:
                case ast::AstNodeKind::NullConditionalMemberExpression:
                    name = cursor->text();
                    break;
                case ast::AstNodeKind::CallExpression:
                case ast::AstNodeKind::IndexExpression:
                    cursor = cursor->first();
                    continue;
                default:
                    break;
            }
        }
    }

    const SourceRange end = current().range();
    expect(TokenKind::RightParen, "')' after 'nameof' argument");

    ast::AstNode* literal = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StringLiteralExpression, join(begin, end));
    literal->setText(std::move(name));
    return literal;
}

ast::AstNode* Parser::parsePrimaryExpression()
{
    if (startsParenthesizedBlock("match"))
        return parseMatchExpression();
    if (atContextualKeyword("loop") &&
        (peek().kind() == TokenKind::LeftBrace ||
         (peek().kind() == TokenKind::Identifier && peek(2).kind() == TokenKind::LeftBrace)))
        return parseLoopExpression();
    if (atContextualKeyword("comprehend") && peek().kind() == TokenKind::LeftParen)
    {
        std::size_t cursor = index_ + 1U;
        std::size_t depth = 0;
        for (; cursor < tokens_.size(); ++cursor)
        {
            if (tokens_[cursor].kind() == TokenKind::LeftParen) ++depth;
            else if (tokens_[cursor].kind() == TokenKind::RightParen)
            {
                if (--depth == 0) break;
            }
        }
        if (cursor + 1U < tokens_.size() && tokens_[cursor + 1U].kind() == TokenKind::LeftBrace)
            return parseComprehensionExpression();
    }
    if (atContextualKeyword("nameof") && peek().kind() == TokenKind::LeftParen)
        return parseNameOfExpression();
    const lexer::Token& token = current();
    ast::AstNodeKind kind = ast::AstNodeKind::IdentifierExpression;

    switch (token.kind())
    {
        case TokenKind::Identifier: kind = ast::AstNodeKind::IdentifierExpression; break;
        case TokenKind::IntegerLiteral: kind = ast::AstNodeKind::IntegerLiteralExpression; break;
        case TokenKind::FloatingLiteral: kind = ast::AstNodeKind::FloatingLiteralExpression; break;
        case TokenKind::CharacterLiteral: kind = ast::AstNodeKind::CharacterLiteralExpression; break;
        case TokenKind::StringLiteral:
            kind = ast::AstNodeKind::StringLiteralExpression;
            break;
        case TokenKind::InterpolatedStringLiteral:
            kind = ast::AstNodeKind::InterpolatedStringExpression;
            break;
        case TokenKind::KwTrue:
        case TokenKind::KwFalse: kind = ast::AstNodeKind::BooleanLiteralExpression; break;
        case TokenKind::KwNull: kind = ast::AstNodeKind::NullLiteralExpression; break;
        case TokenKind::KwThis:
            kind = parsingExtensionBody_
                ? ast::AstNodeKind::IdentifierExpression
                : ast::AstNodeKind::ThisExpression;
            break;
        case TokenKind::KwBase: kind = ast::AstNodeKind::BaseExpression; break;
        case TokenKind::LeftParen:
        {
            // Collapse a pure run of grouping parentheses before entering the full expression
            // precedence ladder. The recursive spelling previously multiplied every source
            // parenthesis by the complete ladder depth and exhausted the Windows debug stack at
            // otherwise practical nesting. A top-level comma disables this path so tuple parsing
            // keeps the existing node construction and element ordering.
            std::size_t groupingDepth = 1;
            while (index_ + groupingDepth < tokens_.size() &&
                tokens_[index_ + groupingDepth].kind() == TokenKind::LeftParen)
                ++groupingDepth;
            bool pureGroupingRun = groupingDepth > 1 &&
                index_ + groupingDepth < tokens_.size() &&
                tokens_[index_ + groupingDepth].kind() != TokenKind::RightParen;
            if (pureGroupingRun)
            {
                std::size_t nestedParentheses = 0;
                std::size_t nestedBrackets = 0;
                std::size_t nestedBraces = 0;
                std::size_t closingIndex = tokens_.size();
                for (std::size_t cursor = index_ + groupingDepth;
                     cursor < tokens_.size(); ++cursor)
                {
                    const TokenKind kind = tokens_[cursor].kind();
                    if (kind == TokenKind::LeftParen) ++nestedParentheses;
                    else if (kind == TokenKind::RightParen)
                    {
                        if (nestedParentheses != 0)
                        {
                            --nestedParentheses;
                        }
                        else
                        {
                            closingIndex = cursor;
                            break;
                        }
                    }
                    else if (kind == TokenKind::LeftBracket) ++nestedBrackets;
                    else if (kind == TokenKind::RightBracket && nestedBrackets != 0)
                        --nestedBrackets;
                    else if (kind == TokenKind::LeftBrace) ++nestedBraces;
                    else if (kind == TokenKind::RightBrace && nestedBraces != 0)
                        --nestedBraces;
                    else if (kind == TokenKind::Comma && nestedParentheses == 0 &&
                        nestedBrackets == 0 && nestedBraces == 0)
                    {
                        pureGroupingRun = false;
                        break;
                    }
                }
                if (closingIndex == tokens_.size()) pureGroupingRun = false;
                for (std::size_t offset = 1; pureGroupingRun && offset < groupingDepth; ++offset)
                {
                    pureGroupingRun = closingIndex + offset < tokens_.size() &&
                        tokens_[closingIndex + offset].kind() == TokenKind::RightParen;
                }
            }
            if (pureGroupingRun)
            {
                const SourceRange outerOpen = current().range();
                for (std::size_t depth = 0; depth < groupingDepth; ++depth) consume();
                ast::AstNode* expression = parseArgument();
                SourceRange outerClose = current().range();
                for (std::size_t depth = 0; depth < groupingDepth; ++depth)
                {
                    outerClose = current().range();
                    expect(TokenKind::RightParen, "')'");
                }
                if (expression != nullptr)
                    expression->setRange(SourceRange(outerOpen.begin(), outerClose.end()));
                return expression;
            }

            const SourceRange open = consume().range();
            if (at(TokenKind::RightParen))
            {
                const SourceRange close = consume().range();
                return arena_.make<ast::AstNode>(
                    ast::AstNodeKind::TupleExpression, join(open, close));
            }
            ast::AstNode* expression = parseArgument();
            if (consumeIf(TokenKind::Comma))
            {
                ast::AstNode* tuple = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::TupleExpression, open);
                tuple->addChild(expression);
                while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
                {
                    ast::AstNode* element = nullptr;
                    if (at(TokenKind::DotDot))
                    {
                        const SourceRange spread = consume().range();
                        ast::AstNode* value = parseAssignmentExpression();
                        element = arena_.make<ast::AstNode>(
                            ast::AstNodeKind::SpreadElementExpression,
                            join(spread, value != nullptr ? value->range() : spread));
                        element->setTokenKind(TokenKind::DotDot);
                        element->setFirst(value);
                    }
                    else
                    {
                        element = parseArgument();
                    }
                    if (element != nullptr) tuple->addChild(element);
                    if (!consumeIf(TokenKind::Comma)) break;
                }
                expect(TokenKind::RightParen, "')' after tuple");
                tuple->setRange(SourceRange(open.begin(), tokens_[index_ - 1].range().end()));
                return tuple;
            }
            expect(TokenKind::RightParen, "')'");
            if (expression != nullptr)
            {
                expression->setRange(SourceRange(token.range().begin(), tokens_[index_ - 1].range().end()));
            }
            return expression;
        }
        default:
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error,
                token.range(),
                "expected expression");
            if (!at(TokenKind::EndOfFile))
            {
                consume();
            }
            return makeErrorExpression();
    }

    consume();
    SourceRange nodeRange = token.range();
    std::string nodeText(token.lexeme());
    if (token.kind() == TokenKind::KwThis && parsingExtensionBody_)
        nodeText = "__nsc_extension_this";
    // Non-generic static receivers use the ordinary postfix member chain.  Preserve the module
    // prefix in its first identifier so ProjectNameLookup sees one canonical spelling while the
    // following dots remain normal MemberExpression nodes.
    if (kind == ast::AstNodeKind::IdentifierExpression &&
        at(TokenKind::Colon) && peek().kind() == TokenKind::Colon)
    {
        consume();
        consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "type name after module qualifier '::'");
            return makeErrorExpression();
        }
        const lexer::Token& qualified = consume();
        nodeText += "::";
        nodeText += qualified.lexeme();
        nodeRange = join(token.range(), qualified.range());
    }
    ast::AstNode* node = arena_.make<ast::AstNode>(kind, nodeRange);
    node->setTokenKind(token.kind());
    node->setText(std::move(nodeText));

    if (const auto* integer = std::get_if<std::uint64_t>(&token.value()))
    {
        node->setIntegerValue(*integer);
    }
    if (const auto* floating = std::get_if<double>(&token.value()))
    {
        node->setFloatingValue(*floating);
    }
    if (const auto* character = std::get_if<std::uint8_t>(&token.value()))
    {
        node->setIntegerValue(*character);
    }
    if (const auto* string = std::get_if<std::string>(&token.value()))
    {
        node->setText(*string);
        if (kind == ast::AstNodeKind::InterpolatedStringExpression)
        {
            std::vector<language::InterpolationPart> parts;
            std::string error;
            if (!language::parseStaticInterpolation(*string, parts, error))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidStringLiteral,
                    diagnostics::DiagnosticSeverity::Error,
                    token.range(),
                    std::move(error));
                node->setIntegerValue(0);
                return node;
            }
            node->setIntegerValue(parts.size());

            const bool fullyStatic = std::ranges::all_of(parts, isStaticInterpolationHole);
            if (fullyStatic) return node;

            // Runtime interpolation is syntax sugar over the existing NativeSharp.Core.String
            // formatting surface and ordinary string concatenation. Each hole is parsed as a
            // real expression exactly once, and the left-associated concat tree freezes textual
            // evaluation order. Starting with an empty literal is intentional: even a single
            // runtime hole reaches StringConcat, so `noheap` observes the allocation instead of
            // accidentally accepting a formatting call whose transitive body allocates.
            std::vector<std::string> knownTypes;
            knownTypes.reserve(knownTypeNames_.size());
            for (const std::string& name : knownTypeNames_) knownTypes.push_back(name);

            const auto makeStringLiteral = [&](std::string value) -> ast::AstNode*
            {
                ast::AstNode* literal = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::StringLiteralExpression, token.range());
                literal->setTokenKind(TokenKind::StringLiteral);
                literal->setText(std::move(value));
                return literal;
            };
            const auto makeBooleanLiteral = [&](bool value) -> ast::AstNode*
            {
                ast::AstNode* literal = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::BooleanLiteralExpression, token.range());
                literal->setTokenKind(value ? TokenKind::KwTrue : TokenKind::KwFalse);
                literal->setIntegerValue(value ? 1U : 0U);
                return literal;
            };
            const auto makeStringCall = [&](std::string method,
                                            std::vector<ast::AstNode*> arguments) -> ast::AstNode*
            {
                ast::AstNode* receiver = makeIdentifierExpression(token.range(), "String");
                ast::AstNode* member = makeMemberExpression(receiver, std::move(method));
                ast::AstNode* call = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::CallExpression, token.range());
                call->setFirst(member);
                call->setAuxiliaryText("runtime-interpolation");
                for (ast::AstNode* argument : arguments) call->addChild(argument);
                return call;
            };
            const auto parseWidth = [](std::string_view format) -> std::optional<std::uint64_t>
            {
                std::uint64_t value = 0;
                for (std::size_t index = 1; index < format.size(); ++index)
                {
                    const unsigned char byte = static_cast<unsigned char>(format[index]);
                    if (!std::isdigit(byte)) return std::nullopt;
                    value = value * 10U + static_cast<std::uint64_t>(byte - '0');
                    if (value > 64U) return std::nullopt;
                }
                return value;
            };

            ast::AstNode* result = makeStringLiteral("");
            for (const language::InterpolationPart& part : parts)
            {
                ast::AstNode* piece = nullptr;
                if (part.kind == language::InterpolationPartKind::Literal)
                {
                    if (part.text.empty()) continue;
                    piece = makeStringLiteral(part.text);
                }
                else
                {
                    const std::uint32_t tokenOffset = token.range().begin().offset();
                    const std::uint64_t requestedOffset =
                        static_cast<std::uint64_t>(tokenOffset) + 2U + part.expressionOffset;
                    const std::size_t prefix = requestedOffset <=
                            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
                        ? static_cast<std::size_t>(requestedOffset)
                        : static_cast<std::size_t>(tokenOffset);
                    std::string fragment(prefix, ' ');
                    fragment += part.text;
                    source::SourceFile fragmentSource(
                        token.range().begin().fileId(), "<runtime-interpolation>", std::move(fragment));
                    lexer::Lexer fragmentLexer(fragmentSource, diagnostics_);
                    const std::vector<lexer::Token> fragmentTokens = fragmentLexer.lexAll();
                    Parser fragmentParser(fragmentTokens, arena_, diagnostics_, knownTypes);
                    ast::AstNode* hole = fragmentParser.parseExpression();
                    if (!fragmentParser.at(TokenKind::EndOfFile))
                    {
                        diagnostics_.report(
                            diagnostics::DiagnosticId::ExpectedExpression,
                            diagnostics::DiagnosticSeverity::Error,
                            token.range(),
                            "interpolation hole must contain exactly one expression");
                    }
                    if (hole == nullptr) hole = makeErrorExpression();

                    if (part.format.empty())
                    {
                        piece = makeStringCall("ToString", {hole});
                    }
                    else
                    {
                        const char mode = part.format.front();
                        const std::optional<std::uint64_t> width = parseWidth(part.format);
                        if (!width.has_value() ||
                            (mode != 'D' && mode != 'd' && mode != 'X' && mode != 'x' &&
                             mode != 'F' && mode != 'f' && mode != 'G' && mode != 'g') ||
                            ((mode == 'G' || mode == 'g') && part.format.size() != 1))
                        {
                            diagnostics_.report(
                                diagnostics::DiagnosticId::InvalidStringLiteral,
                                diagnostics::DiagnosticSeverity::Error,
                                token.range(),
                                "runtime interpolation format must be D or X with optional width, F with optional precision, or G without precision; numeric arguments are limited to 64");
                            piece = makeStringCall("ToString", {hole});
                        }
                        else if (mode == 'F' || mode == 'f')
                        {
                            const std::uint64_t precision = part.format.size() == 1 ? 2U : *width;
                            piece = makeStringCall("ToStringFixed", {
                                hole, makeIntegerLiteralExpression(token.range(), precision)});
                        }
                        else if (mode == 'G' || mode == 'g')
                        {
                            piece = makeStringCall("ToString", {hole});
                        }
                        else if (mode == 'X' || mode == 'x')
                        {
                            piece = makeStringCall("ToHexString", {
                                hole,
                                makeIntegerLiteralExpression(token.range(), *width),
                                makeBooleanLiteral(mode == 'X')});
                        }
                        else
                        {
                            piece = makeStringCall("ToString", {hole});
                            if (*width != 0)
                            {
                                piece = makeStringCall("ZFill", {
                                    piece,
                                    makeIntegerLiteralExpression(token.range(), *width)});
                            }
                        }
                    }
                }
                result = makeBinaryExpression(TokenKind::Plus, result, piece);
                result->setRange(token.range());
            }
            return result;
        }
    }

    return node;
}

ast::AstNode* Parser::parseBinaryLevel(
    ast::AstNode* (Parser::*nextLevel)(),
    std::span<const TokenKind> operators,
    ast::AstNodeKind nodeKind)
{
    ast::AstNode* left = (this->*nextLevel)();
    while (contains(operators, current().kind()))
    {
        const TokenKind operation = consume().kind();
        ast::AstNode* right = (this->*nextLevel)();
        ast::AstNode* node = arena_.make<ast::AstNode>(nodeKind, join(left->range(), right->range()));
        node->setTokenKind(operation);
        node->setFirst(left);
        node->setSecond(right);
        left = node;
    }
    return left;
}

ast::AstNode* Parser::makeErrorExpression()
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, current().range());
    node->setText("<error>");
    return node;
}

void Parser::synchronizeGlobal()
{
    while (!at(TokenKind::EndOfFile))
    {
        if (consumeIf(TokenKind::Semicolon))
        {
            return;
        }
        if (at(TokenKind::RightBrace) || at(TokenKind::KwStruct) || at(TokenKind::KwEnum) ||
            at(TokenKind::KwNamespace) || at(TokenKind::KwUsing) || at(TokenKind::KwExtern) ||
            at(TokenKind::KwPublic) || at(TokenKind::KwInternal) || at(TokenKind::KwConst) ||
            canStartTypeAt(index_))
        {
            return;
        }
        consume();
    }
}

void Parser::synchronizeStatement()
{
    while (!at(TokenKind::EndOfFile) && !at(TokenKind::RightBrace))
    {
        if (consumeIf(TokenKind::Semicolon))
        {
            return;
        }
        consume();
    }
}
}
