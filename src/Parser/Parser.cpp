#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>
#include <utility>

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

bool isInvalidVoidObjectType(const ast::TypeSyntax* type) noexcept
{
    if (type == nullptr || type->baseTokenKind() != TokenKind::KwVoid)
    {
        return false;
    }

    const auto& modifiers = type->modifiers();
    return modifiers.empty() ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::Array ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::DynamicArray;
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

std::string operatorMethodName(TokenKind kind, std::size_t arity)
{
    switch (kind)
    {
        case TokenKind::Plus: return arity == 1 ? "op_UnaryPlus" : "op_Addition";
        case TokenKind::Minus: return arity == 1 ? "op_UnaryNegation" : "op_Subtraction";
        case TokenKind::Star: return "op_Multiply";
        case TokenKind::Slash: return "op_Division";
        case TokenKind::Percent: return "op_Modulus";
        case TokenKind::Bang: return "op_LogicalNot";
        case TokenKind::Tilde: return "op_OnesComplement";
        case TokenKind::Amp: return "op_BitwiseAnd";
        case TokenKind::Pipe: return "op_BitwiseOr";
        case TokenKind::Caret: return "op_ExclusiveOr";
        case TokenKind::LessLess: return "op_LeftShift";
        case TokenKind::GreaterGreater: return "op_RightShift";
        case TokenKind::EqualEqual: return "op_Equality";
        case TokenKind::BangEqual: return "op_Inequality";
        case TokenKind::Less: return "op_LessThan";
        case TokenKind::LessEqual: return "op_LessThanOrEqual";
        case TokenKind::Greater: return "op_GreaterThan";
        case TokenKind::GreaterEqual: return "op_GreaterThanOrEqual";
        case TokenKind::PlusPlus: return "op_Increment";
        case TokenKind::MinusMinus: return "op_Decrement";
        default: return "op_Invalid";
    }
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
    return tokens_[index_ < tokens_.size() ? index_ : tokens_.size() - 1];
}

const lexer::Token& Parser::peek(std::size_t distance) const noexcept
{
    const std::size_t target = index_ + distance;
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

    return token.kind() == TokenKind::Identifier && knownTypeNames_.contains(std::string(token.lexeme()));
}

bool Parser::canStartTypeAt(std::size_t tokenIndex) const noexcept
{
    if (tokenIndex >= tokens_.size())
    {
        return false;
    }

    if (lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
    {
        ++tokenIndex;
        if (tokenIndex >= tokens_.size() ||
            lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
        {
            return false;
        }
    }

    const lexer::Token& token = tokens_[tokenIndex];
    if (lexer::isBuiltinTypeKeyword(token.kind()))
    {
        return true;
    }
    if (token.kind() != TokenKind::Identifier)
    {
        return false;
    }

    std::string candidate(token.lexeme());
    std::size_t cursor = tokenIndex + 1;
    bool qualified = false;
    while (cursor + 1 < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Dot &&
           tokens_[cursor + 1].kind() == TokenKind::Identifier)
    {
        qualified = true;
        candidate.push_back('.');
        candidate += tokens_[cursor + 1].lexeme();
        cursor += 2;
    }

    if (qualified)
    {
        return knownTypeNames_.contains(candidate);
    }
    return knownTypeNames_.contains(candidate);
}

bool Parser::looksLikeParenthesizedType() const noexcept
{
    if (!at(TokenKind::LeftParen))
    {
        return false;
    }

    std::size_t cursor = index_ + 1;
    if (cursor >= tokens_.size() || !canStartTypeAt(cursor))
    {
        return false;
    }

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
                    return false;
                }
                ++cursor;
            }

            if (depth != 0)
            {
                return false;
            }
            continue;
        }
        break;
    }

    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::RightParen;
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
        completeRange = base.range();
    }

    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        completeRange, baseKind, std::move(baseName));
    type->setOwnershipQualifier(ownershipQualifier);
    if (ownershipQualifier != ast::OwnershipQualifierSyntaxKind::None)
    {
        completeRange = join(qualifierRange, completeRange);
    }

    if (baseKind == TokenKind::Identifier && at(TokenKind::Less))
    {
        std::vector<ast::TypeSyntax*> arguments;
        if (parseTypeArgumentList(arguments))
        {
            for (ast::TypeSyntax* argument : arguments) type->addTypeArgument(argument);
            if (index_ > 0) completeRange = join(completeRange, tokens_[index_ - 1].range());
        }
    }

    while (true)
    {
        if (at(TokenKind::Star))
        {
            const lexer::Token& star = consume();
            type->addModifier(ast::TypeModifierSyntax{
                ast::TypeModifierSyntaxKind::Pointer,
                nullptr,
                star.range()});
            completeRange = join(completeRange, star.range());
            continue;
        }

        if (at(TokenKind::LeftBracket))
        {
            const SourceRange left = consume().range();
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

            ast::AstNode* count = parseExpression();
            const SourceRange beforeRight = current().range();
            expect(TokenKind::RightBracket, "']'");
            SourceRange modifierRange = join(left, beforeRight);
            if (index_ > 0 && tokens_[index_ - 1].kind() == TokenKind::RightBracket)
            {
                modifierRange = join(left, tokens_[index_ - 1].range());
            }
            type->addModifier(ast::TypeModifierSyntax{
                ast::TypeModifierSyntaxKind::Array,
                count,
                modifierRange});
            completeRange = join(completeRange, modifierRange);
            continue;
        }

        break;
    }

    type->setRange(completeRange);
    return type;
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
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "generic type parameter");
            return false;
        }
        const lexer::Token& parameter = consume();
        const std::string name(parameter.lexeme());
        if (!names.insert(name).second)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range(),
                "duplicate generic type parameter '" + name + "'");
        }
        declaration->addGenericParameter(name);
        if (!consumeIf(TokenKind::Comma)) break;
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
        clause.parameterName = std::string(parameter.lexeme());
        clause.parameterRange = parameter.range();

        const std::vector<std::string>& declared = declaration->genericParameters();
        const bool known =
            std::find(declared.begin(), declared.end(), clause.parameterName) != declared.end();
        if (!known)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::UnknownIdentifier,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range(),
                "'" + clause.parameterName +
                    "' is not a generic type parameter of this declaration");
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

        if (!expect(TokenKind::Colon, "':' after the constrained type parameter")) return false;

        std::unordered_set<std::string> written;
        bool firstConstraint = true;
        while (true)
        {
            // Las cuatro formas que esta entrega no implementa. `class` no esta en la lista: es la
            // que SI se admite, y se distingue por ser palabra clave y no identificador.
            const bool isKeywordClass = at(TokenKind::KwClass);
            const char* unsupported = nullptr;
            if (at(TokenKind::KwStruct)) unsupported = "struct";
            else if (at(TokenKind::KwNew)) unsupported = "new()";
            else if (atContextualKeyword("unmanaged")) unsupported = "unmanaged";
            else if (atContextualKeyword("notnull")) unsupported = "notnull";
            if (unsupported != nullptr)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    std::string("the '") + unsupported +
                        "' generic constraint is not supported yet; this compiler implements "
                        "'class', a base class and interfaces");
                synchronizeStatement();
                return false;
            }
            if (isKeywordClass)
            {
                const lexer::Token& keyword = consume();
                if (!firstConstraint)
                {
                    // La regla de C#, y no es cosmetica: `class` dice de que CLASE de tipo se
                    // trata, y los demas dicen de que deriva. Mezclado en medio, un lector ya no
                    // puede leer la lista de izquierda a derecha.
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        keyword.range(),
                        "'class' must be the first constraint of its 'where' clause");
                    ok = false;
                }
                if (clause.requiresClass)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        keyword.range(),
                        "duplicate 'class' constraint for '" + clause.parameterName + "'");
                    ok = false;
                }
                clause.requiresClass = true;
            }
            else
            {
                ast::TypeSyntax* constraint = parseType();
                if (constraint == nullptr) return false;
                // `where T : U` con `U` otro parametro de la MISMA declaracion. C# lo admite; este
                // compilador no, y la razon esta en la fase que valida: monomorfiza, de modo que
                // «U» ya no existe cuando se comprueba, y lo unico que quedaria por comparar seria
                // el tipo con el que se cerro U -- que es otra regla, no esta. Se rechaza aqui
                // para que la resolucion de nombres no acabe diciendo «unknown type name 'U'»,
                // que manda al lector a buscar un tipo que nunca hubo.
                if (std::find(declared.begin(), declared.end(), std::string(constraint->baseName())) !=
                    declared.end())
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        constraint->range(),
                        "'" + std::string(constraint->baseName()) +
                            "' is a generic type parameter of this declaration and cannot be used "
                            "as a constraint; this compiler implements 'class', a base class and "
                            "interfaces");
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
    while (true)
    {
        if (!canStartType(current()) && !at(TokenKind::Identifier))
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
// corchetes de un arreglo con su contador, y los `<`/`>` del anidamiento. Cualquier otra cosa
// --un operador, un literal de cadena, un parentesis, una llave-- significa que esto no era una
// lista de argumentos de tipo, y se contesta que no en vez de comprometerse y morir.
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
        // El contador de un arreglo fijo: `Foo<float[4]>(x)`.
        case TokenKind::IntegerLiteral:
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::GreaterGreater:
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
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!canAppearInTypeArgumentList(kind))
        {
            return false;
        }
        if (kind == TokenKind::Less) ++genericDepth;
        else if (kind == TokenKind::Greater || kind == TokenKind::GreaterGreater)
        {
            genericDepth -= kind == TokenKind::GreaterGreater ? 2 : 1;
            if (genericDepth == 0)
            {
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
    while (cursor < tokens_.size())
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (!canAppearInTypeArgumentList(kind))
        {
            return false;
        }
        if (kind == TokenKind::Less) ++genericDepth;
        else if (kind == TokenKind::Greater || kind == TokenKind::GreaterGreater)
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
    if (at(TokenKind::KwUsing))
    {
        return parseUsingDirective();
    }
    if (at(TokenKind::KwNamespace))
    {
        return parseNamespaceDeclaration();
    }
    return parseGlobalDeclaration();
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
    if (peek().kind() == TokenKind::Equal)
    {
        consume();
        consume();
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
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UsingAliasDirective,
            tokenRangeFrom(beginIndex));
        node->setText(std::string(first.lexeme()));
        node->setTypeSyntax(target);
        // The statement parser decides "is `X y = ...;` a declaration?" from this set, so an
        // alias name has to be in it or `DWORD d = 42u;` parses as an expression and fails. Same
        // registration a struct/class/enum name gets when it is declared.
        knownTypeNames_.insert(std::string(first.lexeme()));
        if (target->modifiers().empty() &&
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
    const std::size_t declarationBeginIndex = index_;
    TokenKind accessibility = TokenKind::Invalid;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
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

    while (true)
    {
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
        if (at(TokenKind::KwPublic) || at(TokenKind::KwInternal))
        {
            const lexer::Token& modifier = consume();
            if (accessibility != TokenKind::Invalid)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(),
                    accessibility == modifier.kind()
                        ? "duplicate accessibility modifier on top-level type declaration"
                        : "conflicting accessibility modifiers on top-level type declaration");
            }
            else
            {
                accessibility = modifier.kind();
            }
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
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "partial"));
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
             (peek().kind() == TokenKind::Identifier && peek().lexeme() == "union") ||
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
        break;
    }

    ast::AstNode* declaration = nullptr;
    if (at(TokenKind::KwDelegate))
    {
        if (isPartial || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "delegate declarations cannot be partial, abstract, or sealed in Stage 42");
        }
        declaration = parseDelegateDeclaration();
    }
    else if (at(TokenKind::KwClass))
    {
        declaration = parseClassDeclaration(isPartial, isAbstract, isSealed);
    }
    else if (at(TokenKind::KwInterface))
    {
        if (isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "interfaces are implicitly abstract and cannot use abstract/sealed type modifiers in Stage 36");
        }
        declaration = parseInterfaceDeclaration(isPartial);
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
        if (isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "abstract/sealed modifiers are supported only for class declarations in Stage 35");
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
            isBorrowedView && at(TokenKind::KwStruct));
    }
    else if (at(TokenKind::KwEnum))
    {
        if (isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "abstract/sealed modifiers are supported only for class declarations in Stage 35");
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
        if (isPartial || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[declarationBeginIndex].range(),
                "extern declarations cannot be partial, abstract, or sealed");
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
    else if (isAbstract || isSealed)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "abstract/sealed modifiers are supported only for class declarations in Stage 35");
        synchronizeGlobal();
        return nullptr;
    }
    else if (accessibility != TokenKind::Invalid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "top-level accessibility supports only public/internal class, interface, struct, enum, and delegate declarations");
        synchronizeGlobal();
        return nullptr;
    }
    else if (at(TokenKind::KwConst) || lexer::isOwnershipQualifierKeyword(current().kind()) ||
             lexer::isBuiltinTypeKeyword(current().kind()) || at(TokenKind::Identifier))
    {
        return parseFunctionOrGlobalVariable();
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

    if (declaration != nullptr)
    {
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
        if (accessibility != TokenKind::Invalid)
        {
            declaration->setTokenKind(accessibility);
        }
        if (declarationBeginIndex < tokens_.size())
        {
            declaration->setRange(join(tokens_[declarationBeginIndex].range(), declaration->range()));
        }
    }
    return declaration;
}

