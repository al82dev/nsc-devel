#include "nativesharp/Parser/Parser.h"
#include "KnownGenericTypeScope.h"

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

ast::AstNode* Parser::cloneDeclarationNode(const ast::AstNode* node)
{
    return cloneParsedNode(arena_, node);
}

ast::TypeSyntax* Parser::cloneDeclarationType(const ast::TypeSyntax* type)
{
    return cloneParsedType(arena_, type);
}

bool Parser::isAccessibilityModifier(TokenKind kind) const noexcept
{
    return isAccessibilityToken(kind);
}

bool Parser::isInvalidMemberObjectType(const ast::TypeSyntax* type) const noexcept
{
    return isInvalidVoidObjectType(type);
}

bool Parser::isMemberOperatorToken(TokenKind kind) const noexcept
{
    return isStage38OperatorToken(kind);
}

std::string_view Parser::memberOperatorSpelling(TokenKind kind) const noexcept
{
    return operatorSpelling(kind);
}

std::string Parser::memberOperatorMethodName(TokenKind kind, std::size_t arity) const
{
    return operatorMethodName(kind, arity);
}

void Parser::applyMemberFloatingPointMode(
    ast::AstNode* node,
    ast::FloatingPointModeSyntaxKind mode)
{
    applyFloatingPointModeToTree(node, mode);
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
    // NS304-T04-F05: `static` local function — el modificador no participa en el escaneo del tipo.
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::KwStatic) ++cursor;
    // NS304-T04-F06: un identificador suelto (posible parámetro genérico) es un tipo de retorno
    // plausible; scanTypeAt rechaza nombres desconocidos, así que se acepta `T name (...)`.
    if (!scanTypeAt(cursor, afterType))
    {
        if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Identifier)
            return false;
        afterType = cursor + 1;
    }
    cursor = afterType;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Identifier) return false;
    ++cursor;
    // NS304-T04-F06: salta la lista de parámetros genéricos `<T, U>` tras el nombre.
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Less)
    {
        int depth = 1;
        ++cursor;
        for (; cursor < tokens_.size(); ++cursor)
        {
            if (tokens_[cursor].kind() == TokenKind::Less) ++depth;
            else if (tokens_[cursor].kind() == TokenKind::Greater)
            {
                if (--depth == 0)
                {
                    ++cursor;
                    break;
                }
            }
        }
    }
    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::LeftParen;
}

ast::AstNode* Parser::parseLocalFunctionAsLambda()
{
    // NS304-T04-F06: baja `int name(params) { body }` a `var name = (params) => body;` para
    // reutilizar la inferencia de tipo callable natural de lambda (T04-F07). La recursion
    // (autoreferencia) es follow-up.
    const std::size_t beginIndex = index_;
    // NS304-T04-F05: `static` local function — sin captura; el marcador viaja al lambda.
    const bool isStaticLocal = at(TokenKind::KwStatic);
    if (isStaticLocal) consume();
    ast::AstNode* function = parseFunctionOrGlobalVariable();
    if (function == nullptr || function->kind() != ast::AstNodeKind::FunctionDeclaration)
    {
        return function;
    }
    ast::AstNode* lambda = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LambdaExpression, function->range());
    if (isStaticLocal) lambda->addModifier(TokenKind::KwStatic);
    // NS304-T04-F06: conserva los parámetros genéricos de la función local en el lambda.
    for (const ast::GenericParameterSyntax& generic : function->genericParameterSyntaxes())
    {
        lambda->addGenericParameter(generic);
    }
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

ast::TypeSyntax* Parser::parseType()
{
    return parseTypeImpl();
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
            // NS304-T11-F05: `ref-like` is a three-token contextual keyword (`ref` `-` `like`).
            const bool isRefLike = atContextualKeyword("ref") &&
                peek().kind() == TokenKind::Minus &&
                peek(2).kind() == TokenKind::Identifier && peek(2).lexeme() == "like";
            if (isKeywordClass || isKeywordStruct || isKeywordNew || isCopy || isMove ||
                isLifetime || isUnmanaged || isNotNull || isRefLike)
            {
                const lexer::Token& keyword = consume();
                if (isRefLike)
                {
                    consume(); // '-'
                    consume(); // 'like'
                }
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
                else if (isNotNull) { flag = &clause.requiresNotNull; spelling = "notnull"; }
                else { flag = &clause.requiresRefLike; spelling = "ref-like"; }
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
    else if (isAbstract || isSealed || (isStatic && !parsingExtensionBody_))
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
        // NS304-T08-F03: `static` dentro de un bloque extension marca una función de namespace
        // estática (sin receptor implícito); fuera de extensiones sigue rechazado arriba.
        if (declaration != nullptr && isStatic && parsingExtensionBody_)
        {
            declaration->addModifier(TokenKind::KwStatic);
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
        (declaration->kind() == ast::AstNodeKind::FunctionDeclaration ||
         declaration->kind() == ast::AstNodeKind::MethodDeclaration))
    {
        std::string marker = declaration->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "async";
        declaration->setAuxiliaryText(std::move(marker));
    }
    return declaration;
}

ast::AstNode* Parser::parseInterfaceMember(bool isTrait)
{
    return parseInterfaceMemberImpl(isTrait);
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
    return parseNestedTypeDeclarationImpl(modifiers, beginIndex);
}

ast::AstNode* Parser::parseClassMember(
    std::string_view className,
    bool allowUnresolvedNamedTypes,
    bool structContext)
{
    return parseClassMemberImpl(className, allowUnresolvedNamedTypes, structContext);
}

void Parser::appendParsedStatement(ast::AstNode*& destination, ast::AstNode* statement)
{
    if (statement == nullptr) return;
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
        return;
    }
    if (statement->kind() == ast::AstNodeKind::BlockStatement &&
        statement->auxiliaryText() == "conditional-pattern-inline")
    {
        for (ast::AstNode* child : statement->children()) destination->addChild(child);
        return;
    }
    destination->addChild(statement);
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
        appendParsedStatement(destination, statement);
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

    if (atContextualKeyword("yield"))
    {
        // NS304-T05-F09: a bare `yield;` in an async body is an explicit suspension point. The
        // poll suspends once and continues on the next poll (resume). No produced value.
        const std::size_t beginIndex = index_;
        consume(); // yield
        ast::AstNode* statement = arena_.make<ast::AstNode>(
            ast::AstNodeKind::SuspendStatement, tokenRangeFrom(beginIndex));
        expect(TokenKind::Semicolon, "';' after 'yield'");
        return statement;
    }

    if (at(TokenKind::KwStatic))
    {
        // NS304-T04-F05: `static int name(...) { ... }` es una función local estática.
        if (looksLikeLocalFunction()) return parseLocalFunctionAsLambda();
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
    if (atContextualKeyword("await"))
    {
        // NS304-T07-F06: `await future;` (un Future<void>) es una sentencia de expresion -- un
        // poll bloqueante del future -- y no una declaracion de variable con un tipo `await`.
        const std::size_t beginIndex = index_;
        ast::AstNode* expression = parseExpression();
        ast::AstNode* statement = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ExpressionStatement,
            expression != nullptr ? expression->range() : current().range());
        statement->setFirst(expression);
        expect(TokenKind::Semicolon, "';' after await expression");
        statement->setRange(tokenRangeFrom(beginIndex));
        return statement;
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
    // NS304-T13-F03: `scope Tipo x = ...` — contextual frame-storage modifier (no reserved keyword).
    const bool isScope = atContextualKeyword("scope") &&
        (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar);
    if (isScope) consume();
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
    if (isScope) variable->setAuxiliaryText("local:scope");
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
    enum class WorkKind
    {
        Evaluate,
        ContinueIf
    };
    struct Work final
    {
        WorkKind kind = WorkKind::Evaluate;
        const ast::AstNode* node = nullptr;
    };

    std::vector<Work> work{{WorkKind::Evaluate, statement}};
    std::vector<bool> values;
    while (!work.empty())
    {
        const Work current = work.back();
        work.pop_back();
        if (current.kind == WorkKind::ContinueIf)
        {
            const bool thenDiverges = values.back();
            values.pop_back();
            if (!thenDiverges)
            {
                values.push_back(false);
                continue;
            }
            work.push_back({WorkKind::Evaluate, current.node->third()});
            continue;
        }

        if (current.node == nullptr)
        {
            values.push_back(false);
            continue;
        }
        switch (current.node->kind())
        {
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ThrowStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
            values.push_back(true);
            break;
        case ast::AstNodeKind::BlockStatement:
            if (current.node->children().empty())
                values.push_back(false);
            else
                work.push_back({WorkKind::Evaluate, current.node->children().back()});
            break;
        case ast::AstNodeKind::IfStatement:
            if (current.node->second() == nullptr || current.node->third() == nullptr)
            {
                values.push_back(false);
                break;
            }
            work.push_back({WorkKind::ContinueIf, current.node});
            work.push_back({WorkKind::Evaluate, current.node->second()});
            break;
        default:
            values.push_back(false);
            break;
        }
    }
    return !values.empty() && values.back();
}

ast::AstNode* Parser::parseConditionalPatternStatement()
{
    return parseStructuredControlStatement();
}

void Parser::parseIfHeader(IfParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    if (looksLikeConditionDeclaration())
    {
        state.conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            state.conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            state.condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerIfStatement(const IfParseState& state, ast::AstNode* elseNode)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, tokenRangeFrom(state.beginIndex));
    node->setFirst(state.condition);
    node->setSecond(state.thenBlock);
    node->setThird(elseNode);
    if (state.conditionDeclaration == nullptr) return node;

    ast::AstNode* lowered = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement,
        tokenRangeFrom(state.beginIndex));
    lowered->setAuxiliaryText("condition-declaration");
    lowered->addChild(state.conditionDeclaration);
    lowered->addChild(node);
    return lowered;
}

