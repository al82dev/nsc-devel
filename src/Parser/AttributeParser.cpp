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
#include <variant>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
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
    GlobalDeclarationAttributeState state;
    state.declarationBeginIndex = index_;
    parseGlobalDeclarationPrefixes(state);
    state.isAsyncMethod = false;
    if (atContextualKeyword("async"))
    {
        // NS304-T05-F01: se consume (ya no se rechaza con NS3049); el marcador se baja al callable.
        state.isAsyncMethod = true;
        consume();
    }
    ast::AstNode* declaration = dispatchGlobalDeclaration(state);
    if (state.returnDirectly) return declaration;
    applyGlobalFunctionAttributes(declaration, state);
    applyGlobalIdentityAttributes(declaration, state);
    applyGlobalLayoutAttributes(declaration, state);
    finishGlobalDeclarationAttributes(declaration, state);
    if (state.isAsyncMethod && declaration != nullptr &&
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

void Parser::parseGlobalDeclarationPrefixes(GlobalDeclarationAttributeState& state)
{
    while (tryParseGlobalRepresentationPrefix(state) ||
           tryParseGlobalLayoutPrefix(state) ||
           tryParseGlobalFunctionPrefix(state) ||
           tryParseGlobalValueTypePrefix(state) ||
           tryParseGlobalInheritancePrefix(state))
    {
    }
}

bool Parser::tryParseGlobalRepresentationPrefix(GlobalDeclarationAttributeState& state)
{
    auto& representation = state.representation;
    auto& isUnsafeTraitContract = state.isUnsafeTraitContract;
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
        return true;
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
        return true;
    }
    return false;
}

bool Parser::tryParseGlobalLayoutPrefix(GlobalDeclarationAttributeState& state)
{
    auto& layoutPack = state.layoutPack;
    auto& layoutAlign = state.layoutAlign;
    auto& layoutSize = state.layoutSize;
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
        return true;
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
        return true;
    }
    return false;
}

bool Parser::tryParseGlobalFunctionPrefix(GlobalDeclarationAttributeState& state)
{
    auto& isExportedFunction = state.isExportedFunction;
    auto& isInlineFunction = state.isInlineFunction;
    auto& isForceInlineFunction = state.isForceInlineFunction;
    auto& isNoInlineFunction = state.isNoInlineFunction;
    auto& accessibility = state.accessibility;
    auto& granularVisibility = state.granularVisibility;
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
        return true;
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
        return true;
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
        return true;
    }
    return false;
}

bool Parser::tryParseGlobalValueTypePrefix(GlobalDeclarationAttributeState& state)
{
    auto& isRecordStruct = state.isRecordStruct;
    auto& isCopyType = state.isCopyType;
    auto& isReadonlyType = state.isReadonlyType;
    auto& isPartial = state.isPartial;
    auto& isBorrowedView = state.isBorrowedView;
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
        return true;
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
        return true;
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
        return true;
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
        return true;
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
        return true;
    }
    return false;
}

bool Parser::tryParseGlobalInheritancePrefix(GlobalDeclarationAttributeState& state)
{
    auto& isAbstract = state.isAbstract;
    auto& isSealed = state.isSealed;
    auto& isStatic = state.isStatic;
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
        return true;
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
        return true;
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
        return true;
    }
    return false;
}


ast::AstNode* Parser::dispatchGlobalDeclaration(GlobalDeclarationAttributeState& state)
{
    ast::AstNode* declaration = nullptr;
    if (tryDispatchReferenceGlobal(state, declaration) ||
        tryDispatchValueGlobal(state, declaration) ||
        tryDispatchExternGlobal(state, declaration))
        return declaration;
    return dispatchGlobalFallback(state);
}

bool Parser::tryDispatchReferenceGlobal(
    GlobalDeclarationAttributeState& state,
    ast::AstNode*& declaration)
{
    const std::size_t declarationBeginIndex = state.declarationBeginIndex;
    const auto accessibility = state.accessibility;
    const bool isPartial = state.isPartial;
    const bool isAbstract = state.isAbstract;
    const bool isSealed = state.isSealed;
    const bool isStatic = state.isStatic;
    const bool isReadonlyType = state.isReadonlyType;
    const bool isRecordStruct = state.isRecordStruct;
    const bool isBorrowedView = state.isBorrowedView;
    const bool isCopyType = state.isCopyType;
    const auto& layoutPack = state.layoutPack;
    const auto& layoutAlign = state.layoutAlign;
    const auto& layoutSize = state.layoutSize;
    const auto& representation = state.representation;
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
    else return false;
    return true;
}