ast::AstNode* Parser::parseDelegateDeclaration()
{
    const std::size_t beginIndex = index_;
    consume(); // delegate

    if (!canStartType(current()))
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
    node->setAuxiliaryText("delegate");
    parseGenericParameterList(node);

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
    // NSC-LIMIT-007: `delegate void D<T>(T x) where T : class;` -- despues de la lista de
    // parametros y antes del punto y coma, que es donde el lenguaje la pone.
    parseGenericConstraintClauses(node);
    expect(TokenKind::Semicolon, "';'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::AstNode* Parser::parseStructDeclaration(StructLayoutForm form, bool isBorrowedView)
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
    if (form == StructLayoutForm::Union) node->setAuxiliaryText("union");
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
    parseGenericParameterList(node);
    // NSC-LIMIT-007: un struct no declara bases en este lenguaje, asi que la clausula va aqui,
    // entre los parametros genericos y el cuerpo.
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
        ast::AstNode* member = parseClassMember(structName, true);
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
            //   * And TWO of them must stay refused for reasons that are not about effort at all.
            //     Those two get their own message here, because "unsupported" would be a lie: with
            //     the gate lifted they COMPILE AND SILENTLY MISBEHAVE, which is worse than being
            //     rejected. Measured, both of them:
            //
            //       - a struct DESTRUCTOR emits its body and is never called. A value type has no
            //         owner and is copied freely; there is no moment this compiler can point to as
            //         "this is the last copy, run it now". RAII lives on `raii` class locals and on
            //         `owned` fields, which have exactly that moment.
            //       - `virtual`/`override`/`abstract`/`sealed` on a struct degrades to a DIRECT
            //         call. A value has no vtable pointer and no place to put one without changing
            //         its layout -- and its layout is the contract it crosses the C boundary with.
            //         Polymorphism is what classes are for.
            if (member->kind() == ast::AstNodeKind::DestructorDeclaration)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    member->range(),
                    "a struct cannot declare a destructor: a value type has no owner and is copied "
                    "freely, so there is no last copy to run it on; use a raii class, or an owned "
                    "field, for a resource with a lifetime");
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

ast::AstNode* Parser::parseClassDeclaration(bool isPartial, bool isAbstract, bool isSealed)
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
    if (isAbstract)
    {
        node->addModifier(TokenKind::KwAbstract);
    }
    if (isSealed)
    {
        node->addModifier(TokenKind::KwSealed);
    }

    if (consumeIf(TokenKind::Colon))
    {
        bool first = true;
        do
        {
            if (!canStartType(current()))
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
        ast::AstNode* member = parseClassMember(className);
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

ast::AstNode* Parser::parseInterfaceDeclaration(bool isPartial)
{
    const std::size_t beginIndex = index_;
    consume(); // interface

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

    if (consumeIf(TokenKind::Colon))
    {
        do
        {
            if (!canStartType(current()))
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


    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        if (ast::AstNode* member = parseInterfaceMember(); member != nullptr)
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

ast::AstNode* Parser::parseInterfaceMember()
{
    const std::size_t beginIndex = index_;
    TokenKind accessibility = TokenKind::KwPublic;
    bool sawPublic = false;
    bool sawAbstract = false;

    while (at(TokenKind::KwPublic) || at(TokenKind::KwAbstract))
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
        else
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
    }

    if (!canStartType(current()))
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

    if (at(TokenKind::KwThis))
    {
        return parseIndexerTail(
            returnType, accessibility, false, false, true, false, beginIndex, true);
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
        return parseMethodTail(
            returnType, name, accessibility,
            false, false, false, true, false, false, false, beginIndex);
    }
    if (at(TokenKind::LeftBrace))
    {
        return parsePropertyTail(
            returnType, name, accessibility,
            false, false, false, true, false, beginIndex, true);
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
        (atContextualKeyword("union") && peek().kind() == TokenKind::Identifier);
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
    if (modifiers.isStatic)
    {
        refuse("static", "a type is not an instance of its container; a nested type is reached "
                         "as Outer.Item whether or not anything of Outer exists");
    }
    if (modifiers.isReadonly) refuse("readonly", "a type is not storage");
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
    const bool nestedClass = at(TokenKind::KwClass);
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
        declaration = parseClassDeclaration(false, modifiers.isAbstract, modifiers.isSealed);
    }
    else if (at(TokenKind::KwInterface))
    {
        declaration = parseInterfaceDeclaration(false);
    }
    else if (at(TokenKind::KwStruct) || atContextualKeyword("union"))
    {
        declaration = parseStructDeclaration(
            at(TokenKind::KwStruct) ? StructLayoutForm::Struct : StructLayoutForm::Union, false);
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
    if (beginIndex < tokens_.size())
    {
        declaration->setRange(join(tokens_[beginIndex].range(), declaration->range()));
    }
    return declaration;
}

ast::AstNode* Parser::parseClassMember(std::string_view className, bool allowUnresolvedNamedTypes)
{
    (void)allowUnresolvedNamedTypes;
    const std::size_t beginIndex = index_;
    TokenKind accessibility = TokenKind::Invalid;
    bool isStatic = false;
    bool isReadonly = false;
    bool isConst = false;
    bool isVirtual = false;
    bool isOverride = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isExported = false;
    bool isExternC = false;
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
    // `at(N)` may appear anywhere among the modifiers -- before them, after them, or between two
    // of them -- because no ordering of `public`, `static` and `at(4)` is more correct than
    // another and a parser that accepted only one of them would be an arbitrary rule to remember.
    while (true)
    {
        parseExplicitFieldOffset();
        if (!(at(TokenKind::KwPublic) || at(TokenKind::KwInternal) ||
              at(TokenKind::KwProtected) || at(TokenKind::KwPrivate) ||
              at(TokenKind::KwStatic) || at(TokenKind::KwReadonly) || at(TokenKind::KwConst) ||
              at(TokenKind::KwVirtual) || at(TokenKind::KwOverride) ||
              at(TokenKind::KwAbstract) || at(TokenKind::KwSealed) ||
              at(TokenKind::KwExport) || at(TokenKind::KwExtern)))
        {
            break;
        }
        {
        const lexer::Token& modifier = consume();
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
                    if (!at(TokenKind::RightParen))
                    {
                        while (true)
                        {
                            ast::AstNode* parameter = parseParameter();
                            if (parameter != nullptr) imported->addChild(parameter);
                            if (!consumeIf(TokenKind::Comma)) break;
                        }
                    }
                    expect(TokenKind::RightParen, "')'");
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

    if (isVirtual && isOverride)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a method cannot be declared both virtual and override");
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
    if (isConst && (isReadonly || isStatic))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const already has static immutable semantics; do not combine const with static or readonly");
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
        if (accessibility != TokenKind::Invalid || isStatic || isReadonly || isConst ||
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
        if (isVirtual || isOverride || isStatic || isReadonly || isConst || isAbstract || isSealed ||
            isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "constructors cannot be static, readonly, const, virtual, override, abstract or sealed in Stage 35");
        }
        return parseConstructorTail(className, accessibility, beginIndex);
    }

    if (at(TokenKind::KwImplicit) || at(TokenKind::KwExplicit))
    {
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isReadonly || isConst || isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "conversion operators cannot be readonly, const, virtual, override, abstract or sealed");
        }
        const bool implicitConversion = at(TokenKind::KwImplicit);
        return parseConversionOperatorTail(
            implicitConversion, accessibility, isStatic, beginIndex);
    }

    if (at(TokenKind::KwEvent))
    {
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isReadonly || isConst || isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "Stage 42 events support accessibility and optional static only");
        }
        return parseEventTail(accessibility, isStatic, beginIndex);
    }

    // NSC-LIMIT-009. `partial` no entra en el bucle de modificadores de arriba --es contextual, y
    // alli seria indistinguible de un tipo de campo llamado `partial`-- asi que se reconoce aqui,
    // por lo que le sigue, y se consume para poder dar ESTE mensaje en vez de dejar que `partial`
    // se lea como el tipo de un campo y el error salga tres tokens mas alla.
    if (atContextualKeyword("partial") &&
        (peek().kind() == TokenKind::KwClass || peek().kind() == TokenKind::KwStruct ||
         peek().kind() == TokenKind::KwInterface || peek().kind() == TokenKind::KwEnum ||
         peek().kind() == TokenKind::KwDelegate))
    {
        const lexer::Token& modifier = consume();
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            modifier.range(),
            "a nested type cannot be partial: the parts would have to agree on a container, and "
            "this compiler aggregates partial types by namespace identity only");
    }

    // NSC-LIMIT-009. UN TIPO ANIDADO. Va antes de la guarda de abajo porque `class`, `struct`,
    // `interface`, `enum` y `delegate` no son comienzos de tipo ni identificadores, de modo que
    // caian todos en «expected an instance field or method declaration in class body» -- que es
    // literalmente lo unico que le pasaba a `class Outer { struct Item { } }`.
    if (atNestedTypeDeclaration())
    {
        return parseNestedTypeDeclaration(
            NestedTypeModifiers{
                accessibility,
                isStatic,
                isReadonly,
                isConst,
                isVirtual,
                isOverride,
                isAbstract,
                isSealed,
                isExported,
                isExternC,
                explicitFieldOffset.has_value()},
            beginIndex);
    }

    if (!canStartType(current()) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected an instance field or method declaration in class body");
        synchronizeStatement();
        return nullptr;
    }

    ast::TypeSyntax* type = parseType();

    if (at(TokenKind::KwOperator))
    {
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isReadonly || isConst || isVirtual || isOverride || isAbstract || isSealed)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "user-defined operators cannot be readonly, const, virtual, override, abstract or sealed");
        }
        return parseOperatorTail(type, accessibility, isStatic, beginIndex);
    }

    if (at(TokenKind::KwThis))
    {
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isStatic || isReadonly || isConst)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "indexers are instance members and cannot be static, readonly, or const");
        }
        return parseIndexerTail(
            type, accessibility, isVirtual, isOverride, isAbstract, isSealed, beginIndex, false);
    }

    if (!at(TokenKind::Identifier))
    {
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
        if (isReadonly || isConst)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "readonly and const modifiers are valid only on fields in Stage 37");
        }
        return parseMethodTail(
            type, name, accessibility, isStatic, isVirtual, isOverride, isAbstract, isSealed,
            isExported, isExternC, beginIndex);
    }

    if (at(TokenKind::LeftBrace))
    {
        if (isExported || isExternC)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "export extern C is valid only on ordinary static methods in Stage 47");
        }
        if (isReadonly || isConst)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                name.range(),
                "readonly and const modifiers are not property modifiers; use accessor restrictions instead");
        }
        return parsePropertyTail(
            type, name, accessibility, isStatic, isVirtual, isOverride, isAbstract, isSealed,
            beginIndex, false);
    }

    if (isExported || isExternC)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    }

    if (isVirtual || isOverride || isAbstract || isSealed)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "virtual, override, abstract and sealed modifiers are valid only on instance methods in Stage 35");
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
    // C2.2: the explicit offset rides on the field's auxiliary text, which is where
    // ProjectMemberModel reads it back. The other auxiliary-text producers for fields are
    // compiler-generated backings ("property-backing:", "event-backing:"), which never take
    // `at(N)`, so the prefixes cannot collide.
    if (explicitFieldOffset.has_value())
    {
        field->setAuxiliaryText("at:" + std::to_string(*explicitFieldOffset));
    }
    if (accessibility != TokenKind::Invalid)
    {
        field->setTokenKind(accessibility);
        field->addModifier(accessibility);
    }
    if (isStatic)
    {
        field->addModifier(TokenKind::KwStatic);
    }
    if (isReadonly)
    {
        field->addModifier(TokenKind::KwReadonly);
    }
    if (isConst)
    {
        field->addModifier(TokenKind::KwConst);
    }

    if (consumeIf(TokenKind::Equal))
    {
        // C7 "aggregate initializer": a field takes the same braced form a local does, for the
        // same reason -- it has a declared type to be filled against.
        field->setFirst(at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    }
    else if (isConst)
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
    bool isStatic,
    std::size_t beginIndex)
{
    const SourceRange eventRange = consume().range(); // event
    if (!canStartType(current()))
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

    expect(TokenKind::Semicolon, "';'");
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
    if (isVirtual) property->addModifier(TokenKind::KwVirtual);
    if (isOverride) property->addModifier(TokenKind::KwOverride);
    if (isAbstract || interfaceContext) property->addModifier(TokenKind::KwAbstract);
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
        const bool setter = at(TokenKind::KwSet);
        if (!getter && !setter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected get or set accessor in property declaration");
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
            isAbstract || interfaceContext,
            isSealed,
            std::string("property:") + (getter ? "get:" : "set:") + propertyName,
            accessorRange);
        if (setter)
        {
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, accessorRange);
            value->setText("value");
            value->setTypeSyntax(propertyType);
            accessor->addChild(value);
        }

        if (consumeIf(TokenKind::Semicolon))
        {
            if (interfaceContext || isAbstract)
            {
                // Pure contract accessor: no body.
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
            if (interfaceContext || isAbstract)
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
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    std::size_t beginIndex,
    bool interfaceContext)
{
    const SourceRange thisRange = consume().range(); // this
    ast::AstNode* indexer = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IndexerDeclaration, thisRange);
    indexer->setText("Item");
    indexer->setAuxiliaryText("indexer");
    indexer->setTypeSyntax(elementType);
    if (accessibility != TokenKind::Invalid)
    {
        indexer->setTokenKind(accessibility);
        indexer->addModifier(accessibility);
    }
    if (isVirtual) indexer->addModifier(TokenKind::KwVirtual);
    if (isOverride) indexer->addModifier(TokenKind::KwOverride);
    if (isAbstract || interfaceContext) indexer->addModifier(TokenKind::KwAbstract);
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
        const bool setter = at(TokenKind::KwSet);
        if (!getter && !setter)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected get or set accessor in indexer declaration");
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
            isAbstract || interfaceContext,
            isSealed,
            std::string("indexer:") + (getter ? "get:Item" : "set:Item"),
            accessorRange);
        for (ast::AstNode* parameter : indexParameters) accessor->addChild(parameter);
        if (setter)
        {
            ast::AstNode* value = arena_.make<ast::AstNode>(
                ast::AstNodeKind::ParameterDeclaration, accessorRange);
            value->setText("value");
            value->setTypeSyntax(elementType);
            accessor->addChild(value);
        }

        if (consumeIf(TokenKind::Semicolon))
        {
            if (!interfaceContext && !isAbstract)
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
            if (interfaceContext || isAbstract)
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
    std::size_t beginIndex)
{
    const SourceRange operatorKeywordRange = consume().range(); // operator
    if (!isStage38OperatorToken(current().kind()))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "unsupported user-defined operator token in Stage 38");
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
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "user-defined operator requires a body");
        if (!consumeIf(TokenKind::Semicolon)) synchronizeStatement();
    }
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
    if (!canStartType(current()))
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
    std::size_t beginIndex)
{
    const lexer::Token& name = consume();
    ast::AstNode* constructor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConstructorDeclaration,
        name.range());
    constructor->setText(std::string(className));
    if (accessibility != TokenKind::Invalid)
    {
        constructor->setTokenKind(accessibility);
        constructor->addModifier(accessibility);
    }

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
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
                    ast::AstNode* argument = parseExpression();
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
    bool isStatic,
    bool isVirtual,
    bool isOverride,
    bool isAbstract,
    bool isSealed,
    bool isExported,
    bool isExternC,
    std::size_t beginIndex)
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

    parseGenericParameterList(method);

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
    // NSC-LIMIT-007: `void M<T>(T x) where T : class { ... }` -- despues de la lista de
    // parametros y antes del cuerpo o del punto y coma.
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
    expectContextualKeyword("C", "'C'");

    if (at(TokenKind::KwDelegate))
    {
        ast::AstNode* delegate = parseDelegateDeclaration();
        if (delegate != nullptr)
        {
            delegate->addModifier(TokenKind::KwExtern);
            delegate->setRange(tokenRangeFrom(beginIndex));
        }
        return delegate;
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ExternCDeclaration, tokenRangeFrom(beginIndex));

    if (consumeIf(TokenKind::LeftBrace))
    {
        while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            ast::TypeSyntax* returnType = parseType();
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "external function name");
                synchronizeGlobal();
                continue;
            }
            const lexer::Token& name = consume();
            node->addChild(parseFunctionTail(returnType, name, true));
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
        return parseFunctionTail(type, name, false);
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

    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            ast::AstNode* parameter = parseParameter();
            if (parameter != nullptr)
            {
                function->addChild(parameter);
            }
            if (!consumeIf(TokenKind::Comma))
            {
                break;
            }
        }
    }
    expect(TokenKind::RightParen, "')'");
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
        expect(TokenKind::Semicolon, "';'");
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

    function->setRange(tokenRangeFrom(beginIndex));
    return function;
}