ast::AstNode* Parser::parseIfStatement()
{
    return parseStructuredControlStatement();
}

class Parser::StructuredControlParser final
{
public:
    explicit StructuredControlParser(Parser& parser) noexcept : parser_(parser) {}

    ast::AstNode* parse()
    {
        work_.push_back({WorkKind::ParseControl, 0});
        execute();
        return values_.empty() ? nullptr : values_.back();
    }

    ast::AstNode* parseCatchClause()
    {
        work_.push_back({WorkKind::ParseStandaloneCatch, 0});
        execute();
        return values_.empty() ? nullptr : values_.back();
    }

private:
    void execute()
    {
        while (!work_.empty())
        {
            const Work current = work_.back();
            work_.pop_back();
            dispatch(current);
        }
    }

    enum class WorkKind
    {
        ParseControl,
        ParseIf,
        ParseWhile,
        ParseDoWhile,
        ParseFor,
        ParseTry,
        ParseStandaloneCatch,
        ParseUsing,
        ParseDefer,
        ParseForeach,
        ParseConditionalPattern,
        ParseSwitch,
        ParseMatch,
        ParseEmbedded,
        WrapEmbedded,
        ParseBlock,
        ContinueBlock,
        AppendBlock,
        FinishThen,
        FinishElse,
        FinishWhile,
        FinishDoWhile,
        FinishFor,
        ContinueTry,
        FinishTryBlock,
        FinishCatchBlock,
        FinishFinallyBlock,
        FinishStandaloneCatch,
        FinishUsing,
        FinishDefer,
        FinishForeach,
        FinishConditionalPatternSuccess,
        FinishConditionalPatternFailure,
        FinishConditionalPatternWhile,
        FinishConditionalPatternLetElse,
        ContinueSwitch,
        ContinueSwitchSection,
        AppendSwitchStatement,
        ContinueMatch,
        FinishMatchArm
    };
    struct Work final
    {
        WorkKind kind = WorkKind::ParseIf;
        std::size_t stateIndex = 0;
    };
    struct BlockParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* block = nullptr;
        ast::AstNode* destination = nullptr;
    };
    struct SwitchParseState final
    {
        std::size_t beginIndex = 0;
        std::size_t sectionBegin = 0;
        ast::AstNode* conditionDeclaration = nullptr;
        ast::AstNode* node = nullptr;
        ast::AstNode* section = nullptr;
    };
    struct TryParseState final
    {
        std::size_t beginIndex = 0;
        std::size_t pendingCatchBeginIndex = 0;
        ast::AstNode* node = nullptr;
        ast::AstNode* pendingCatch = nullptr;
        bool hasFinally = false;
    };
    struct StandaloneCatchParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* clause = nullptr;
    };
    struct UsingParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* node = nullptr;
    };
    struct DeferCapture final
    {
        bool byReference = false;
        bool receiver = false;
        std::string name;
        SourceRange range;
    };
    struct DeferParseState final
    {
        std::size_t beginIndex = 0;
        SourceRange deferRange;
        std::vector<DeferCapture> captures;
    };
    enum class ConditionalPatternForm
    {
        If,
        While,
        LetElse
    };
    struct ConditionalPatternParseState final
    {
        std::size_t beginIndex = 0;
        ConditionalPatternForm form = ConditionalPatternForm::LetElse;
        SourceRange featureRange;
        Pattern pattern;
        ast::AstNode* sourceExpression = nullptr;
        ast::AstNode* successBody = nullptr;
        ast::AstNode* failureBody = nullptr;
    };
    struct MatchArm final
    {
        Pattern pattern;
        SourceRange range;
        ast::AstNode* guard = nullptr;
        ast::AstNode* body = nullptr;
    };
    struct MatchParseState final
    {
        std::size_t beginIndex = 0;
        std::size_t armBeginIndex = 0;
        SourceRange keywordRange;
        ast::AstNode* governing = nullptr;
        MatchArm pendingArm;
        std::vector<MatchArm> arms;
        bool sawCatchAll = false;
        std::vector<std::pair<std::int64_t, std::int64_t>> coveredIntegerRanges;
    };

    bool ordinaryIfAtCursor() const noexcept
    {
        const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return parser_.at(TokenKind::KwIf) && !conditional;
    }

    bool ordinaryWhileAtCursor() const noexcept
    {
        const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return parser_.at(TokenKind::KwWhile) && !conditional;
    }

    bool conditionalPatternAtCursor() const noexcept
    {
        const bool conditionalControl =
            (parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile)) &&
            parser_.peek().kind() == TokenKind::LeftParen &&
            parser_.peek(2).kind() == TokenKind::Identifier &&
            parser_.peek(2).lexeme() == "let";
        return conditionalControl || parser_.atContextualKeyword("let");
    }

    bool structuredControlAtCursor() const noexcept
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

    ast::AstNode* wrapEmbedded(ast::AstNode* statement)
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

    void dispatch(const Work& current)
    {
        switch (current.kind)
        {
        case WorkKind::ParseControl: parseControl(); break;
        case WorkKind::ParseIf: parseIf(); break;
        case WorkKind::ParseWhile: parseWhile(); break;
        case WorkKind::ParseDoWhile: parseDoWhile(); break;
        case WorkKind::ParseFor: parseFor(); break;
        case WorkKind::ParseTry: parseTry(); break;
        case WorkKind::ParseStandaloneCatch: parseStandaloneCatch(); break;
        case WorkKind::ParseUsing: parseUsing(); break;
        case WorkKind::ParseDefer: parseDefer(); break;
        case WorkKind::ParseForeach: parseForeach(); break;
        case WorkKind::ParseConditionalPattern: parseConditionalPattern(); break;
        case WorkKind::ParseSwitch: parseSwitch(); break;
        case WorkKind::ParseMatch: parseMatch(); break;
        case WorkKind::ParseEmbedded: parseEmbedded(); break;
        case WorkKind::WrapEmbedded: finishWrapEmbedded(); break;
        case WorkKind::ParseBlock: parseBlock(); break;
        case WorkKind::ContinueBlock: continueBlock(current); break;
        case WorkKind::AppendBlock: appendBlock(current.stateIndex); break;
        case WorkKind::FinishThen: finishThen(current.stateIndex); break;
        case WorkKind::FinishElse: finishElse(current.stateIndex); break;
        case WorkKind::FinishWhile: finishWhile(current.stateIndex); break;
        case WorkKind::FinishDoWhile: finishDoWhile(current.stateIndex); break;
        case WorkKind::FinishFor: finishFor(current.stateIndex); break;
        case WorkKind::ContinueTry: continueTry(current.stateIndex); break;
        case WorkKind::FinishTryBlock: finishTryBlock(current.stateIndex); break;
        case WorkKind::FinishCatchBlock: finishCatchBlock(current.stateIndex); break;
        case WorkKind::FinishFinallyBlock: finishFinallyBlock(current.stateIndex); break;
        case WorkKind::FinishStandaloneCatch: finishStandaloneCatch(current.stateIndex); break;
        case WorkKind::FinishUsing: finishUsing(current.stateIndex, popValue()); break;
        case WorkKind::FinishDefer: finishDefer(current.stateIndex, popValue()); break;
        case WorkKind::FinishForeach: finishForeach(current.stateIndex, popValue()); break;
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
        }
    }

    void parseControl()
    {
        if (ordinaryIfAtCursor()) work_.push_back({WorkKind::ParseIf, 0});
        else if (ordinaryWhileAtCursor()) work_.push_back({WorkKind::ParseWhile, 0});
        else if (parser_.at(TokenKind::KwDo)) work_.push_back({WorkKind::ParseDoWhile, 0});
        else if (parser_.at(TokenKind::KwFor)) work_.push_back({WorkKind::ParseFor, 0});
        else if (parser_.at(TokenKind::KwTry)) work_.push_back({WorkKind::ParseTry, 0});
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

    void parseIf()
    {
        const std::size_t index = ifStates_.size();
        ifStates_.emplace_back();
        parser_.parseIfHeader(ifStates_.back());
        work_.push_back({WorkKind::FinishThen, index});
        work_.push_back({WorkKind::ParseEmbedded, 0});
    }

    void parseWhile()
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

    void parseDoWhile()
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

    void parseFor()
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

    void parseTry()
    {
        const std::size_t index = tryStates_.size();
        tryStates_.push_back({parser_.index_});
        TryParseState& state = tryStates_.back();
        parser_.consume();
        state.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::TryStatement,
            parser_.tokenRangeFrom(state.beginIndex));
        work_.push_back({WorkKind::ContinueTry, index});
        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishTryBlock, index});
            work_.push_back({WorkKind::ParseBlock, 0});
        }
        else
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after try");
        }
    }

    ast::AstNode* parseCatchHeader()
    {
        const std::size_t beginIndex = parser_.index_;
        parser_.consume();
        ast::AstNode* clause = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::CatchClause,
            parser_.tokenRangeFrom(beginIndex));
        if (parser_.consumeIf(TokenKind::LeftParen))
        {
            if (parser_.canStartTypeAt(parser_.index_))
                clause->setTypeSyntax(parser_.parseType());
            else
                parser_.expect(TokenKind::Identifier, "exception type");
            if (parser_.at(TokenKind::Identifier))
                clause->setText(std::string(parser_.consume().lexeme()));
            parser_.expect(TokenKind::RightParen, "')' after catch declaration");
        }
        else
        {
            clause->setFlag(true);
        }
        if (parser_.atContextualKeyword("when"))
        {
            parser_.consume();
            parser_.expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
            clause->setSecond(parser_.parseExpression());
            parser_.expect(TokenKind::RightParen, "')' after catch filter expression");
        }
        return clause;
    }

    void continueTry(std::size_t index)
    {
        TryParseState& state = tryStates_[index];
        if (parser_.at(TokenKind::KwCatch))
        {
            state.pendingCatchBeginIndex = parser_.index_;
            state.pendingCatch = parseCatchHeader();
            work_.push_back({WorkKind::ContinueTry, index});
            if (parser_.at(TokenKind::LeftBrace))
            {
                work_.push_back({WorkKind::FinishCatchBlock, index});
                work_.push_back({WorkKind::ParseBlock, 0});
            }
            else
            {
                parser_.expect(TokenKind::LeftBrace, "'{' after catch");
                state.pendingCatch->setRange(
                    parser_.tokenRangeFrom(state.pendingCatchBeginIndex));
                state.node->addChild(state.pendingCatch);
                state.pendingCatch = nullptr;
            }
            return;
        }
        if (parser_.consumeIf(TokenKind::KwFinally))
        {
            state.hasFinally = true;
            if (parser_.at(TokenKind::LeftBrace))
            {
                work_.push_back({WorkKind::FinishFinallyBlock, index});
                work_.push_back({WorkKind::ParseBlock, 0});
            }
            else
            {
                parser_.expect(TokenKind::LeftBrace, "'{' after finally");
                values_.push_back(finishTry(state));
            }
            return;
        }
        values_.push_back(finishTry(state));
    }

    void finishTryBlock(std::size_t index)
    {
        tryStates_[index].node->setFirst(popValue());
    }

    void finishCatchBlock(std::size_t index)
    {
        TryParseState& state = tryStates_[index];
        state.pendingCatch->setFirst(popValue());
        state.pendingCatch->setRange(parser_.tokenRangeFrom(state.pendingCatchBeginIndex));
        state.node->addChild(state.pendingCatch);
        state.pendingCatch = nullptr;
    }

    void finishFinallyBlock(std::size_t index)
    {
        TryParseState& state = tryStates_[index];
        state.node->setSecond(popValue());
        values_.push_back(finishTry(state));
    }

    ast::AstNode* finishTry(TryParseState& state)
    {
        if (state.node->children().empty() && !state.hasFinally)
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "expected 'catch' or 'finally' after try block");
        }
        state.node->setRange(parser_.tokenRangeFrom(state.beginIndex));
        return state.node;
    }

    void parseStandaloneCatch()
    {
        const std::size_t index = standaloneCatchStates_.size();
        const std::size_t beginIndex = parser_.index_;
        standaloneCatchStates_.push_back({beginIndex, parseCatchHeader()});
        if (parser_.at(TokenKind::LeftBrace))
        {
            work_.push_back({WorkKind::FinishStandaloneCatch, index});
            work_.push_back({WorkKind::ParseBlock, 0});
        }
        else
        {
            parser_.expect(TokenKind::LeftBrace, "'{' after catch");
            StandaloneCatchParseState& state = standaloneCatchStates_[index];
            state.clause->setRange(parser_.tokenRangeFrom(state.beginIndex));
            values_.push_back(state.clause);
        }
    }

    void finishStandaloneCatch(std::size_t index)
    {
        StandaloneCatchParseState& state = standaloneCatchStates_[index];
        state.clause->setFirst(popValue());
        state.clause->setRange(parser_.tokenRangeFrom(state.beginIndex));
        values_.push_back(state.clause);
    }

    void parseUsing()
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

    void finishUsing(std::size_t index, ast::AstNode* body)
    {
        UsingParseState& state = usingStates_[index];
        state.node->setSecond(body);
        state.node->setRange(parser_.tokenRangeFrom(state.beginIndex));
        values_.push_back(state.node);
    }

    void parseDefer()
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

    void finishDefer(std::size_t index, ast::AstNode* body)
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

    void parseForeach()
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

    void finishForeach(std::size_t index, ast::AstNode* body)
    {
        values_.push_back(parser_.lowerForeachStatement(
            std::move(foreachStates_[index]), body));
    }

    std::vector<std::pair<std::string, PatternBindingMode>> collectPatternBindings(
        const Pattern& root)
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
    }

    void parseConditionalPatternHeader(ConditionalPatternParseState& state)
    {
        state.beginIndex = parser_.index_;
        state.featureRange = parser_.current().range();
        if (parser_.at(TokenKind::KwIf))
        {
            state.form = ConditionalPatternForm::If;
            state.featureRange = parser_.consume().range();
            parser_.expect(TokenKind::LeftParen, "'(' after 'if'");
            parser_.expectContextualKeyword("let", "'let' in conditional pattern");
        }
        else if (parser_.at(TokenKind::KwWhile))
        {
            state.form = ConditionalPatternForm::While;
            state.featureRange = parser_.consume().range();
            parser_.expect(TokenKind::LeftParen, "'(' after 'while'");
            parser_.expectContextualKeyword("let", "'let' in conditional pattern");
        }
        else
        {
            state.form = ConditionalPatternForm::LetElse;
            parser_.expectContextualKeyword("let", "'let'");
        }

        state.pattern = parser_.parsePattern(false);
        parser_.expect(TokenKind::Equal, "'=' between conditional pattern and source expression");
        state.sourceExpression = parser_.parseAssignmentExpression();
        if (state.form != ConditionalPatternForm::LetElse)
            parser_.expect(TokenKind::RightParen, "')' after conditional pattern");

        std::function<void(const Pattern&, bool)> validateBindingModes =
            [&](const Pattern& currentPattern, bool preservesAddress)
        {
            if (currentPattern.kind == PatternKind::Binding &&
                currentPattern.bindingMode != PatternBindingMode::Value && !preservesAddress)
            {
                parser_.diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    currentPattern.range,
                    "ref/move pattern binding cannot cross a tuple deconstruction or variant "
                    "payload copy; bind the aggregate first");
            }
            const bool childPreservesAddress = preservesAddress &&
                currentPattern.kind != PatternKind::Tuple &&
                currentPattern.kind != PatternKind::Variant;
            for (const Pattern& child : currentPattern.children)
                validateBindingModes(child, childPreservesAddress);
        };
        std::function<void(const Pattern&)> validateOrBindings =
            [&](const Pattern& currentPattern)
        {
            if (currentPattern.kind == PatternKind::Or && !currentPattern.children.empty())
            {
                const auto expected = collectPatternBindings(currentPattern.children.front());
                bool identical = true;
                for (std::size_t childIndex = 1;
                     childIndex < currentPattern.children.size(); ++childIndex)
                {
                    identical = identical &&
                        collectPatternBindings(currentPattern.children[childIndex]) == expected;
                }
                if (!identical)
                {
                    parser_.diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        currentPattern.range,
                        "OR-pattern alternatives must bind identical names with identical "
                        "value/ref/move modes");
                }
            }
            for (const Pattern& child : currentPattern.children) validateOrBindings(child);
        };
        validateBindingModes(state.pattern, true);
        validateOrBindings(state.pattern);
    }

    void parseConditionalPattern()
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

    void finishConditionalPatternSuccess(std::size_t index, ast::AstNode* successBody)
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

    void finishConditionalPatternFailure(std::size_t index, ast::AstNode* failureBody)
    {
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.failureBody = failureBody;
        values_.push_back(lowerConditionalPattern(state));
    }

    void finishConditionalPatternWhile(std::size_t index, ast::AstNode* successBody)
    {
        ConditionalPatternParseState& state = conditionalPatternStates_[index];
        state.successBody = successBody;
        values_.push_back(lowerConditionalPattern(state));
    }

    void finishConditionalPatternLetElse(std::size_t index, ast::AstNode* failureBody)
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

    ast::AstNode* lowerConditionalPattern(ConditionalPatternParseState& state)
    {
        const SourceRange statementRange = parser_.tokenRangeFrom(state.beginIndex);
        const std::string temporary =
            "$__nsc_conditional_pattern_" + std::to_string(parser_.nextMatchTemporary_++);
        const std::string matched = temporary + "_matched";
        const auto bindings = collectPatternBindings(state.pattern);
        const bool hasReferenceBinding = std::ranges::any_of(
            bindings, [](const auto& binding)
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
            ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
                state.featureRange, TokenKind::KwVar, "var");
            ast::AstNode* declaration = parser_.makeVariableDeclaration(
                inferred, temporary, state.sourceExpression, false,
                !hasReferenceBinding && !hasMoveBinding);
            if (hasReferenceBinding) declaration->setAuxiliaryText("local:ref");
            destination->addChild(declaration);
            ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
                state.featureRange, TokenKind::KwBool, "bool");
            destination->addChild(parser_.makeVariableDeclaration(
                boolType,
                matched,
                parser_.makeBooleanLiteral(state.featureRange, false),
                false,
                false));
        };
        const auto makeNotMatched = [&]()
        {
            ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::UnaryExpression, statementRange);
            result->setTokenKind(TokenKind::Bang);
            result->setFirst(parser_.makeIdentifierExpression(statementRange, matched));
            return result;
        };
        const auto makeSuccess = [&](bool includeBody)
        {
            ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, statementRange);
            body->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
                parser_.makeIdentifierExpression(statementRange, matched),
                parser_.makeBooleanLiteral(statementRange, true))));
            if (includeBody && state.successBody != nullptr)
            {
                for (ast::AstNode* child : state.successBody->children()) body->addChild(child);
            }
            return body;
        };
        PatternExpressionFactory root = [&, statementRange]
        {
            return parser_.makeIdentifierExpression(statementRange, temporary);
        };

        if (state.form == ConditionalPatternForm::While)
        {
            ast::AstNode* iteration = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, statementRange);
            addPrelude(iteration);
            iteration->addChild(parser_.lowerPattern(
                state.pattern, root, makeSuccess(true), temporary));
            ast::AstNode* stopBody = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, statementRange);
            stopBody->addChild(parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BreakStatement, statementRange));
            ast::AstNode* stop = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::IfStatement, statementRange);
            stop->setFirst(makeNotMatched());
            stop->setSecond(stopBody);
            iteration->addChild(stop);

            ast::AstNode* loop = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::WhileStatement, statementRange);
            loop->setFirst(parser_.makeBooleanLiteral(state.featureRange, true));
            loop->setSecond(iteration);
            return loop;
        }

        ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, statementRange);
        addPrelude(lowered);
        lowered->addChild(parser_.lowerPattern(
            state.pattern,
            root,
            makeSuccess(state.form == ConditionalPatternForm::If),
            temporary,
            state.form != ConditionalPatternForm::LetElse));

        if (state.failureBody != nullptr)
        {
            ast::AstNode* failure = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::IfStatement, statementRange);
            failure->setFirst(makeNotMatched());
            failure->setSecond(state.failureBody);
            lowered->addChild(failure);
        }

        if (state.form == ConditionalPatternForm::LetElse)
        {
            parser_.emitPatternBindings(state.pattern, root, lowered, temporary);
            lowered->setAuxiliaryText("conditional-pattern-inline");
        }
        return lowered;
    }

    void parseSwitch()
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

    ast::AstNode* parseSwitchGoverning(SwitchParseState& state)
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

    void continueSwitch(std::size_t index)
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

    void startSwitchSection(std::size_t index)
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

    void appendSwitchLabel(ast::AstNode* section)
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

    void continueSwitchSection(const Work& current)
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

    void pushSwitchStatement(std::size_t index, WorkKind child)
    {
        work_.push_back({WorkKind::AppendSwitchStatement, index});
        work_.push_back({child, 0});
    }

    void appendSwitchStatement(std::size_t index)
    {
        appendSwitchStatement(switchStates_[index], popValue());
    }

    static void appendSwitchStatement(SwitchParseState& state, ast::AstNode* statement)
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

    ast::AstNode* finishSwitch(SwitchParseState& state)
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
    void parseMatch()
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
    void continueMatch(std::size_t index)
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
    void parseMatchArmHeader(MatchParseState& state)
    {
        state.armBeginIndex = parser_.index_;
        state.pendingArm = MatchArm{};
        state.pendingArm.pattern = parser_.parsePattern(false);
        state.pendingArm.range = state.pendingArm.pattern.range;
        validateMatchBindingModes(state.pendingArm.pattern);
        validateMatchOrBindings(state.pendingArm.pattern);
        if (parser_.atContextualKeyword("when"))
        {
            parser_.consume();
            state.pendingArm.guard = parser_.parseConditionalExpression();
        }
        validateMatchArmReachability(state);
        parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    }

    // Preserves the historical missing-block diagnostic before statement synchronization.
    void reportMissingMatchArmBody()
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "a match arm requires a block body");
    }

    // Returns the stable spelling embedded in binding-mode diagnostics.
    static std::string_view matchBindingModeName(PatternBindingMode mode) noexcept
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

    // Collects bindings iteratively in source order, then canonicalizes them for equality checks.
    static std::vector<std::pair<std::string, PatternBindingMode>> collectMatchBindings(
        const Pattern& root)
    {
        std::vector<std::pair<std::string, PatternBindingMode>> bindings;
        std::vector<const Pattern*> pending{&root};
        while (!pending.empty())
        {
            const Pattern* pattern = pending.back();
            pending.pop_back();
            if (pattern->kind == PatternKind::Binding)
                bindings.emplace_back(pattern->name, pattern->bindingMode);
            else
                for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                    pending.push_back(&*child);
        }
        std::ranges::sort(bindings, [](const auto& left, const auto& right)
        {
            if (left.first != right.first) return left.first < right.first;
            return left.second < right.second;
        });
        return bindings;
    }

    // Tests wildcard/binding reachability through OR alternatives using an explicit worklist.
    static bool matchPatternIsCatchAll(const Pattern& root)
    {
        std::vector<const Pattern*> pending{&root};
        while (!pending.empty())
        {
            const Pattern* pattern = pending.back();
            pending.pop_back();
            if (pattern->kind == PatternKind::Wildcard || pattern->kind == PatternKind::Binding)
                return true;
            if (pattern->kind != PatternKind::Or) continue;
            for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                pending.push_back(&*child);
        }
        return false;
    }

    // Propagates address preservation iteratively and reports invalid ref/move binding sites.
    void validateMatchBindingModes(const Pattern& root)
    {
        std::vector<std::pair<const Pattern*, bool>> pending{{&root, true}};
        while (!pending.empty())
        {
            const auto [pattern, preservesAddress] = pending.back();
            pending.pop_back();
            if (invalidMatchBindingMode(*pattern, preservesAddress)) continue;
            const bool childPreservesAddress = preservesAddress &&
                pattern->kind != PatternKind::Tuple && pattern->kind != PatternKind::Variant;
            for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                pending.emplace_back(&*child, childPreservesAddress);
        }
    }

    // Reports one binding-mode violation and returns whether child traversal must stop.
    bool invalidMatchBindingMode(const Pattern& pattern, bool preservesAddress)
    {
        if (pattern.kind != PatternKind::Binding ||
            pattern.bindingMode == PatternBindingMode::Value || preservesAddress)
            return false;
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            pattern.range,
            std::string(matchBindingModeName(pattern.bindingMode)) +
                " pattern binding cannot cross a tuple deconstruction or variant payload "
                "copy; bind the aggregate by ref/move first");
        return true;
    }

    // Visits every OR pattern without recursion and compares each alternative's binding set.
    void validateMatchOrBindings(const Pattern& root)
    {
        std::vector<const Pattern*> pending{&root};
        while (!pending.empty())
        {
            const Pattern* pattern = pending.back();
            pending.pop_back();
            if (pattern->kind == PatternKind::Or && !pattern->children.empty())
                validateMatchOrAlternatives(*pattern);
            for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                pending.push_back(&*child);
        }
    }

    // Requires every alternative of one OR node to bind identical names and modes.
    void validateMatchOrAlternatives(const Pattern& pattern)
    {
        const auto expected = collectMatchBindings(pattern.children.front());
        for (std::size_t index = 1; index < pattern.children.size(); ++index)
        {
            if (collectMatchBindings(pattern.children[index]) == expected) continue;
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                pattern.range,
                "OR-pattern alternatives must bind identical names with identical "
                "value/ref/move modes");
            return;
        }
    }

    // Updates cross-arm reachability state while preserving diagnostic order.
    void validateMatchArmReachability(MatchParseState& state)
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
        if (matchPatternIsCatchAll(arm.pattern) && guardAlwaysTrue) state.sawCatchAll = true;
        validateMatchIntegerIntervals(state, guardAlwaysTrue);
    }

    // Emits the established constant-false guard diagnostic at the guard range.
    void reportConstantFalseMatchGuard(const MatchArm& arm)
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            arm.guard->range(),
            "match arm is unreachable because its guard is the constant false");
    }

    // Emits the established unreachable-arm diagnostic after an unconditional catch-all.
    void reportMatchArmAfterCatchAll(const MatchArm& arm)
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            arm.range,
            "match arm is unreachable after a catch-all wildcard or binding arm");
    }

    // Collects a complete literal/range OR domain; false means the mixed domain is not comparable.
    static bool collectMatchIntegerIntervals(
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
    static bool appendMatchIntegerInterval(
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
    void validateMatchIntegerIntervals(MatchParseState& state, bool guardAlwaysTrue)
    {
        std::vector<std::pair<std::int64_t, std::int64_t>> current;
        if (!collectMatchIntegerIntervals(state.pendingArm.pattern, current)) return;
        if (matchIntervalsOverlap(current, state.coveredIntegerRanges))
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                state.pendingArm.range,
                "integer literal/range pattern overlaps an earlier alternative");
        if (guardAlwaysTrue)
            state.coveredIntegerRanges.insert(
                state.coveredIntegerRanges.end(), current.begin(), current.end());
    }

    // Compares current intervals with prior arms and earlier alternatives in the same arm.
    static bool matchIntervalsOverlap(
        const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
        const std::vector<std::pair<std::int64_t, std::int64_t>>& covered)
    {
        for (std::size_t index = 0; index < current.size(); ++index)
        {
            const auto interval = current[index];
            for (const auto& earlier : covered)
            {
                if (interval.first < earlier.second && earlier.first < interval.second) return true;
            }
            for (std::size_t earlier = 0; earlier < index; ++earlier)
                if (interval.first < current[earlier].second &&
                    current[earlier].first < interval.second)
                    return true;
        }
        return false;
    }

    // Attaches a completed body, closes the source range, and consumes the optional comma.
    void finishMatchArm(std::size_t index, ast::AstNode* body)
    {
        MatchParseState& state = matchStates_[index];
        state.pendingArm.body = body;
        state.pendingArm.range = parser_.tokenRangeFrom(state.armBeginIndex);
        state.arms.push_back(std::move(state.pendingArm));
        parser_.consumeIf(TokenKind::Comma);
    }

    // Materializes the historical early lowering after every arm body has completed.
    ast::AstNode* lowerMatchStatement(MatchParseState& state)
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
    void reportEmptyMatch(const MatchParseState& state)
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            state.keywordRange,
            "a match statement requires at least one arm");
    }

    // Builds the source-ordered semantic coverage bridge consumed by later analysis.
    std::string buildMatchCoverage(const std::vector<MatchArm>& arms)
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
    void appendMatchArmCoverage(std::string& coverage, const MatchArm& arm)
    {
        std::vector<std::string> cases;
        std::vector<std::pair<std::size_t, bool>> listLengths;
        bool wildcard = false;
        bool partial = false;
        collectMatchCoverage(arm.pattern, cases, listLengths, wildcard, partial);
        const bool restricted = arm.guard != nullptr &&
            !(arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
              arm.guard->tokenKind() == TokenKind::KwTrue);
        if (wildcard)
        {
            if (restricted) coverage += '~';
            coverage += '*';
            return;
        }
        if (!listLengths.empty())
        {
            appendMatchListCoverage(coverage, listLengths, partial || restricted);
            return;
        }
        appendMatchCaseCoverage(coverage, cases, partial || restricted);
    }

    // Collects coverage facts iteratively while preserving OR alternative order.
    static void collectMatchCoverage(
        const Pattern& root,
        std::vector<std::string>& cases,
        std::vector<std::pair<std::size_t, bool>>& listLengths,
        bool& wildcard,
        bool& partial)
    {
        std::vector<const Pattern*> pending{&root};
        while (!pending.empty())
        {
            const Pattern* pattern = pending.back();
            pending.pop_back();
            if (pattern->kind == PatternKind::Wildcard || pattern->kind == PatternKind::Binding)
                wildcard = true;
            else if (pattern->kind == PatternKind::Variant)
            {
                cases.push_back(pattern->name);
                if (!pattern->children.empty() &&
                    !matchPatternIsCatchAll(pattern->children.front())) partial = true;
            }
            else if (pattern->kind == PatternKind::List)
                collectMatchListCoverage(*pattern, listLengths, partial);
            else if (pattern->kind == PatternKind::Or)
                for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
                    pending.push_back(&*child);
        }
    }

    // Records one list-length domain and whether element subpatterns make it partial.
    static void collectMatchListCoverage(
        const Pattern& pattern,
        std::vector<std::pair<std::size_t, bool>>& lengths,
        bool& partial)
    {
        const std::size_t fixedCount = pattern.children.size() -
            (pattern.restIndex.has_value() ? 1U : 0U);
        lengths.emplace_back(fixedCount, pattern.restIndex.has_value());
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            if (pattern.restIndex.has_value() && *pattern.restIndex == index) continue;
            if (!matchPatternIsCatchAll(pattern.children[index]))
            {
                partial = true;
                return;
            }
        }
    }

    // Sorts and deduplicates list domains before emitting their stable marker format.
    static void appendMatchListCoverage(
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
    static void appendMatchCaseCoverage(
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
    void appendMatchGoverningState(
        ast::AstNode& lowered,
        const MatchParseState& state,
        const std::string& temporary)
    {
        const bool hasReference = matchHasBindingMode(
            state.arms,
            PatternBindingMode::Reference,
            PatternBindingMode::ReferenceMutable);
        const bool hasMove = matchHasBindingMode(
            state.arms,
            PatternBindingMode::Move,
            PatternBindingMode::Move);
        ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
            state.keywordRange,
            TokenKind::KwVar,
            "var");
        ast::AstNode* governing = parser_.makeVariableDeclaration(
            inferred,
            temporary,
            state.governing,
            false,
            !hasReference && !hasMove);
        if (hasReference) governing->setAuxiliaryText("local:ref");
        lowered.addChild(governing);
        lowered.addChild(makeMatchSelectedDeclaration(state.keywordRange, temporary));
    }

    // Returns whether any arm binds with either requested ownership mode.
    static bool matchHasBindingMode(
        const std::vector<MatchArm>& arms,
        PatternBindingMode first,
        PatternBindingMode second)
    {
        for (const MatchArm& arm : arms)
            for (const auto& binding : collectMatchBindings(arm.pattern))
                if (binding.second == first || binding.second == second) return true;
        return false;
    }

    // Creates the false-initialized selected flag used by the lowered decision chain.
    ast::AstNode* makeMatchSelectedDeclaration(
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
    ast::AstNode* lowerMatchArm(
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
    ast::AstNode* makeMatchSelectedBody(const MatchArm& arm, const std::string& temporary)
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
    ast::AstNode* wrapMatchGuard(const MatchArm& arm, ast::AstNode* selectedBody)
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
    ast::AstNode* makeMatchNotSelected(SourceRange range, const std::string& temporary)
    {
        ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression,
            range);
        result->setTokenKind(TokenKind::Bang);
        result->setFirst(parser_.makeIdentifierExpression(range, temporary + "_selected"));
        return result;
    }

    void parseEmbedded()
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

    void finishWrapEmbedded()
    {
        ast::AstNode* statement = popValue();
        values_.push_back(wrapEmbedded(statement));
    }

    void parseBlock()
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

    void continueBlock(const Work& current)
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

    void pushBlockChild(std::size_t blockIndex, WorkKind child)
    {
        work_.push_back({WorkKind::AppendBlock, blockIndex});
        work_.push_back({child, 0});
    }

    void appendBlock(std::size_t index)
    {
        parser_.appendParsedStatement(blockStates_[index].destination, popValue());
    }

    void finishThen(std::size_t index)
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

    void finishElse(std::size_t index)
    {
        values_.push_back(parser_.lowerIfStatement(ifStates_[index], popValue()));
    }

    void finishWhile(std::size_t index)
    {
        values_.push_back(parser_.lowerWhileStatement(whileStates_[index], popValue()));
    }

    void finishDoWhile(std::size_t index)
    {
        values_.push_back(parser_.lowerDoWhileStatement(doWhileStates_[index], popValue()));
    }

    void finishFor(std::size_t index)
    {
        values_.push_back(parser_.lowerForStatement(forStates_[index], popValue()));
    }

    ast::AstNode* popValue()
    {
        ast::AstNode* value = values_.back();
        values_.pop_back();
        return value;
    }

    Parser& parser_;
    std::vector<IfParseState> ifStates_;
    std::vector<WhileParseState> whileStates_;
    std::vector<DoWhileParseState> doWhileStates_;
    std::vector<ForParseState> forStates_;
    std::vector<TryParseState> tryStates_;
    std::vector<StandaloneCatchParseState> standaloneCatchStates_;
    std::vector<UsingParseState> usingStates_;
    std::vector<DeferParseState> deferStates_;
    std::vector<ForeachParseState> foreachStates_;
    std::vector<ConditionalPatternParseState> conditionalPatternStates_;
    std::vector<SwitchParseState> switchStates_;
    std::vector<MatchParseState> matchStates_;
    std::vector<BlockParseState> blockStates_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
};

ast::AstNode* Parser::parseStructuredControlStatement()
{
    return StructuredControlParser(*this).parse();
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

void Parser::parseWhileHeader(WhileParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    if (looksLikeConditionDeclaration())
    {
        state.conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            state.conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            state.condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerWhileStatement(
    const WhileParseState& state,
    ast::AstNode* body)
{
    const SourceRange statementRange = tokenRangeFrom(state.beginIndex);
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::WhileStatement, statementRange);
    if (state.conditionDeclaration == nullptr)
    {
        node->setFirst(state.condition);
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
    iteration->addChild(state.conditionDeclaration);

    ast::AstNode* stopCondition = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, state.condition->range());
    stopCondition->setTokenKind(TokenKind::Bang);
    stopCondition->setFirst(state.condition);
    ast::AstNode* stopBody = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    stopBody->addChild(arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, state.condition->range()));
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

ast::AstNode* Parser::parseWhileStatement()
{
    return parseStructuredControlStatement();
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
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::lowerDoWhileStatement(
    const DoWhileParseState& state,
    ast::AstNode* body)
{
    expect(TokenKind::KwWhile, "'while' after the do-loop body");
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();
    expect(TokenKind::RightParen, "')'");
    expect(TokenKind::Semicolon, "';' after the do-while condition");

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DoWhileStatement,
        tokenRangeFrom(state.beginIndex));
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
    return parseStructuredControlStatement();
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
        // Pattern precedence is unary `!`, then `&`, then `|`. Recurse only through the unary
        // layer here; a grouped pattern deliberately re-enters parsePattern below.
        result.children.push_back(parsePatternPrimary(true));
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
            // NS304-T03-F04: a known type name introduces positional deconstruction even when it
            // has one element. Variant case names are not types and keep the tagged-union path.
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
            result.kind = knownTypeNames_.contains(result.name)
                ? PatternKind::Positional
                : PatternKind::Variant;
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
                pattern.bindingMode == PatternBindingMode::Copy ||
                pattern.bindingMode == PatternBindingMode::Reference);
        if (pattern.bindingMode == PatternBindingMode::Reference)
            declaration->setAuxiliaryText("local:pattern-ref");
        else if (pattern.bindingMode == PatternBindingMode::ReferenceMutable)
            declaration->setAuxiliaryText("local:pattern-ref-mut");
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
        // The nominal source is deconstructed exactly once. Each child is then lowered through
        // the ordinary pattern path, so literals, relational patterns and nested patterns produce
        // real predicates instead of an unconditional `true` arm.
        ast::AstNode* block = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.range);
        ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
        deconstruction->setFirst(source());
        deconstruction->setTypeSyntax(arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::Identifier, pattern.name));
        std::vector<std::string> elementNames;
        elementNames.reserve(pattern.children.size());
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = std::string(temporaryPrefix) + "_pos_" +
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
                mode == PatternBindingMode::Value || mode == PatternBindingMode::Copy ||
                    mode == PatternBindingMode::Reference);
            if (mode == PatternBindingMode::Reference)
                declaration->setAuxiliaryText("local:pattern-ref");
            else if (mode == PatternBindingMode::ReferenceMutable)
                declaration->setAuxiliaryText("local:pattern-ref-mut");
            destination->addChild(declaration);
        }
        return;
    }
    if (pattern.kind == PatternKind::Binding)
    {
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        ast::AstNode* initializer = source();
        // A rest value is a borrowed view. Its local cell is compiler-generated, but making that
        // cell const would also make a mutable Span receiver readonly and incorrectly suppress
        // writes to the referenced storage. Readonly sources already produce ReadOnlySpan<T>.
        const bool mutableRestView =
            pattern.bindingMode == PatternBindingMode::Value &&
            initializer != nullptr &&
            initializer->kind() == ast::AstNodeKind::CallExpression &&
            initializer->auxiliaryText() == "list-pattern-rest";
        ast::AstNode* declaration = makeVariableDeclaration(
            type,
            pattern.name,
            initializer,
            false,
            !mutableRestView &&
                (pattern.bindingMode == PatternBindingMode::Value ||
                 pattern.bindingMode == PatternBindingMode::Copy ||
                 pattern.bindingMode == PatternBindingMode::Reference));
        if (pattern.bindingMode == PatternBindingMode::Reference)
            declaration->setAuxiliaryText("local:pattern-ref");
        else if (pattern.bindingMode == PatternBindingMode::ReferenceMutable)
            declaration->setAuxiliaryText("local:pattern-ref-mut");
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
        // Deconstruct once inside the selected arm. The arm guard runs after its declarations, so
        // child predicates can consume these temporaries without repeating a stateful Deconstruct.
        ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
        deconstruction->setFirst(source());
        deconstruction->setTypeSyntax(arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::Identifier, pattern.name));
        std::vector<std::string> elementNames;
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = std::string(temporaryPrefix) + "_pos_" +
                std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
            elementNames.push_back(name);
            deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
        }
        destination->addChild(deconstruction);
        ast::AstNode* positionalCondition = makeBooleanLiteral(pattern.range, true);
        for (std::size_t index = 0; index < pattern.children.size(); ++index)
        {
            const std::string name = elementNames[index];
            PatternExpressionFactory element = [&, name]
            {
                return makeIdentifierExpression(pattern.range, name);
            };
            emitPatternBindings(
                pattern.children[index], element, destination, temporaryPrefix);
            positionalCondition = makeBinaryExpression(
                TokenKind::AmpAmp,
                positionalCondition,
                makePatternCondition(pattern.children[index], element));
        }
        if (destination->kind() == ast::AstNodeKind::MatchArmExpression)
        {
            if (ast::AstNode* guard = destination->second(); guard != nullptr)
            {
                positionalCondition = makeBinaryExpression(
                    TokenKind::AmpAmp, positionalCondition, guard);
            }
            destination->setSecond(positionalCondition);
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
    return parseStructuredControlStatement();
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
    bool hasReferenceBinding = false;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t armBegin = index_;
        Pattern pattern = parsePattern(false);
        validateOrBindings(pattern);
        const auto armBindings = collectBindings(pattern);
        hasReferenceBinding = hasReferenceBinding || std::ranges::any_of(
            armBindings, [](const auto& binding)
            {
                return binding.second == PatternBindingMode::Reference ||
                    binding.second == PatternBindingMode::ReferenceMutable;
            });
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
            std::vector<std::pair<std::size_t, bool>> listLengths;
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
                    listLengths.emplace_back(fixedCount, current.restIndex.has_value());
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
            if (wildcard)
            {
                if (guardRestrictsCoverage) coverage += '~';
                coverage += '*';
            }
            else if (!listLengths.empty())
            {
                std::ranges::sort(listLengths);
                listLengths.erase(
                    std::unique(listLengths.begin(), listLengths.end()), listLengths.end());
                if (partial || guardRestrictsCoverage) coverage += '~';
                for (std::size_t index = 0; index < listLengths.size(); ++index)
                {
                    if (index != 0) coverage += '|';
                    coverage += '#' + std::to_string(listLengths[index].first);
                    if (listLengths[index].second) coverage += '+';
                }
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
    match->setFlag(hasReferenceBinding);
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

ast::AstNode* Parser::parseForExpressionList(std::size_t beginIndex)
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
}

void Parser::parseForHeader(ForParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");

    if (!at(TokenKind::Semicolon))
    {
        if (at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVolatile) ||
            at(TokenKind::KwVar) || canStartTypeAt(index_))
        {
            state.initializer = parseVariableDeclaration(false);
        }
        else
        {
            state.initializer = parseForExpressionList(state.beginIndex);
        }
    }
    expect(TokenKind::Semicolon, "';'");

    if (!at(TokenKind::Semicolon))
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    if (!at(TokenKind::RightParen))
    {
        state.increment = parseForExpressionList(state.beginIndex);
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerForStatement(
    const ForParseState& state,
    ast::AstNode* body)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ForStatement,
        tokenRangeFrom(state.beginIndex));
    node->setFirst(state.initializer);
    node->setSecond(state.condition);
    node->setThird(state.increment);
    node->addChild(body);
    return node;
}

