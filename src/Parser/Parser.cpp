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

ast::AstNode* Parser::parseBlock()
{
    return parseBlockImpl();
}

ast::AstNode* Parser::parseStatement()
{
    return parseStatementImpl();
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

ast::AstNode* Parser::parseExpression()
{
    return parseExpressionImpl();
}
}