ast::AstNode* Parser::parseParameter()
{
    const std::size_t beginIndex = index_;
    // C4 "language references": `ref`/`out`/`in` before the type. The parameter's TYPE becomes a
    // pointer to what was written -- which is what a by-reference parameter actually receives,
    // and which reuses the pointer machinery C1 already certified end to end instead of adding a
    // second calling convention. Which of the three it is stays on the node, because the
    // difference between them is a matter of rules, not of representation.
    std::string passKind;
    if ((atContextualKeyword("ref") || atContextualKeyword("out") || at(TokenKind::KwIn)) &&
        (peek().kind() == TokenKind::Identifier || lexer::isBuiltinTypeKeyword(peek().kind())))
    {
        passKind = at(TokenKind::KwIn) ? "in" : std::string(current().lexeme());
        consume();
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
    if (!passKind.empty()) parameter->setAuxiliaryText("pass:" + passKind);
    return parameter;
}

ast::AstNode* Parser::parseBlock()
{
    const std::size_t beginIndex = index_;
    expect(TokenKind::LeftBrace, "'{'");
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, tokenRangeFrom(beginIndex));

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* statement = parseStatement();
        if (statement != nullptr)
        {
            block->addChild(statement);
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
    if (at(TokenKind::KwIf)) return parseIfStatement();
    if (at(TokenKind::KwWhile)) return parseWhileStatement();
    if (at(TokenKind::KwDo)) return parseDoWhileStatement();
    if (at(TokenKind::KwFor)) return parseForStatement();
    if (at(TokenKind::KwForeach)) return parseForeachStatement();
    if (at(TokenKind::KwSwitch)) return parseSwitchStatement();

    if (at(TokenKind::KwBreak) || at(TokenKind::KwContinue))
    {
        const std::size_t beginIndex = index_;
        const TokenKind kind = consume().kind();
        expect(TokenKind::Semicolon, "';'");
        return arena_.make<ast::AstNode>(
            kind == TokenKind::KwBreak ? ast::AstNodeKind::BreakStatement : ast::AstNodeKind::ContinueStatement,
            tokenRangeFrom(beginIndex));
    }

    if (at(TokenKind::KwReturn)) return parseReturnStatement();
    if (at(TokenKind::KwTry)) return parseTryStatement();
    if (at(TokenKind::KwThrow)) return parseThrowStatement();
    if (at(TokenKind::KwUsing)) return parseUsingStatement();

    // ID 131, la otra mitad. Al principio de una sentencia, `Caja<int>` puede empezar DOS cosas:
    // una declaracion (`Caja<int> c = ...`) o una expresion (`Caja<int>.Cuenta = 5;`). Las
    // distingue lo que sigue al `>` de cierre: un identificador en la primera, un `.` en la
    // segunda -- y `looksLikeClosedGenericTypeReference` exige exactamente `.` mas identificador,
    // que no puede empezar ninguna declaracion.
    //
    // Sin esto, `canStartTypeAt` decia "declaracion", `parseVariableDeclaration` leia el tipo y
    // pedia un nombre de variable, y el error era "expected variable name, found '.'": un
    // diagnostico que no menciona ni genericos ni miembros estaticos y manda a mirar donde no es.
    if (at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVar) ||
        (canStartTypeAt(index_) && !looksLikeClosedGenericTypeReference()))
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

ast::AstNode* Parser::parseVariableDeclaration(bool expectSemicolon)
{
    const std::size_t beginIndex = index_;
    const bool isRaii = consumeIf(TokenKind::KwRaii);
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
        const lexer::Token& varToken = consume();
        type = arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        type = parseType();
    }

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "variable name");
        synchronizeStatement();
        return nullptr;
    }

    const lexer::Token& name = consume();
    ast::AstNode* variable = arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement,
        name.range());
    variable->setTypeSyntax(type);
    variable->setText(std::string(name.lexeme()));
    variable->setFlag(isConst);
    if (isRaii) variable->addModifier(TokenKind::KwRaii);

    if (consumeIf(TokenKind::Equal))
    {
        // C7 "aggregate initializer": `V v = {1, 2};`. A brace can only start an initializer
        // here, so there is nothing to disambiguate against a block statement.
        variable->setFirst(
            at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    }
    else if (isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const declaration requires an initializer");
    }

    if (expectSemicolon)
    {
        expect(TokenKind::Semicolon, "';'");
    }
    variable->setRange(tokenRangeFrom(beginIndex));
    return variable;
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
        ast::AstNode* element =
            at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression();
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

ast::AstNode* Parser::parseIfStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* thenBlock = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (thenBlock == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after if condition");
    }

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
            expect(TokenKind::LeftBrace, "'{' or 'if' after else");
        }
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IfStatement, tokenRangeFrom(beginIndex));
    node->setFirst(condition);
    node->setSecond(thenBlock);
    node->setThird(elseNode);
    return node;
}