ast::AstNode* Parser::parseForStatement()
{
    return parseStructuredControlStatement();
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

bool Parser::parseForeachHeader(ForeachParseState& state)
{
    state.beginIndex = index_;
    // Reserve the identity before parsing the body so a nested foreach receives a different
    // namespace without changing the names already chosen for its enclosing loop.
    state.temporaryPrefix =
        "$__foreach_" + std::to_string(nextMatchTemporary_++);
    state.sourceName = state.temporaryPrefix + "_source";
    state.indexName = state.temporaryPrefix + "_index";
    state.enumeratorName = state.temporaryPrefix + "_enumerator";
    state.rangeBeginName = state.temporaryPrefix + "_range_begin";
    state.rangeEndName = state.temporaryPrefix + "_range_end";
    state.rangeStepName = state.temporaryPrefix + "_range_step";
    state.rangeCursorName = state.temporaryPrefix + "_range_cursor";
    state.elementTemporaryName = state.temporaryPrefix + "_element";
    state.patternTemporaryPrefix = state.temporaryPrefix + "_pattern";
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
    state.elementByReference = atContextualKeyword("ref");
    if (state.elementByReference)
    {
        consume();
        if (atContextualKeyword("mut"))
        {
            state.elementMutableReference = true;
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
    state.elementIsRaii = consumeIf(TokenKind::KwRaii);
    if (state.elementByReference && state.elementIsRaii)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a foreach reference binding aliases source storage and cannot also be raii");
    }
    if (at(TokenKind::KwVar))
    {
        const lexer::Token& varToken = consume();
        state.elementType =
            arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        state.elementType = parseType();
    }

    state.elementDeconstruction = at(TokenKind::LeftParen);
    if (state.elementDeconstruction)
    {
        state.elementPattern = parsePattern(true);
        state.elementName = state.elementTemporaryName;
    }
    else if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "foreach element name");
        synchronizeStatement();
        return false;
    }
    else
    {
        state.elementName = std::string(consume().lexeme());
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
    state.collection = parseExpression();
    const std::size_t collectionEnd = index_;
    index_ = collectionStart;
    state.collectionProbe = parseExpression();
    index_ = collectionEnd;
    expect(TokenKind::RightParen, "')'");
    return true;
}

ast::AstNode* Parser::parseForeachStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::lowerForeachStatement(ForeachParseState&& state, ast::AstNode* body)
{
    const std::size_t beginIndex = state.beginIndex;
    const std::string& sourceName = state.sourceName;
    const std::string& indexName = state.indexName;
    const std::string& enumeratorName = state.enumeratorName;
    const std::string& rangeBeginName = state.rangeBeginName;
    const std::string& rangeEndName = state.rangeEndName;
    const std::string& rangeStepName = state.rangeStepName;
    const std::string& rangeCursorName = state.rangeCursorName;
    const std::string& patternTemporaryPrefix = state.patternTemporaryPrefix;
    const bool elementByReference = state.elementByReference;
    const bool elementMutableReference = state.elementMutableReference;
    const bool elementIsRaii = state.elementIsRaii;
    ast::TypeSyntax* elementType = state.elementType;
    const bool elementDeconstruction = state.elementDeconstruction;
    const Pattern& elementPattern = state.elementPattern;
    const std::string& elementName = state.elementName;
    ast::AstNode* collection = state.collection;
    ast::AstNode* collectionProbe = state.collectionProbe;

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
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::parseTryStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::parseCatchClause()
{
    return StructuredControlParser(*this).parseCatchClause();
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
    return parseStructuredControlStatement();
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
    // NS304-T11-F07: `target = ref storage` stores the address of a storage into a ref field or
    // ref local (distinct from a value copy). Only plain `=` accepts the `ref` marker.
    const bool referenceAssignment = operation == TokenKind::Equal &&
        atContextualKeyword("ref");
    if (referenceAssignment) consume();
    ast::AstNode* right = parseAssignmentExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression,
        join(left->range(), right->range()));
    node->setTokenKind(operation);
    node->setFirst(left);
    node->setSecond(right);
    node->setReference(referenceAssignment);
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
    // NS304-T04-F05: `static` is structural callable metadata. It must not share the auxiliary
    // text channel with capture contracts, effects, calling conventions, or user spellings.
    if (at(TokenKind::KwStatic))
    {
        consume();
        lambda->addModifier(TokenKind::KwStatic);
    }
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

    struct UnaryPrefix
    {
        TokenKind kind;
        SourceRange range;
    };
    const auto isOrdinaryUnaryPrefix = [](TokenKind kind) noexcept
    {
        return kind == TokenKind::Plus || kind == TokenKind::Minus || kind == TokenKind::Bang ||
            kind == TokenKind::Tilde || kind == TokenKind::Amp || kind == TokenKind::Star ||
            kind == TokenKind::PlusPlus || kind == TokenKind::MinusMinus;
    };
    std::vector<UnaryPrefix> prefixes;
    while (isOrdinaryUnaryPrefix(current().kind()))
    {
        const TokenKind kind = current().kind();
        prefixes.push_back(UnaryPrefix{kind, consume().range()});
    }
    if (!prefixes.empty())
    {
        ast::AstNode* operand = parseUnaryExpression();
        for (auto prefix = prefixes.rbegin(); prefix != prefixes.rend(); ++prefix)
        {
            ast::AstNode* node = arena_.make<ast::AstNode>(
                ast::AstNodeKind::UnaryExpression,
                join(prefix->range, operand->range()));
            node->setTokenKind(prefix->kind);
            node->setFirst(operand);
            operand = node;
        }
        return operand;
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
            // NS304-T13-F06: `new (allocator, trailing: capacity) T(...)` — the contextual
            // `trailing:` capacity is the flexible-array extent, not a constructor argument.
            // NS304-T13-F08: `new (allocator, failure: panic|null|result|throws) T(...)` — the
            // explicit allocation-failure profile.
            bool hasTrailing = false;
            ast::AstNode* trailingCapacity = nullptr;
            std::string failureProfile;
            while (consumeIf(TokenKind::Comma))
            {
                if (atContextualKeyword("trailing") && peek().kind() == TokenKind::Colon)
                {
                    consume();
                    consume();
                    trailingCapacity = parseExpression();
                    hasTrailing = trailingCapacity != nullptr;
                    continue;
                }
                if (atContextualKeyword("failure") && peek().kind() == TokenKind::Colon)
                {
                    consume();
                    consume();
                    if (atContextualKeyword("panic") || atContextualKeyword("result") ||
                        atContextualKeyword("throws") || current().kind() == TokenKind::KwNull)
                    {
                        failureProfile = current().lexeme();
                        consume();
                    }
                    else
                    {
                        expect(TokenKind::Colon, "'panic', 'null', 'result' or 'throws' after 'failure:'");
                    }
                    continue;
                }
                expect(TokenKind::Colon, "':' after placement option");
                break;
            }
            expect(TokenKind::RightParen, "')' after placement storage");
            ast::TypeSyntax* type = parseAllocationElementType();
            ast::AstNode* creation = arena_.make<ast::AstNode>(
                ast::AstNodeKind::PlacementConstructionExpression,
                start);
            creation->setFirst(storage);
            creation->setTypeSyntax(type);
            if (hasTrailing)
            {
                creation->setAuxiliaryText("trailing");
            }
            if (!failureProfile.empty())
            {
                creation->setAllocatorFailureProfile(failureProfile);
            }
            // NS304-T13-F08 `failure: result`: synthesize the closed type use
            // `Result<T, AllocationFailure>` so the generic phase materializes its specialization.
            if (failureProfile == "result")
            {
                ast::TypeSyntax* resultType = arena_.make<ast::TypeSyntax>(
                    type->range(), TokenKind::Identifier, "Result");
                resultType->addTypeArgument(cloneParsedType(arena_, type));
                resultType->addTypeArgument(arena_.make<ast::TypeSyntax>(
                    type->range(), TokenKind::Identifier, "AllocationFailure"));
                creation->addTypeSyntax(resultType);
            }
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
            if (hasTrailing)
            {
                // NS304-T13-F06: the trailing capacity is the LAST child so that the leading
                // children are exactly the constructor arguments.
                creation->addChild(trailingCapacity);
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

ast::AstNode* Parser::parseCollectionExpression()
{
    // NS304-T09-F05: `[a, b, c]` — los elementos se bajan como children en orden. El spread
    // (`..x`) y el builder protocol quedan fuera de esta fundación léxica/sintáctica.
    const SourceRange begin = consume().range(); // '['
    ast::AstNode* collection = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CollectionExpression, begin);
    // La cuenta se infiere de los elementos (igual que `new T[] { ... }`), que es la ruta que
    // Sema/HIR ya saben bajar sin contar un `first()`.
    collection->setAuxiliaryText("inferred-array-initializer");
    if (!at(TokenKind::RightBracket))
    {
        while (true)
        {
            if (at(TokenKind::DotDot))
            {
                // NS304-T09-F05: `..x` — spread element (igual que en aggregate initializers).
                const SourceRange spread = consume().range();
                ast::AstNode* value = parseAssignmentExpression();
                ast::AstNode* element = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::SpreadElementExpression,
                    join(spread, value != nullptr ? value->range() : spread));
                element->setTokenKind(TokenKind::DotDot);
                element->setFirst(value);
                collection->addChild(element);
            }
            else
            {
                ast::AstNode* element = parseAssignmentExpression();
                if (element != nullptr) collection->addChild(element);
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightBracket, "']' after collection expression");
    collection->setRange(SourceRange(begin.begin(), tokens_[index_ - 1].range().end()));
    return collection;
}

ast::AstNode* Parser::parseNameOfExpression()
{
    // Keep the source operand until semantic lookup proves that the selected symbol exists and
    // is accessible. The constant string is produced only after that validation.
    const SourceRange begin = current().range();
    consume(); // `nameof`
    expect(TokenKind::LeftParen, "'(' after 'nameof'");

    ast::AstNode* operand = nullptr;
    std::vector<ast::TypeSyntax*> genericArguments;
    ast::TypeSyntax* genericTarget = nullptr;
    // A generic name without a call cannot use the ordinary postfix parser: that parser rightly
    // requires `<...>` to be followed by `(`. Keep the same identifier/member AST shape here and
    // preserve the complete candidate type plus its explicit arguments on NameOfExpression;
    // Sema decides whether the name denotes a type or a method and validates its arity.
    const auto qualifiedGenericType = [&]()
    {
        std::size_t cursor = index_;
        while (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier)
        {
            ++cursor;
            if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Dot)
            {
                ++cursor;
                continue;
            }
            break;
        }
        return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Less;
    };
    if (at(TokenKind::Identifier) && (peek().kind() == TokenKind::Less || qualifiedGenericType()))
    {
        const lexer::Token& identifier = consume();
        SourceRange genericRange = identifier.range();
        std::string qualifiedName(identifier.lexeme());
        operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IdentifierExpression, identifier.range());
        operand->setText(std::string(identifier.lexeme()));
        while (consumeIf(TokenKind::Dot))
        {
            const lexer::Token& member = current();
            expect(TokenKind::Identifier, "member name in 'nameof' argument");
            ast::AstNode* access = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MemberExpression,
                SourceRange(operand->range().begin(), member.range().end()));
            access->setFirst(operand);
            access->setText(std::string(member.lexeme()));
            operand = access;
            genericRange = join(genericRange, member.range());
            qualifiedName += ".";
            qualifiedName += member.lexeme();
        }
        parseTypeArgumentList(genericArguments);
        genericTarget = arena_.make<ast::TypeSyntax>(
            genericRange, TokenKind::Identifier, std::move(qualifiedName));
        for (ast::TypeSyntax* argument : genericArguments)
        {
            genericTarget->addTypeArgument(argument);
        }
    }
    else
    {
        operand = parseExpression();
    }

    const SourceRange end = current().range();
    expect(TokenKind::RightParen, "')' after 'nameof' argument");

    ast::AstNode* expression = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NameOfExpression, join(begin, end));
    expression->setFirst(operand);
    expression->setTypeSyntax(genericTarget);
    for (ast::TypeSyntax* argument : genericArguments)
    {
        expression->addTypeSyntax(argument);
    }

    const ast::AstNode* named = operand;
    while (named != nullptr &&
           (named->kind() == ast::AstNodeKind::CallExpression ||
            named->kind() == ast::AstNodeKind::IndexExpression))
    {
        named = named->first();
    }
    if (named != nullptr &&
        (named->kind() == ast::AstNodeKind::IdentifierExpression ||
         named->kind() == ast::AstNodeKind::MemberExpression ||
         named->kind() == ast::AstNodeKind::PointerMemberExpression ||
         named->kind() == ast::AstNodeKind::NullConditionalMemberExpression))
    {
        expression->setText(named->text());
    }
    return expression;
}

