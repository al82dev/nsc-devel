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

ast::AstNode* Parser::parseExpression()
{
    return parseExpressionImpl();
}
}
