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
    return parseWithContinuation(ContinuationRequestKind::Block);
}

ast::AstNode* Parser::parseStatement()
{
    return parseWithContinuation(ContinuationRequestKind::Statement);
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
    return parseWithContinuation(ContinuationRequestKind::Expression);
}
}