// NS304-T15-F05: `checked(expr)` / `saturating(expr)`. The policy is preserved on the node's
// auxiliaryText; the wrapped expression is the single child.
ast::AstNode* Parser::parseOverflowPolicyExpression()
{
    const lexer::Token& policyToken = consume();
    const std::string policy = std::string(policyToken.lexeme());
    expect(TokenKind::LeftParen, "'(' after '" + policy + "'");
    ast::AstNode* operand = parseExpression();
    expect(TokenKind::RightParen, "')' after '" + policy + "' operand");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::OverflowPolicyExpression,
        join(policyToken.range(), tokens_[index_ - 1U].range()));
    node->setFirst(operand);
    node->setAuxiliaryText(policy);
    return node;
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
    if ((atContextualKeyword("checked") || atContextualKeyword("saturating") ||
         atContextualKeyword("unchecked") || atContextualKeyword("wrapping")) &&
        peek().kind() == TokenKind::LeftParen)
        return parseOverflowPolicyExpression();
    const lexer::Token& token = current();
    ast::AstNodeKind kind = ast::AstNodeKind::IdentifierExpression;

    switch (token.kind())
    {
        case TokenKind::LeftBracket:
            // NS304-T09-F05: `[a, b, c]` en posición de expresión primaria es una collection
            // expression (C# 12); los atributos `[...]` viven en posición de declaración.
            return parseCollectionExpression();
        case TokenKind::Identifier: kind = ast::AstNodeKind::IdentifierExpression; break;
        case TokenKind::IntegerLiteral: kind = ast::AstNodeKind::IntegerLiteralExpression; break;
        case TokenKind::FloatingLiteral: kind = ast::AstNodeKind::FloatingLiteralExpression; break;
        case TokenKind::CharacterLiteral: kind = ast::AstNodeKind::CharacterLiteralExpression; break;
        case TokenKind::StringLiteral:
            kind = ast::AstNodeKind::StringLiteralExpression;
            break;
        case TokenKind::Utf8StringLiteral:
            // NS304-T10-F01: a typed UTF-8 literal reuses the string literal expression; the token
            // kind distinguishes its ReadOnlySpan<byte> type in Sema.
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

            // NS304-T10-F03: retain the hole expressions on the interpolation node (parsed once,
            // in source order) so a handler target can lower them one by one. This runs for both
            // the fully-static and the runtime form.
            {
                std::vector<std::string> holeKnownTypes;
                holeKnownTypes.reserve(knownTypeNames_.size());
                for (const std::string& name : knownTypeNames_) holeKnownTypes.push_back(name);
                for (const language::InterpolationPart& part : parts)
                {
                    if (part.kind != language::InterpolationPartKind::Hole) continue;
                    const std::uint32_t tokenOffset = token.range().begin().offset();
                    const std::uint64_t requestedOffset =
                        static_cast<std::uint64_t>(tokenOffset) + 2U + part.expressionOffset;
                    const std::size_t prefix =
                        requestedOffset <=
                                static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
                            ? static_cast<std::size_t>(requestedOffset)
                            : static_cast<std::size_t>(tokenOffset);
                    std::string fragment(prefix, ' ');
                    fragment += part.text;
                    source::SourceFile fragmentSource(
                        token.range().begin().fileId(), "<interpolation-hole>", std::move(fragment));
                    lexer::Lexer fragmentLexer(fragmentSource, diagnostics_);
                    const std::vector<lexer::Token> fragmentTokens = fragmentLexer.lexAll();
                    Parser fragmentParser(fragmentTokens, arena_, diagnostics_, holeKnownTypes);
                    ast::AstNode* hole = fragmentParser.parseExpression();
                    if (hole == nullptr) hole = makeErrorExpression();
                    node->addChild(hole);
                }
            }

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

}