ast::AstNode* Parser::parseWhileStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        expect(TokenKind::LeftBrace, "'{' after while condition");
    }

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::WhileStatement, tokenRangeFrom(beginIndex));
    node->setFirst(condition);
    node->setSecond(body);
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
    ast::AstNode* governing = parseExpression();
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
                section->addChild(statement);
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
    return node;
}

ast::AstNode* Parser::parseForStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");

    ast::AstNode* initializer = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        if (at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVar) || canStartTypeAt(index_))
        {
            initializer = parseVariableDeclaration(false);
        }
        else
        {
            initializer = parseExpression();
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
        increment = parseExpression();
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
// NativeSharp has no `IEnumerable<T>`/`IEnumerator<T>` -- interfaces in this compiler cannot
// carry type parameters at all (parseInterfaceDeclaration never calls parseGenericParameterList
// the way parseClassDeclaration does), and even if they could, a *non*-generic IEnumerator would
// need `object Current { get; }` to stay uniform across element types, which NativeSharp cannot
// express either (no universal `object` base type -- see Comparer.ns's class comment for the
// same constraint blocking `Comparer<T>.Default`). So `foreach` here is implemented exactly the
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
// The enumerator candidate declares its hidden enumerator local as `raii var`, not a plain
// `var` -- this makes its destructor run automatically at the end of the synthesized block via
// the SAME RAII machinery every other `raii` local already relies on, which is this compiler's
// direct, idiomatic analogue of real C#'s implicit `Dispose()` call in foreach's generated
// `finally` block (via `IDisposable`) -- without needing `IDisposable`/`using` machinery here at
// all, since RAII destruction already gives an equivalent guarantee (called once, on every exit
// path out of the loop, including `break`/`return`/an exception unwinding through the body).
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
    consume(); // foreach
    expect(TokenKind::LeftParen, "'('");

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

    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "foreach element name");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token& elementNameToken = consume();
    const std::string elementName(elementNameToken.lexeme());

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

    // first(): `var $__foreach_source = <collection>;` -- evaluates the collection expression
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
    ast::AstNode* sourceDecl = makeVariableDeclaration(varMarker(), "$__foreach_source", collection, false);

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
            intType, "$__foreach_index", makeIntegerLiteralExpression(range, 0), false);

        ast::AstNode* condition = makeBinaryExpression(
            TokenKind::Less,
            makeIdentifierExpression(range, "$__foreach_index"),
            makeMemberExpression(makeIdentifierExpression(range, "$__foreach_source"), "Length"));

        ast::AstNode* increment = makeAssignmentExpression(
            makeIdentifierExpression(range, "$__foreach_index"),
            makeBinaryExpression(
                TokenKind::Plus,
                makeIdentifierExpression(range, "$__foreach_index"),
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
                makeIdentifierExpression(range, "$__foreach_source"),
                makeIdentifierExpression(range, "$__foreach_index")),
            elementIsRaii,
            /*isConst=*/true);

        ast::AstNode* forBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        forBody->addChild(body);

        ast::AstNode* forNode = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, range);
        forNode->setFirst(indexDecl);
        forNode->setSecond(condition);
        forNode->setThird(increment);
        forNode->addChild(forBody);

        arrayCandidate = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        arrayCandidate->addChild(forNode);
    }

    // third(): the enumerator-pattern candidate.
    //   raii var $__foreach_enumerator = $__foreach_source.GetEnumerator();
    //   while ($__foreach_enumerator.MoveNext()) {
    //       <ElementType> <name> = $__foreach_enumerator.Current;
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
            makeMemberExpression(makeIdentifierExpression(range, "$__foreach_source"), "GetEnumerator"));
        ast::AstNode* enumeratorDecl = makeVariableDeclaration(
            varMarker(), "$__foreach_enumerator", getEnumeratorCall, true);

        ast::AstNode* condition = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, "$__foreach_enumerator"), "MoveNext"));

        // isConst=true here too (P0.3) -- both candidates must agree on the element's
        // read-only-ness, since exactly one of them is the one actually analyzed/lowered and the
        // other is discarded; the user-visible semantics must not depend on which was chosen.
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeMemberExpression(makeIdentifierExpression(range, "$__foreach_enumerator"), "Current"),
            elementIsRaii,
            /*isConst=*/true);

        ast::AstNode* whileBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        whileBody->addChild(elementDecl);
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
    return node;
}