bool Parser::tryDispatchValueGlobal(
    GlobalDeclarationAttributeState& state,
    ast::AstNode*& declaration)
{
    const std::size_t declarationBeginIndex = state.declarationBeginIndex;
    const bool isPartial = state.isPartial;
    const bool isAbstract = state.isAbstract;
    const bool isSealed = state.isSealed;
    const bool isStatic = state.isStatic;
    const bool isReadonlyType = state.isReadonlyType;
    const bool isRecordStruct = state.isRecordStruct;
    const bool isBorrowedView = state.isBorrowedView;
    const bool isCopyType = state.isCopyType;
    auto& layoutPack = state.layoutPack;
    auto& layoutAlign = state.layoutAlign;
    auto& layoutSize = state.layoutSize;
    if (atContextualKeyword("variant") && peek().kind() == TokenKind::Identifier)
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
    else return false;
    return true;
}

bool Parser::tryDispatchExternGlobal(
    GlobalDeclarationAttributeState& state,
    ast::AstNode*& declaration)
{
    const std::size_t declarationBeginIndex = state.declarationBeginIndex;
    const auto accessibility = state.accessibility;
    const bool isPartial = state.isPartial;
    const bool isAbstract = state.isAbstract;
    const bool isSealed = state.isSealed;
    const bool isStatic = state.isStatic;
    if (at(TokenKind::KwExtern))
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
            state.returnDirectly = true;
            declaration = externDeclaration;
            return true;
        }
    }
    else return false;
    return true;
}

ast::AstNode* Parser::dispatchGlobalFallback(GlobalDeclarationAttributeState& state)
{
    const auto accessibility = state.accessibility;
    const bool isPartial = state.isPartial;
    const bool isAbstract = state.isAbstract;
    const bool isSealed = state.isSealed;
    const bool isStatic = state.isStatic;
    const bool isReadonlyType = state.isReadonlyType;
    ast::AstNode* declaration = nullptr;
    if (isPartial)
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
    return declaration;
}

void Parser::applyGlobalFunctionAttributes(
    ast::AstNode* declaration,
    const GlobalDeclarationAttributeState& state)
{
    const bool isExportedFunction = state.isExportedFunction;
    const bool isInlineFunction = state.isInlineFunction;
    const bool isForceInlineFunction = state.isForceInlineFunction;
    const bool isNoInlineFunction = state.isNoInlineFunction;

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
}

void Parser::applyGlobalIdentityAttributes(
    ast::AstNode* declaration,
    const GlobalDeclarationAttributeState& state)
{
    if (declaration == nullptr) return;
    const bool isUnsafeTraitContract = state.isUnsafeTraitContract;
    const auto& granularVisibility = state.granularVisibility;
    const bool isCopyType = state.isCopyType;
    const bool isBorrowedView = state.isBorrowedView;
    const bool isReadonlyType = state.isReadonlyType;

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
}

void Parser::applyGlobalLayoutAttributes(
    ast::AstNode* declaration,
    const GlobalDeclarationAttributeState& state)
{
    if (declaration == nullptr) return;
    const auto& layoutPack = state.layoutPack;
    const auto& layoutAlign = state.layoutAlign;
    const auto& layoutSize = state.layoutSize;
    const auto& representation = state.representation;

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
}

void Parser::finishGlobalDeclarationAttributes(
    ast::AstNode* declaration,
    const GlobalDeclarationAttributeState& state)
{
    if (declaration == nullptr) return;
    const auto accessibility = state.accessibility;
    const std::size_t declarationBeginIndex = state.declarationBeginIndex;

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


void Parser::applyNestedTypeLayout(
    ast::AstNode* declaration,
    const NestedTypeModifiers& modifiers)
{
    if (!modifiers.layoutPack.has_value() &&
        !modifiers.layoutAlign.has_value() &&
        !modifiers.layoutSize.has_value())
        return;
    if (declaration->kind() != ast::AstNodeKind::StructDeclaration)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "pack/align/size layout modifiers apply only to nested struct and union declarations");
        return;
    }
    if (declaration->auxiliaryText().find("variant") != std::string::npos)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "pack/align/size cannot override the canonical tagged-variant layout");
        return;
    }
    std::string layout = declaration->auxiliaryText();
    const auto append = [&](std::string_view name, std::optional<std::uint64_t> value)
    {
        if (!value.has_value()) return;
        layout += (layout.empty() ? "" : ",");
        layout += std::string(name) + "=" + std::to_string(*value);
    };
    append("pack", modifiers.layoutPack);
    append("align", modifiers.layoutAlign);
    append("size", modifiers.layoutSize);
    declaration->setAuxiliaryText(std::move(layout));
}