ast::AstNode* Parser::parseReturnStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    ast::AstNode* value = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        value = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ReturnStatement, tokenRangeFrom(beginIndex));
    node->setFirst(value);
    return node;
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

    if (consumeIf(TokenKind::KwFinally))
    {
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
        if (canStartType(current()))
        {
            clause->setTypeSyntax(parseType());
        }
        else
        {
            expect(TokenKind::Identifier, "exception type");
        }

        if (at(TokenKind::Identifier))
        {
            clause->setText(std::string(consume().lexeme()));
        }
        else
        {
            expect(TokenKind::Identifier, "exception variable name");
        }

        expect(TokenKind::RightParen, "')' after catch declaration");
    }
    else
    {
        clause->setFlag(true);
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
    if (canStartType(current()))
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
    ast::AstNode* condition = parseLogicalOrExpression();
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

bool Parser::looksLikeLambdaExpression() const noexcept
{
    if (at(TokenKind::Identifier))
    {
        return index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind() == TokenKind::FatArrow;
    }
    if (!at(TokenKind::LeftParen)) return false;

    std::size_t cursor = index_ + 1;
    if (cursor >= tokens_.size()) return false;
    if (tokens_[cursor].kind() == TokenKind::RightParen)
    {
        return cursor + 1 < tokens_.size() && tokens_[cursor + 1].kind() == TokenKind::FatArrow;
    }
    while (cursor < tokens_.size())
    {
        if (tokens_[cursor].kind() != TokenKind::Identifier) return false;
        ++cursor;
        if (cursor >= tokens_.size()) return false;
        if (tokens_[cursor].kind() == TokenKind::Comma)
        {
            ++cursor;
            continue;
        }
        if (tokens_[cursor].kind() != TokenKind::RightParen) return false;
        return cursor + 1 < tokens_.size() && tokens_[cursor + 1].kind() == TokenKind::FatArrow;
    }
    return false;
}

ast::AstNode* Parser::parseLambdaExpression()
{
    const std::size_t beginIndex = index_;
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
                const lexer::Token& parameter = current();
                expect(TokenKind::Identifier, "lambda parameter name");
                auto* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ParameterDeclaration, parameter.range());
                node->setText(std::string(parameter.lexeme()));
                parameters.push_back(node);
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')'");
    }
    expect(TokenKind::FatArrow, "'=>'");

    auto* lambda = arena_.make<ast::AstNode>(ast::AstNodeKind::LambdaExpression, tokenRangeFrom(beginIndex));
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

    if (at(TokenKind::KwSizeof))
    {
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
                    ast::AstNodeKind::SizeofTypeExpression,
                    tokenRangeFrom(checkpoint));
                node->setTypeSyntax(type);
                node->setRange(join(start, tokens_[index_ - 1].range()));
                return node;
            }
        }

        ast::AstNode* expression = parseExpression();
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::SizeofExpression,
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
                    creation->addChild(parseAssignmentExpression());
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
        if (consumeIf(TokenKind::LeftBracket))
        {
            node->setFlag(true);
            node->setFirst(parseExpression());
            expect(TokenKind::RightBracket, "']'");
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
        if (looksLikeGenericCallTypeArguments())
        {
            parseTypeArgumentList(genericCallArguments);
        }

        if (consumeIf(TokenKind::LeftParen))
        {
            ast::AstNode* call = arena_.make<ast::AstNode>(ast::AstNodeKind::CallExpression, expression->range());
            call->setFirst(expression);
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
                    const bool refArgument = (atContextualKeyword("ref") || atContextualKeyword("out") ||
                        at(TokenKind::KwIn)) && peek().kind() != TokenKind::Comma &&
                        peek().kind() != TokenKind::RightParen;
                    if (refArgument)
                    {
                        const lexer::Token& keyword = consume();
                        const std::string spelling =
                            keyword.kind() == TokenKind::KwIn ? "in" : std::string(keyword.lexeme());
                        ast::AstNode* operand = parseAssignmentExpression();
                        ast::AstNode* address = arena_.make<ast::AstNode>(
                            ast::AstNodeKind::UnaryExpression, keyword.range());
                        address->setTokenKind(TokenKind::Amp);
                        address->setFirst(operand);
                        address->setAuxiliaryText("pass:" + spelling);
                        if (operand != nullptr)
                        {
                            address->setRange(join(keyword.range(), operand->range()));
                        }
                        call->addChild(address);
                    }
                    else
                    {
                        call->addChild(parseAssignmentExpression());
                    }
                    if (!consumeIf(TokenKind::Comma))
                    {
                        break;
                    }
                }
            }
            expect(TokenKind::RightParen, "')'");
            call->setRange(SourceRange(expression->range().begin(), tokens_[index_ - 1].range().end()));
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
                    ast::AstNode* indexExpression = parseAssignmentExpression();
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

        break;
    }

    return expression;
}

ast::AstNode* Parser::parsePrimaryExpression()
{
    const lexer::Token& token = current();
    ast::AstNodeKind kind = ast::AstNodeKind::IdentifierExpression;

    switch (token.kind())
    {
        case TokenKind::Identifier: kind = ast::AstNodeKind::IdentifierExpression; break;
        case TokenKind::IntegerLiteral: kind = ast::AstNodeKind::IntegerLiteralExpression; break;
        case TokenKind::FloatingLiteral: kind = ast::AstNodeKind::FloatingLiteralExpression; break;
        case TokenKind::CharacterLiteral: kind = ast::AstNodeKind::CharacterLiteralExpression; break;
        case TokenKind::StringLiteral: kind = ast::AstNodeKind::StringLiteralExpression; break;
        case TokenKind::KwTrue:
        case TokenKind::KwFalse: kind = ast::AstNodeKind::BooleanLiteralExpression; break;
        case TokenKind::KwNull: kind = ast::AstNodeKind::NullLiteralExpression; break;
        case TokenKind::KwThis: kind = ast::AstNodeKind::ThisExpression; break;
        case TokenKind::KwBase: kind = ast::AstNodeKind::BaseExpression; break;
        case TokenKind::LeftParen:
        {
            consume();
            ast::AstNode* expression = parseExpression();
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
    ast::AstNode* node = arena_.make<ast::AstNode>(kind, token.range());
    node->setTokenKind(token.kind());
    node->setText(std::string(token.lexeme()));

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
            at(TokenKind::KwPublic) || at(TokenKind::KwInternal) || at(TokenKind::KwConst) || canStartType(current()))
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