void Parser::parseMemberFrameStorage(MemberParseState& state)
{
    parseFrameStorageAttribute(state.frameStorage);
}

bool Parser::parseFrameStorageAttribute(std::string& frameStorage)
{
    if (!at(TokenKind::LeftBracket)) return false;
    consume();
    if (at(TokenKind::Identifier) && current().lexeme() == "FrameStorage")
    {
        consume();
        expect(TokenKind::LeftParen, "'(' after FrameStorage");
        if (at(TokenKind::Identifier))
            frameStorage = std::string(consume().lexeme());
        else
            expect(TokenKind::Identifier, "FrameStorage argument (Stack, Arena or Heap)");
        expect(TokenKind::RightParen, "')' after FrameStorage argument");
        expect(TokenKind::RightBracket, "']' after FrameStorage");
        return true;
    }
    --index_;
    return false;
}

void Parser::applyNamespaceCallableAttributes(
    ast::AstNode* declaration,
    bool isAsync,
    const std::string& frameStorage)
{
    if (!isAsync || declaration == nullptr ||
        declaration->kind() != ast::AstNodeKind::FunctionDeclaration)
        return;
    std::string marker = declaration->auxiliaryText();
    if (!marker.empty()) marker += ';';
    marker += "async";
    if (!frameStorage.empty())
    {
        marker += ";framestorage:";
        marker += frameStorage;
    }
    declaration->setAuxiliaryText(std::move(marker));
}


void Parser::parseExplicitFieldOffsets(MemberParseState& state)
{
    while (atContextualKeyword("at") && peek().kind() == TokenKind::LeftParen)
    {
        const lexer::Token& keyword = consume();
        consume();
        if (at(TokenKind::IntegerLiteral) &&
            std::holds_alternative<std::uint64_t>(current().value()))
        {
            state.explicitFieldOffset = std::get<std::uint64_t>(current().value());
            consume();
        }
        else expect(TokenKind::IntegerLiteral, "field byte offset");
        expect(TokenKind::RightParen, "')'");
        if (state.explicitFieldOffset.has_value() &&
            *state.explicitFieldOffset > 0xFFFFFFULL)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                keyword.range(),
                "explicit field offset is out of range");
            state.explicitFieldOffset.reset();
        }
    }
}


bool Parser::tryParseNestedLayoutModifier(MemberParseState& state)
{
    const bool isPack = atContextualKeyword("pack");
    const bool isAlign = atContextualKeyword("align");
    if ((!isPack && !isAlign && !atContextualKeyword("size")) ||
        peek().kind() != TokenKind::LeftParen)
        return false;
    const lexer::Token& keyword = consume();
    consume();
    std::optional<std::uint64_t> value;
    if (at(TokenKind::IntegerLiteral) &&
        std::holds_alternative<std::uint64_t>(current().value()))
    {
        value = std::get<std::uint64_t>(current().value());
        consume();
    }
    else expect(TokenKind::IntegerLiteral,
        isPack ? "pack value" : isAlign ? "alignment value" : "size value");
    expect(TokenKind::RightParen, "')'");
    if (value.has_value() && ((isPack || isAlign)
            ? (*value == 0 || (*value & (*value - 1)) != 0) : (*value == 0)))
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
    if (isPack) state.nestedLayoutPack = value;
    else if (isAlign) state.nestedLayoutAlign = value;
    else state.nestedLayoutSize = value;
    return true;
}


void Parser::parseLambdaCaptureOrAttribute(
    std::string& captureContract,
    std::string& attributeContract)
{
    if (!consumeIf(TokenKind::LeftBracket)) return;
    if (!at(TokenKind::RightBracket) && !atContextualKeyword("copy") &&
        !atContextualKeyword("move") && !at(TokenKind::KwMove) &&
        !atContextualKeyword("ref"))
    {
        std::string name;
        if (at(TokenKind::Identifier)) name = std::string(consume().lexeme());
        expect(TokenKind::RightBracket, "']' after lambda attribute");
        attributeContract = "attribute:" + name;
        return;
    }
    parseLambdaCaptureList(captureContract);
}

}
