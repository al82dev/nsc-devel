#include "nativesharp/Parser/Parser.h"
#include "KnownGenericTypeScope.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
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

ast::AstNode* Parser::parseInterfaceMemberImpl(bool isTrait)
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

    parseInterfaceMemberModifiers(
        isTrait, sawPublic, sawAbstract, sawNoHeap, sawStatic);

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

    InterfaceMemberState state{
        accessibility, sawPublic, sawAbstract, sawNoHeap, sawStatic};
    return parseInterfaceTypedMember(
        isTrait, beginIndex, state, returnType, isReferenceReturn,
        std::move(returnLifetime));
}

void Parser::parseInterfaceMemberModifiers(
    bool isTrait,
    bool& sawPublic,
    bool& sawAbstract,
    bool& sawNoHeap,
    bool& sawStatic)
{
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
}

ast::AstNode* Parser::parseInterfaceTypedMember(
    bool isTrait,
    std::size_t beginIndex,
    InterfaceMemberState& state,
    ast::TypeSyntax* returnType,
    bool isReferenceReturn,
    std::string returnLifetime)
{
    auto& accessibility = state.accessibility;
    auto& sawAbstract = state.sawAbstract;
    if (at(TokenKind::KwOperator))
        return parseInterfaceOperatorMember(
            isTrait, beginIndex, state, returnType);
    if (at(TokenKind::KwThis))
    {
        return parseIndexerTail(
            returnType, accessibility, false, false, false, sawAbstract, false,
            isReferenceReturn, beginIndex, true, false);
    }

    return parseInterfaceNamedMember(
        isTrait, beginIndex, state, returnType, isReferenceReturn,
        std::move(returnLifetime));
}

ast::AstNode* Parser::parseInterfaceOperatorMember(
    bool isTrait,
    std::size_t beginIndex,
    const InterfaceMemberState& state,
    ast::TypeSyntax* returnType)
{
    const TokenKind accessibility = state.accessibility;
    const bool sawStatic = state.sawStatic;
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
    return nullptr;
}

ast::AstNode* Parser::parseInterfaceNamedMember(
    bool isTrait,
    std::size_t beginIndex,
    const InterfaceMemberState& state,
    ast::TypeSyntax* returnType,
    bool isReferenceReturn,
    std::string returnLifetime)
{
    const TokenKind accessibility = state.accessibility;
    const bool sawAbstract = state.sawAbstract;
    const bool sawNoHeap = state.sawNoHeap;
    const bool sawStatic = state.sawStatic;
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
            false, sawStatic, false, false, sawAbstract, false, beginIndex, true, false, false);
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        name.range(),
        "Stage 38 interfaces do not contain fields; expected method, property, or indexer contract");
    synchronizeStatement();
    return nullptr;
}

ast::AstNode* Parser::parseNestedTypeDeclarationImpl(
    const NestedTypeModifiers& modifiers,
    std::size_t beginIndex)
{
    const bool nestedReadonlyStruct =
        validateNestedTypeModifiers(modifiers, beginIndex);
    ast::AstNode* declaration = createNestedTypeDeclaration(modifiers);
    if (declaration == nullptr) return nullptr;
    applyNestedTypeIdentity(
        declaration, modifiers, nestedReadonlyStruct, beginIndex);
    applyNestedTypeLayout(declaration, modifiers);
    if (beginIndex < tokens_.size())
        declaration->setRange(join(tokens_[beginIndex].range(), declaration->range()));
    return declaration;
}

void Parser::refuseNestedTypeModifier(
    std::size_t beginIndex,
    const char* modifierName,
    const char* reason)
{
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        tokens_[beginIndex].range(),
        std::string("'") + modifierName +
            "' is not valid on a nested type declaration: " + reason);
}

bool Parser::validateNestedTypeModifiers(
    const NestedTypeModifiers& modifiers,
    std::size_t beginIndex)
{
    const bool nestedClass = at(TokenKind::KwClass);
    const bool nestedRecordStruct =
        atContextualKeyword("record") && peek().kind() == TokenKind::KwStruct;
    const bool nestedStruct = at(TokenKind::KwStruct) || nestedRecordStruct;
    const bool readonlyStruct = modifiers.isReadonly && nestedStruct;
    const auto refuse = [&](const char* name, const char* reason)
    {
        refuseNestedTypeModifier(beginIndex, name, reason);
    };
    if (modifiers.isPartial && !nestedClass)
        refuse("partial", "only nested classes can be split into materialization parts");
    if (modifiers.isStatic && !nestedClass)
        refuse("static", "only a nested class can be static");
    if (modifiers.isReadonly && !readonlyStruct)
        refuse("readonly", "only a non-union struct can be readonly");
    if (modifiers.isConst) refuse("const", "a type is not a value");
    if (modifiers.isVirtual) refuse("virtual", "a type is not dispatched");
    if (modifiers.isOverride) refuse("override", "a type does not override anything");
    if (modifiers.isExported || modifiers.isExternC)
        refuse("export extern C", "the host C ABI exports functions, not types");
    if (modifiers.hasExplicitFieldOffset)
        refuse("at(N)", "a type has no offset inside its container; only a field does");
    if ((modifiers.isAbstract || modifiers.isSealed) && !nestedClass)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[beginIndex].range(),
            "abstract/sealed modifiers are supported only for class declarations");
    return readonlyStruct;
}

ast::AstNode* Parser::createNestedTypeDeclaration(
    const NestedTypeModifiers& modifiers)
{
    const bool record =
        atContextualKeyword("record") && peek().kind() == TokenKind::KwStruct;
    if (at(TokenKind::KwClass))
        return parseClassDeclaration(
            modifiers.isPartial, modifiers.isAbstract,
            modifiers.isSealed, modifiers.isStatic);
    if (at(TokenKind::KwInterface)) return parseInterfaceDeclaration(false);
    if (atContextualKeyword("variant")) return parseVariantDeclaration();
    if (at(TokenKind::KwStruct) || record || atContextualKeyword("union"))
    {
        if (record) consume();
        return parseStructDeclaration(
            at(TokenKind::KwStruct) ? StructLayoutForm::Struct : StructLayoutForm::Union,
            modifiers.isBorrowedView && at(TokenKind::KwStruct),
            record,
            modifiers.isCopyType);
    }
    if (at(TokenKind::KwEnum)) return parseEnumDeclaration();
    return parseDelegateDeclaration();
}

void Parser::applyNestedTypeIdentity(
    ast::AstNode* declaration,
    const NestedTypeModifiers& modifiers,
    bool nestedReadonlyStruct,
    std::size_t beginIndex)
{
    if (modifiers.isCopyType &&
        declaration->kind() != ast::AstNodeKind::StructDeclaration)
        refuseNestedTypeModifier(
            beginIndex, "copy",
            "only nested structs, records, and variants can opt into Copy");
    else if (modifiers.isCopyType &&
             declaration->auxiliaryText().find("copy") == std::string::npos)
    {
        std::string markers = declaration->auxiliaryText();
        markers += (markers.empty() ? "" : ",");
        markers += "copy";
        declaration->setAuxiliaryText(std::move(markers));
    }
    if (!declaration->genericParameters().empty())
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a nested type cannot declare generic parameters yet; declare '" +
                declaration->text() + "' at namespace scope");
    if (modifiers.accessibility != TokenKind::Invalid)
        declaration->setTokenKind(modifiers.accessibility);
    if (modifiers.isNew) declaration->addModifier(TokenKind::KwNew);
    if (!nestedReadonlyStruct) return;
    declaration->addModifier(TokenKind::KwReadonly);
    for (ast::AstNode* member : declaration->children())
    {
        if (member != nullptr && member->kind() == ast::AstNodeKind::FieldDeclaration &&
            member->auxiliaryText() == "promoted-primary-field" &&
            !member->hasModifier(TokenKind::KwReadonly))
            member->addModifier(TokenKind::KwReadonly);
    }
}

ast::AstNode* Parser::parseClassMemberImpl(
    std::string_view className,
    bool allowUnresolvedNamedTypes,
    bool structContext)
{
    (void)allowUnresolvedNamedTypes;
    MemberParseState state;
    state.beginIndex = index_;
    parseMemberFrameStorage(state);
    parseMemberModifiers(state);
    if (state.completedDeclaration != nullptr) return state.completedDeclaration;
    if (atContextualKeyword("async"))
    {
        state.isAsync = true;
        consume();
    }
    validateMemberModifiers(state);
    return dispatchClassMember(className, structContext, state);
}

void Parser::parseMemberModifiers(MemberParseState& state)
{
    while (true)
    {
        parseExplicitFieldOffsets(state);
        if (tryParseUnsafeMemberModifier(state) ||
            tryParseSpecializeMemberModifier(state) ||
            tryParsePartialMemberModifier(state) ||
            tryParseNestedLayoutModifier(state) ||
            tryParseNestedValueModifier(state))
            continue;
        if (!tryParseStandardMemberModifier(state)) break;
        if (state.completedDeclaration != nullptr) break;
    }
}

bool Parser::tryParseUnsafeMemberModifier(MemberParseState& state)
{
    if (!atContextualKeyword("unsafe")) return false;
    std::size_t afterType = index_ + 1U;
    const bool methodModifier = scanTypeAt(index_ + 1U, afterType) &&
        afterType + 1U < tokens_.size() &&
        tokens_[afterType].kind() == TokenKind::Identifier &&
        (tokens_[afterType + 1U].kind() == TokenKind::LeftParen ||
         tokens_[afterType + 1U].kind() == TokenKind::Less);
    if (!methodModifier) return false;
    const lexer::Token& modifier = consume();
    setMemberFlag(state.isUnsafe, modifier, "duplicate unsafe modifier on method");
    return true;
}

bool Parser::tryParseSpecializeMemberModifier(MemberParseState& state)
{
    if (!atContextualKeyword("specialize")) return false;
    const lexer::Token& modifier = consume();
    if (state.isControlledSpecialization)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            modifier.range(),
            "duplicate specialize modifier on class member");
    }
    else
    {
        state.isControlledSpecialization = true;
        state.controlledSpecializationRange = modifier.range();
    }
    return true;
}

bool Parser::tryParsePartialMemberModifier(MemberParseState& state)
{
    if (!atContextualKeyword("partial")) return false;
    const bool contextualType = peek().kind() == TokenKind::Identifier &&
        (peek().lexeme() == "union" || peek().lexeme() == "variant" ||
         (peek().lexeme() == "record" && peek(2).kind() == TokenKind::KwStruct));
    const bool nested = peek().kind() == TokenKind::KwClass ||
        peek().kind() == TokenKind::KwStruct || peek().kind() == TokenKind::KwInterface ||
        peek().kind() == TokenKind::KwEnum || peek().kind() == TokenKind::KwDelegate ||
        contextualType || isAccessibilityModifier(peek().kind()) ||
        peek().kind() == TokenKind::KwStatic || peek().kind() == TokenKind::KwAbstract ||
        peek().kind() == TokenKind::KwSealed;
    if (nested)
    {
        const lexer::Token& modifier = consume();
        setMemberFlag(
            state.isPartial, modifier,
            "duplicate 'partial' modifier on nested type declaration");
        return true;
    }
    const bool member = lexer::isBuiltinTypeKeyword(peek().kind()) ||
        peek().kind() == TokenKind::KwVoid || peek().kind() == TokenKind::Identifier ||
        peek().kind() == TokenKind::KwIn || peek().kind() == TokenKind::KwEvent ||
        lexer::isOwnershipQualifierKeyword(peek().kind());
    if (!member) return false;
    consume();
    state.isPartialMember = true;
    return true;
}

bool Parser::tryParseNestedValueModifier(MemberParseState& state)
{
    if (at(TokenKind::KwBorrowed) &&
        (peek().kind() == TokenKind::KwStruct ||
         (peek().kind() == TokenKind::Identifier && peek().lexeme() == "copy")))
    {
        const lexer::Token& modifier = consume();
        setMemberFlag(
            state.isBorrowedView, modifier,
            "duplicate borrowed modifier on nested type");
        return true;
    }
    const bool copy = atContextualKeyword("copy") &&
        (peek().kind() == TokenKind::KwStruct || peek().kind() == TokenKind::KwClass ||
         peek().kind() == TokenKind::KwEnum || peek().kind() == TokenKind::KwDelegate ||
         isAccessibilityModifier(peek().kind()) ||
         peek().kind() == TokenKind::KwReadonly ||
         (peek().kind() == TokenKind::Identifier &&
          (peek().lexeme() == "pack" || peek().lexeme() == "align" ||
           peek().lexeme() == "size" || peek().lexeme() == "record" ||
           peek().lexeme() == "variant")));
    if (!copy) return false;
    const lexer::Token& modifier = consume();
    setMemberFlag(
        state.isCopyType, modifier,
        "duplicate copy modifier on nested value type");
    return true;
}

bool Parser::tryParseStandardMemberModifier(MemberParseState& state)
{
    if (!(isAccessibilityModifier(current().kind()) || at(TokenKind::KwNew) ||
          at(TokenKind::KwStatic) || at(TokenKind::KwReadonly) ||
          at(TokenKind::KwConst) || at(TokenKind::KwVolatile) ||
          at(TokenKind::KwInline) || at(TokenKind::KwForceInline) ||
          at(TokenKind::KwNoInline) || at(TokenKind::KwNoHeap) ||
          at(TokenKind::KwVirtual) || at(TokenKind::KwOverride) ||
          at(TokenKind::KwAbstract) || at(TokenKind::KwSealed) ||
          at(TokenKind::KwExport) || at(TokenKind::KwExtern) ||
          atContextualKeyword("required")))
        return false;
    const lexer::Token& modifier = consume();
    return tryParseMemberStorageModifier(state, modifier) ||
        tryParseMemberDispatchModifier(state, modifier) ||
        tryParseMemberCodegenModifier(state, modifier) ||
        tryParseMemberInteropModifier(state, modifier);
}

bool Parser::tryParseMemberStorageModifier(
    MemberParseState& state,
    const lexer::Token& modifier)
{
    switch (modifier.kind())
    {
        case TokenKind::KwNew:
            setMemberFlag(state.isNew, modifier, "duplicate new modifier on class member");
            return true;
        case TokenKind::KwStatic:
            setMemberFlag(state.isStatic, modifier, "duplicate static modifier on class member");
            return true;
        case TokenKind::KwReadonly:
            setMemberFlag(state.isReadonly, modifier, "duplicate readonly modifier on class member");
            return true;
        case TokenKind::KwVolatile:
            setMemberFlag(state.isVolatile, modifier, "duplicate volatile modifier on class member");
            return true;
        case TokenKind::KwConst:
            setMemberFlag(state.isConst, modifier, "duplicate const modifier on class member");
            return true;
        default:
            break;
    }
    if (modifier.lexeme() != "required") return false;
    setMemberFlag(state.isRequired, modifier, "duplicate required modifier on class member");
    return true;
}

bool Parser::tryParseMemberDispatchModifier(
    MemberParseState& state,
    const lexer::Token& modifier)
{
    switch (modifier.kind())
    {
        case TokenKind::KwVirtual:
            setMemberFlag(state.isVirtual, modifier, "duplicate virtual modifier on class member");
            return true;
        case TokenKind::KwOverride:
            setMemberFlag(state.isOverride, modifier, "duplicate override modifier on class member");
            return true;
        case TokenKind::KwAbstract:
            setMemberFlag(state.isAbstract, modifier, "duplicate abstract modifier on class member");
            return true;
        case TokenKind::KwSealed:
            setMemberFlag(state.isSealed, modifier, "duplicate sealed modifier on class member");
            return true;
        default:
            return false;
    }
}

bool Parser::tryParseMemberCodegenModifier(
    MemberParseState& state,
    const lexer::Token& modifier)
{
    switch (modifier.kind())
    {
        case TokenKind::KwInline:
            setMemberFlag(state.isInline, modifier, "duplicate inline modifier on class member");
            return true;
        case TokenKind::KwForceInline:
            setMemberFlag(
                state.isForceInline, modifier,
                "duplicate forceinline modifier on class member");
            return true;
        case TokenKind::KwNoInline:
            setMemberFlag(state.isNoInline, modifier, "duplicate noinline modifier on class member");
            return true;
        case TokenKind::KwNoHeap:
            setMemberFlag(state.isNoHeap, modifier, "duplicate noheap modifier on class member");
            return true;
        default:
            return false;
    }
}

bool Parser::tryParseMemberInteropModifier(
    MemberParseState& state,
    const lexer::Token& modifier)
{
    if (modifier.kind() == TokenKind::KwExport)
    {
        setMemberFlag(state.isExported, modifier, "duplicate export modifier on class member");
        return true;
    }
    if (modifier.kind() == TokenKind::KwExtern)
    {
        if (state.isExternC)
            setMemberFlag(state.isExternC, modifier, "duplicate extern C modifier on class member");
        expectContextualKeyword("C", "'C' after extern");
        if ((at(TokenKind::Identifier) && current().lexeme() == "from") ||
            at(TokenKind::LeftBrace))
            state.completedDeclaration = parseExternCMemberImportBlock(state, modifier);
        else state.isExternC = true;
        return true;
    }
    if (state.accessibility != TokenKind::Invalid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            modifier.range(),
            state.accessibility == modifier.kind()
                ? "duplicate accessibility modifier on class member"
                : "conflicting accessibility modifiers on class member");
    }
    else state.accessibility = modifier.kind();
    return true;
}

void Parser::setMemberFlag(
    bool& flag,
    const lexer::Token& modifier,
    std::string_view duplicateMessage)
{
    if (flag)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            modifier.range(),
            std::string(duplicateMessage));
    }
    flag = true;
}

ast::AstNode* Parser::parseExternCMemberImportBlock(
    MemberParseState& state,
    const lexer::Token& modifier)
{
    rejectControlledSpecialization(state, "an extern C import block");
    if (state.isInline || state.isForceInline || state.isNoInline || state.isNoHeap)
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
    if (dynamicImport && at(TokenKind::StringLiteral))
    {
        dllName = std::string(current().lexeme());
        if (dllName.size() >= 2 && dllName.front() == '"' && dllName.back() == '"')
            dllName = dllName.substr(1, dllName.size() - 2);
        consume();
    }
    else if (dynamicImport)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "extern C from requires a DLL name string, e.g. extern C from \"user32.dll\"");
    }
    ast::AstNode* block = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExternCDeclaration, tokenRangeFrom(state.beginIndex));
    block->setText(dllName);
    expect(TokenKind::LeftBrace, "'{'");
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (ast::AstNode* imported = parseExternCImportedFunction(dllName))
            block->addChild(imported);
    }
    expect(TokenKind::RightBrace, "'}'");
    block->setRange(tokenRangeFrom(state.beginIndex));
    return block;
}

ast::AstNode* Parser::parseExternCImportedFunction(std::string_view dllName)
{
    const std::size_t entryBegin = index_;
    TokenKind accessibility = TokenKind::KwPublic;
    if (isAccessibilityModifier(current().kind()))
        accessibility = consume().kind();
    consumeIf(TokenKind::KwStatic);
    ast::TypeSyntax* returnType = parseType();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "imported C function name");
        synchronizeStatement();
        return nullptr;
    }
    const lexer::Token& name = consume();
    ast::AstNode* imported = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, name.range());
    imported->setText(std::string(name.lexeme()));
    imported->setAuxiliaryText("cimport:" + std::string(dllName));
    imported->setTypeSyntax(returnType);
    imported->setTokenKind(accessibility);
    imported->addModifier(accessibility);
    imported->addModifier(TokenKind::KwStatic);
    imported->addModifier(TokenKind::KwExtern);
    parseExternCImportedParameters(imported);
    parseFfiUnwindClause(imported);
    expect(TokenKind::Semicolon, "';' (imported C functions have no body)");
    imported->setRange(tokenRangeFrom(entryBegin));
    return imported;
}

void Parser::parseExternCImportedParameters(ast::AstNode* imported)
{
    expect(TokenKind::LeftParen, "'('");
    bool sawFixedParameter = false;
    bool variadic = false;
    while (!at(TokenKind::RightParen))
    {
        if (at(TokenKind::Ellipsis))
        {
            if (!sawFixedParameter)
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "a variadic C declaration requires at least one fixed parameter before '...'");
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
    imported->setVariadic(variadic);
    expect(TokenKind::RightParen, "')'");
}

void Parser::validateMemberModifiers(const MemberParseState& state)
{
    validateMemberInheritanceModifiers(state);
    validateMemberBodyModifiers(state);
    validateMemberFieldModifiers(state);
}

void Parser::validateMemberInheritanceModifiers(const MemberParseState& state)
{
    if (state.isVirtual && state.isOverride)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a method cannot be declared both virtual and override");
    if (state.isNew && state.isOverride)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a member cannot be both new and override");
    if (state.isStatic &&
        (state.isVirtual || state.isOverride || state.isAbstract || state.isSealed))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "static methods cannot be virtual, override, abstract or sealed");
    if (state.isVirtual && state.isAbstract)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "abstract methods are implicitly virtual; do not combine abstract with virtual");
    if (state.isSealed && !state.isOverride)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "sealed is valid on a method only together with override");
    if (state.isAbstract && state.isSealed)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a method cannot be both abstract and sealed");
}

void Parser::validateMemberBodyModifiers(const MemberParseState& state)
{
    if (state.isInline && state.isAbstract)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "inline requires a method body and cannot modify an abstract method");
    if (state.isForceInline && state.isAbstract)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "forceinline requires a method body and cannot modify an abstract method");
    if (state.isNoInline && state.isAbstract)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "noinline requires a method body and cannot modify an abstract method");
    if ((static_cast<unsigned>(state.isInline) +
         static_cast<unsigned>(state.isForceInline) +
         static_cast<unsigned>(state.isNoInline)) > 1U)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "inline, forceinline, and noinline are contradictory inlining policies");
}

void Parser::validateMemberFieldModifiers(const MemberParseState& state)
{
    if (state.isVolatile && state.isReadonly)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a field cannot be both volatile and readonly");
    if (state.isConst && (state.isReadonly || state.isStatic || state.isVolatile))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const already has static immutable semantics; do not combine const with static, readonly, or volatile");
    if (state.isExported != state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "Stage 47 host ABI requires the paired modifier sequence 'export extern C'");
}

void Parser::rejectControlledSpecialization(
    const MemberParseState& state,
    std::string_view declarationKind)
{
    if (!state.isControlledSpecialization) return;
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        state.controlledSpecializationRange,
        "specialize applies only to an ordinary generic method, not " +
            std::string(declarationKind));
}

ast::AstNode* Parser::dispatchClassMember(
    std::string_view className,
    bool structContext,
    MemberParseState& state)
{
    if (at(TokenKind::Tilde)) return parseDestructorMember(className, state);
    if (at(TokenKind::Identifier) && current().lexeme() == className &&
        peek().kind() == TokenKind::LeftParen)
        return parseConstructorMember(className, state);
    if (at(TokenKind::KwImplicit) || at(TokenKind::KwExplicit))
        return parseConversionMember(state);
    if (at(TokenKind::KwEvent)) return parseEventMember(state);
    const bool anonymousStruct =
        at(TokenKind::KwStruct) && peek().kind() == TokenKind::LeftBrace;
    const bool anonymousUnion =
        atContextualKeyword("union") && peek().kind() == TokenKind::LeftBrace;
    if (anonymousStruct || anonymousUnion)
        return parseAnonymousAggregateMember(structContext, state);
    if (atNestedTypeDeclaration()) return parseNestedClassMember(state);
    return parseTypedClassMember(className, structContext, state);
}

ast::AstNode* Parser::parseDestructorMember(
    std::string_view className,
    MemberParseState& state)
{
    const std::size_t beginIndex = state.beginIndex;
    auto& accessibility = state.accessibility;
    auto& isNew = state.isNew;
    auto& isStatic = state.isStatic;
    auto& isReadonly = state.isReadonly;
    auto& isVolatile = state.isVolatile;
    auto& isConst = state.isConst;
    auto& isInline = state.isInline;
    auto& isForceInline = state.isForceInline;
    auto& isNoInline = state.isNoInline;
    auto& isNoHeap = state.isNoHeap;
    auto& isVirtual = state.isVirtual;
    auto& isOverride = state.isOverride;
    auto& isAbstract = state.isAbstract;
    auto& isSealed = state.isSealed;
    auto& isExported = state.isExported;
    auto& isExternC = state.isExternC;
    const auto rejectControlledSpecialization = [&](std::string_view declarationKind)
    {
        this->rejectControlledSpecialization(state, declarationKind);
    };
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
    return nullptr;
}

ast::AstNode* Parser::parseConstructorMember(
    std::string_view className,
    MemberParseState& state)
{
    rejectControlledSpecialization(state, "a constructor");
    if (state.isStatic)
    {
        if (state.accessibility != TokenKind::Invalid)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "static constructors cannot declare accessibility");
        if (state.isNew || state.isVirtual || state.isOverride ||
            state.isReadonly || state.isVolatile || state.isConst ||
            state.isInline || state.isForceInline || state.isNoInline ||
            state.isAbstract || state.isSealed ||
            state.isExported || state.isExternC)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "static constructors accept only the static and optional noheap modifiers");
    }
    else if (state.isNew || state.isVirtual || state.isOverride ||
        state.isReadonly || state.isVolatile || state.isConst ||
        state.isInline || state.isForceInline || state.isNoInline ||
        state.isAbstract || state.isSealed ||
        state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "constructors cannot be readonly, volatile, const, virtual, override, abstract or sealed");
    return parseConstructorTail(
        className, state.accessibility, state.isStatic, state.isNoHeap,
        state.beginIndex, state.isPartialMember);
}

ast::AstNode* Parser::parseConversionMember(MemberParseState& state)
{
    rejectControlledSpecialization(state, "a conversion operator");
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    if (state.isNew || state.isReadonly || state.isVolatile ||
        state.isConst || state.isInline || state.isForceInline ||
        state.isNoInline || state.isNoHeap || state.isVirtual ||
        state.isOverride || state.isAbstract || state.isSealed)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "conversion operators cannot be readonly, volatile, const, virtual, override, abstract or sealed");
    const bool implicitConversion = at(TokenKind::KwImplicit);
    return parseConversionOperatorTail(
        implicitConversion, state.accessibility,
        state.isStatic, state.beginIndex);
}

ast::AstNode* Parser::parseEventMember(MemberParseState& state)
{
    rejectControlledSpecialization(state, "an event");
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    if (state.isNew || state.isReadonly || state.isVolatile ||
        state.isConst || state.isInline || state.isForceInline ||
        state.isNoInline || state.isNoHeap || state.isVirtual ||
        state.isOverride || state.isAbstract || state.isSealed)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "events support accessibility and optional static only; volatile applies only to fields");
    return parseEventTail(
        state.accessibility, state.isNew, state.isStatic,
        state.beginIndex, state.isPartialMember);
}

ast::AstNode* Parser::parseAnonymousAggregateMember(
    bool structContext,
    MemberParseState& state)
{
    const bool anonymousUnion =
        atContextualKeyword("union") && peek().kind() == TokenKind::LeftBrace;
    rejectControlledSpecialization(state, "an anonymous aggregate field");
    const lexer::Token& keyword = consume();
    const std::string synthesizedName =
        std::string(anonymousUnion
            ? "__nsc_anonymous_union_f" : "__nsc_anonymous_struct_f") +
        std::to_string(keyword.range().begin().fileId().value()) + "_" +
        std::to_string(keyword.range().begin().offset());
    knownTypeNames_.insert(synthesizedName);
    validateAnonymousAggregateMember(structContext, keyword, state);
    ast::AstNode* declaration = createAnonymousAggregateDeclaration(
        anonymousUnion, keyword, synthesizedName, state);
    declaration->setFirst(createAnonymousAggregateField(
        declaration, keyword, synthesizedName, state));
    declaration->setRange(tokenRangeFrom(state.beginIndex));
    return declaration;
}

void Parser::validateAnonymousAggregateMember(
    bool structContext,
    const lexer::Token& keyword,
    const MemberParseState& state)
{
    if (state.isPartial || state.isVirtual || state.isOverride ||
        state.isAbstract || state.isSealed || state.isExported ||
        state.isExternC || state.isInline || state.isForceInline ||
        state.isNoInline || state.isNoHeap || state.isBorrowedView)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            keyword.range(),
            "an anonymous aggregate field accepts field accessibility, new, static, readonly, "
            "volatile, const and layout modifiers only");
    if (structContext && state.accessibility == TokenKind::KwProtected)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            keyword.range(),
            "a struct member cannot be protected: structs are not inheritable");
}

ast::AstNode* Parser::createAnonymousAggregateDeclaration(
    bool anonymousUnion,
    const lexer::Token& keyword,
    std::string_view synthesizedName,
    MemberParseState& state)
{
    ast::AstNode* declaration = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StructDeclaration, keyword.range());
    declaration->setText(std::string(synthesizedName));
    declaration->setAuxiliaryText(
        anonymousUnion ? "anonymous-union,union" : "anonymous-struct");
    if (state.accessibility != TokenKind::Invalid)
        declaration->setTokenKind(state.accessibility);
    const auto appendLayout =
        [&](std::string_view name, std::optional<std::uint64_t> value)
    {
        if (!value.has_value()) return;
        std::string markers = declaration->auxiliaryText();
        markers += "," + std::string(name) + "=" + std::to_string(*value);
        declaration->setAuxiliaryText(std::move(markers));
    };
    appendLayout("pack", state.nestedLayoutPack);
    appendLayout("align", state.nestedLayoutAlign);
    appendLayout("size", state.nestedLayoutSize);
    parseStructBody(declaration, synthesizedName, state.beginIndex);
    return declaration;
}

ast::AstNode* Parser::createAnonymousAggregateField(
    ast::AstNode* declaration,
    const lexer::Token& keyword,
    std::string_view synthesizedName,
    MemberParseState& state)
{
    const bool promotesMembers = !at(TokenKind::Identifier);
    std::string fieldName;
    SourceRange fieldNameRange = current().range();
    if (promotesMembers)
    {
        fieldName = "__nsc_anonymous_member_f" +
            std::to_string(keyword.range().begin().fileId().value()) + "_" +
            std::to_string(keyword.range().begin().offset());
        declaration->setAuxiliaryText(declaration->auxiliaryText() + ",promote");
        if (state.isNew || state.isStatic || state.isReadonly ||
            state.isVolatile || state.isConst)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                keyword.range(),
                "an unnamed anonymous aggregate accepts accessibility, at, pack, align and size only");
    }
    else
    {
        const lexer::Token& name = consume();
        fieldName = std::string(name.lexeme());
        fieldNameRange = name.range();
    }
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        keyword.range(), TokenKind::Identifier, std::string(synthesizedName));
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, fieldNameRange);
    field->setTypeSyntax(type);
    field->setText(std::move(fieldName));
    if (state.explicitFieldOffset.has_value())
        field->setAuxiliaryText(
            "at:" + std::to_string(*state.explicitFieldOffset));
    if (promotesMembers)
    {
        std::string metadata = field->auxiliaryText();
        metadata += (metadata.empty() ? "" : ",");
        metadata += "anonymous-member-carrier";
        field->setAuxiliaryText(std::move(metadata));
    }
    applyAnonymousFieldModifiers(field, state);
    if (!promotesMembers && consumeIf(TokenKind::Equal))
        field->setFirst(
            at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    else if (!promotesMembers && state.isConst)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            fieldNameRange,
            "const field requires an initializer");
    expect(TokenKind::Semicolon, "';'");
    field->setRange(tokenRangeFrom(state.beginIndex));
    return field;
}

void Parser::applyAnonymousFieldModifiers(
    ast::AstNode* field,
    const MemberParseState& state)
{
    if (state.accessibility != TokenKind::Invalid)
    {
        field->setTokenKind(state.accessibility);
        field->addModifier(state.accessibility);
    }
    if (state.isVolatile) field->addModifier(TokenKind::KwVolatile);
    if (state.isStatic) field->addModifier(TokenKind::KwStatic);
    if (state.isNew) field->addModifier(TokenKind::KwNew);
    if (state.isReadonly) field->addModifier(TokenKind::KwReadonly);
    if (state.isConst) field->addModifier(TokenKind::KwConst);
}

ast::AstNode* Parser::parseNestedClassMember(MemberParseState& state)
{
    rejectControlledSpecialization(state, "a nested type");
    if (state.isVolatile)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "volatile applies only to fields, not nested types");
    if (state.isInline || state.isForceInline ||
        state.isNoInline || state.isNoHeap)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "inline is valid only on an ordinary method with a body");
    return parseNestedTypeDeclaration(
        NestedTypeModifiers{
            state.accessibility,
            state.isPartial,
            state.isNew,
            state.isStatic,
            state.isReadonly,
            state.isConst,
            state.isVirtual,
            state.isOverride,
            state.isAbstract,
            state.isSealed,
            state.isExported,
            state.isExternC,
            state.explicitFieldOffset.has_value(),
            state.isBorrowedView,
            state.isCopyType,
            state.nestedLayoutPack,
            state.nestedLayoutAlign,
            state.nestedLayoutSize},
        state.beginIndex);
}

ast::AstNode* Parser::parseTypedClassMember(
    std::string_view className,
    bool structContext,
    MemberParseState& state)
{
    const bool isReferenceReturn = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1U) || peek().kind() == TokenKind::LeftBracket);
    std::string returnLifetime;
    if (isReferenceReturn)
    {
        consume();
        returnLifetime = parseOptionalLifetimeAnnotation("ref return");
    }
    if (!canStartTypeAt(index_) && !at(TokenKind::Identifier) &&
        !lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        rejectControlledSpecialization(state, "an invalid member declaration");
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected an instance field or method declaration in class body");
        synchronizeStatement();
        return nullptr;
    }
    ast::TypeSyntax* type = parseType();
    if (at(TokenKind::Colon))
        return parseUnnamedBitFieldMember(type, isReferenceReturn, state);
    if (at(TokenKind::KwOperator))
        return parseOperatorClassMember(type, isReferenceReturn, state);
    if (at(TokenKind::KwThis))
        return parseIndexerClassMember(type, structContext, isReferenceReturn, state);
    return parseNamedClassMember(
        className, structContext, type, isReferenceReturn,
        std::move(returnLifetime), state);
}

ast::AstNode* Parser::parseUnnamedBitFieldMember(
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    MemberParseState& state)
{
    rejectControlledSpecialization(state, "an unnamed bit-field");
    const lexer::Token& colon = consume();
    ast::AstNode* width = parseExpression();
    if (width != nullptr &&
        width->kind() == ast::AstNodeKind::AssignmentExpression)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            width->range(),
            "bit-field width cannot contain an assignment or initializer");
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, colon.range());
    field->setTypeSyntax(type);
    field->setText("__nsc_unnamed_bitfield_f" +
        std::to_string(colon.range().begin().fileId().value()) + "_" +
        std::to_string(colon.range().begin().offset()));
    field->setAuxiliaryText("bitfield;bitfield-unnamed");
    field->setSecond(width);
    if (isInvalidMemberObjectType(type))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type->range(),
            "bit-field cannot have object type 'void'");
    if (state.accessibility != TokenKind::Invalid)
    {
        field->setTokenKind(state.accessibility);
        field->addModifier(state.accessibility);
    }
    if (state.isVolatile) field->addModifier(TokenKind::KwVolatile);
    if (state.isStatic || state.isNew || state.isReadonly ||
        state.isConst || isReferenceReturn || state.isVirtual ||
        state.isOverride || state.isAbstract || state.isSealed ||
        state.isExported || state.isExternC || state.isInline ||
        state.isForceInline || state.isNoInline || state.isNoHeap ||
        state.isPartial || state.isBorrowedView ||
        state.explicitFieldOffset.has_value() ||
        state.nestedLayoutPack.has_value() ||
        state.nestedLayoutAlign.has_value() ||
        state.nestedLayoutSize.has_value())
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            colon.range(),
            "an unnamed bit-field accepts accessibility and optional volatile only");
    expect(TokenKind::Semicolon, "';'");
    field->setRange(tokenRangeFrom(state.beginIndex));
    return field;
}

ast::AstNode* Parser::parseOperatorClassMember(
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    MemberParseState& state)
{
    rejectControlledSpecialization(state, "an operator");
    if (isReferenceReturn)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "ref return is valid only on an ordinary method in item 313");
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    if (state.isReadonly || state.isVolatile || state.isConst ||
        state.isNoHeap || state.isVirtual || state.isOverride ||
        state.isAbstract || state.isSealed)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "user-defined operators cannot be readonly, volatile, const, noheap, virtual, override, abstract or sealed");
    ast::AstNode* declaration = parseOperatorTail(
        type, state.accessibility, state.isStatic, state.beginIndex);
    if (declaration != nullptr)
    {
        if (state.isInline) declaration->addModifier(TokenKind::KwInline);
        if (state.isForceInline)
            declaration->addModifier(TokenKind::KwForceInline);
        if (state.isNoInline)
            declaration->addModifier(TokenKind::KwNoInline);
    }
    return declaration;
}

ast::AstNode* Parser::parseIndexerClassMember(
    ast::TypeSyntax* type,
    bool structContext,
    bool isReferenceReturn,
    MemberParseState& state)
{
    rejectControlledSpecialization(state, "an indexer");
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    if (state.isStatic || state.isVolatile || state.isConst ||
        state.isInline || state.isForceInline || state.isNoInline ||
        state.isNoHeap || (state.isReadonly && !structContext))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "indexers are instance members; readonly is valid only for a struct indexer and "
            "static, volatile, or const are never valid here");
    ast::AstNode* indexer = parseIndexerTail(
        type, state.accessibility, state.isNew, state.isVirtual,
        state.isOverride, state.isAbstract, state.isSealed,
        isReferenceReturn, state.beginIndex, false, structContext);
    if (!state.isReadonly || indexer == nullptr) return indexer;
    indexer->addModifier(TokenKind::KwReadonly);
    bool setterFound = false;
    for (ast::AstNode* accessor : indexer->children())
    {
        if (accessor == nullptr ||
            accessor->kind() != ast::AstNodeKind::MethodDeclaration)
            continue;
        if (accessor->auxiliaryText().starts_with("indexer:set:"))
            setterFound = true;
        else if (accessor->auxiliaryText().starts_with("indexer:get:"))
            accessor->addModifier(TokenKind::KwReadonly);
    }
    if (setterFound)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            indexer->range(),
            "a readonly struct indexer cannot declare a setter");
    return indexer;
}

ast::AstNode* Parser::parseNamedClassMember(
    std::string_view className,
    bool structContext,
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    std::string returnLifetime,
    MemberParseState& state)
{
    const auto rejectControlledSpecialization = [&](std::string_view declarationKind)
    {
        this->rejectControlledSpecialization(state, declarationKind);
    };
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
        return parseMethodClassMember(
            name, structContext, type, isReferenceReturn,
            std::move(returnLifetime), state);
    if (at(TokenKind::LeftBrace))
        return parsePropertyClassMember(
            name, structContext, type, isReferenceReturn, state);
    return parseFieldClassMember(name, type, isReferenceReturn, state);
}

ast::AstNode* Parser::parseMethodClassMember(
    const lexer::Token& name,
    bool structContext,
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    std::string returnLifetime,
    MemberParseState& state)
{
    if (state.isVolatile || state.isConst ||
        (state.isReadonly && (!structContext || state.isStatic)))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "readonly is valid only on an instance struct method; const and volatile are field-only");
    ast::AstNode* method = parseMethodTail(
        type, name, state.accessibility, state.isNew, state.isStatic,
        state.isVirtual, state.isOverride, state.isAbstract, state.isSealed,
        state.isExported, state.isExternC, state.isInline,
        state.isForceInline, state.isNoInline, state.isNoHeap, state.isUnsafe,
        state.beginIndex, false, state.isPartialMember);
    finalizeMethodClassMember(
        method, structContext, isReferenceReturn,
        std::move(returnLifetime), state);
    return method;
}

void Parser::finalizeMethodClassMember(
    ast::AstNode* method,
    bool structContext,
    bool isReferenceReturn,
    std::string returnLifetime,
    MemberParseState& state)
{
    if (method == nullptr) return;
    if (state.isControlledSpecialization)
    {
        method->setAuxiliaryText("controlled-specialization");
        if (method->genericParameters().empty())
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                state.controlledSpecializationRange,
                "specialize requires an ordinary generic method");
        if (state.isVirtual || state.isOverride ||
            state.isAbstract || state.isSealed)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                state.controlledSpecializationRange,
                "controlled specialization is not valid on virtual, override, abstract, or sealed methods");
    }
    if (state.isReadonly && structContext && !state.isStatic)
        method->addModifier(TokenKind::KwReadonly);
    method->setReference(isReferenceReturn);
    method->setLifetimeName(std::move(returnLifetime));
    if (state.isAsync)
    {
        std::string marker = method->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "async";
        if (!state.frameStorage.empty())
        {
            marker += ";framestorage:";
            marker += state.frameStorage;
        }
        method->setAuxiliaryText(std::move(marker));
    }
    if (state.isPartialMember)
    {
        std::string marker = method->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "partial-method";
        method->setAuxiliaryText(std::move(marker));
    }
}

ast::AstNode* Parser::parsePropertyClassMember(
    const lexer::Token& name,
    bool structContext,
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    MemberParseState& state)
{
    rejectControlledSpecialization(state, "a property");
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    if (state.isVolatile || state.isConst || state.isInline ||
        state.isForceInline || state.isNoInline || state.isNoHeap ||
        (state.isReadonly && (!structContext || state.isStatic)))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "readonly is valid only on an instance struct property; const, volatile, and inline are not property modifiers");
    ast::AstNode* property = parsePropertyTail(
        type, name, state.accessibility, state.isNew, state.isStatic,
        state.isVirtual, state.isOverride, state.isAbstract, state.isSealed,
        state.beginIndex, false, structContext && !state.isStatic,
        isReferenceReturn, state.isPartialMember);
    if (!state.isReadonly || !structContext || state.isStatic ||
        property == nullptr)
        return property;
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
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            property->range(),
            "a readonly struct property cannot declare a setter");
    return property;
}

ast::AstNode* Parser::parseFieldClassMember(
    const lexer::Token& name,
    ast::TypeSyntax* type,
    bool isReferenceReturn,
    MemberParseState& state)
{
    validateFieldMember(name, type, state);
    ast::AstNode* field =
        createFieldDeclaration(name, type, isReferenceReturn);
    ast::AstNode* bitWidth =
        parseFieldBitWidth(field, name, isReferenceReturn, state);
    applyFieldMetadata(field, state);
    applyFieldModifiers(field, state);
    parseFieldInitializer(field, name, bitWidth, state);
    expect(TokenKind::Semicolon, "';'");
    field->setRange(tokenRangeFrom(state.beginIndex));
    return field;
}

void Parser::validateFieldMember(
    const lexer::Token& name,
    ast::TypeSyntax* type,
    const MemberParseState& state)
{
    if (state.isExported || state.isExternC)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "export extern C is valid only on ordinary static methods in Stage 47");
    rejectControlledSpecialization(state, "a field");
    if (state.isVirtual || state.isOverride ||
        state.isAbstract || state.isSealed)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "virtual, override, abstract and sealed modifiers are valid only on instance methods in Stage 35");
    if (state.isInline || state.isForceInline ||
        state.isNoInline || state.isNoHeap)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "inline is valid only on an ordinary method with a body");
    if (isInvalidMemberObjectType(type))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type->range(),
            "instance field cannot have object type 'void'; pointer-to-void remains a valid pointer type");
}

ast::AstNode* Parser::createFieldDeclaration(
    const lexer::Token& name,
    ast::TypeSyntax* type,
    bool isReferenceReturn)
{
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, name.range());
    field->setTypeSyntax(type);
    field->setText(std::string(name.lexeme()));
    field->setReference(isReferenceReturn);
    return field;
}

ast::AstNode* Parser::parseFieldBitWidth(
    ast::AstNode* field,
    const lexer::Token& name,
    bool isReferenceReturn,
    const MemberParseState& state)
{
    if (!consumeIf(TokenKind::Colon)) return nullptr;
    ast::AstNode* bitWidth = parseExpression();
    if (bitWidth != nullptr &&
        bitWidth->kind() == ast::AstNodeKind::AssignmentExpression)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            bitWidth->range(),
            "bit-field width cannot contain an assignment or initializer");
    field->setSecond(bitWidth);
    field->setAuxiliaryText("bitfield");
    if (state.isStatic || state.isNew || state.isReadonly ||
        state.isConst || isReferenceReturn || state.isPartial ||
        state.isBorrowedView || state.explicitFieldOffset.has_value() ||
        state.nestedLayoutPack.has_value() ||
        state.nestedLayoutAlign.has_value() ||
        state.nestedLayoutSize.has_value())
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "a bit-field accepts accessibility and optional volatile only");
    return bitWidth;
}

void Parser::applyFieldMetadata(
    ast::AstNode* field,
    const MemberParseState& state)
{
    if (state.explicitFieldOffset.has_value())
        field->setAuxiliaryText(
            "at:" + std::to_string(*state.explicitFieldOffset));
    if (state.nestedLayoutAlign.has_value())
    {
        std::string metadata = field->auxiliaryText();
        if (!metadata.empty()) metadata += ";";
        metadata += "field-align:" +
            std::to_string(*state.nestedLayoutAlign);
        field->setAuxiliaryText(std::move(metadata));
    }
    if (state.isRequired)
    {
        std::string metadata = field->auxiliaryText();
        if (!metadata.empty()) metadata += ";";
        metadata += "required";
        field->setAuxiliaryText(std::move(metadata));
    }
}

void Parser::applyFieldModifiers(
    ast::AstNode* field,
    const MemberParseState& state)
{
    if (state.accessibility != TokenKind::Invalid)
    {
        field->setTokenKind(state.accessibility);
        field->addModifier(state.accessibility);
    }
    if (state.isVolatile) field->addModifier(TokenKind::KwVolatile);
    if (state.isStatic) field->addModifier(TokenKind::KwStatic);
    if (state.isNew) field->addModifier(TokenKind::KwNew);
    if (state.isReadonly) field->addModifier(TokenKind::KwReadonly);
    if (state.isConst) field->addModifier(TokenKind::KwConst);
}

void Parser::parseFieldInitializer(
    ast::AstNode* field,
    const lexer::Token& name,
    ast::AstNode* bitWidth,
    const MemberParseState& state)
{
    if (bitWidth == nullptr && consumeIf(TokenKind::Equal))
    {
        field->setFirst(
            at(TokenKind::LeftBrace)
                ? parseAggregateInitializer()
                : parseExpression());
        return;
    }
    if (bitWidth == nullptr && state.isConst)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            name.range(),
            "const field requires an initializer");
}

ast::AstNode* Parser::parseEventTail(
    TokenKind accessibility,
    bool isNew,
    bool isStatic,
    std::size_t beginIndex,
    bool isPartial)
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
    event->setAuxiliaryText(isPartial ? "event;partial-event" : "event");
    if (accessibility != TokenKind::Invalid)
    {
        event->setTokenKind(accessibility);
        event->addModifier(accessibility);
    }
    if (isStatic) event->addModifier(TokenKind::KwStatic);
    if (isNew) event->addModifier(TokenKind::KwNew);

    addEventBackingField(event, name, eventType, isStatic);
    if (at(TokenKind::LeftBrace))
        parseEventAccessors(event, name, eventType, beginIndex, isPartial);
    else
        expect(TokenKind::Semicolon, "';'");
    event->setRange(tokenRangeFrom(beginIndex));
    return event;
}

void Parser::addEventBackingField(
    ast::AstNode* event,
    const lexer::Token& name,
    ast::TypeSyntax* eventType,
    bool isStatic)
{
    ast::AstNode* backing = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, name.range());
    backing->setText("__nsc_event_" + std::string(name.lexeme()) + "$stage42");
    backing->setAuxiliaryText("event-backing:" + std::string(name.lexeme()));
    backing->setTypeSyntax(eventType);
    backing->setTokenKind(TokenKind::KwPrivate);
    backing->addModifier(TokenKind::KwPrivate);
    if (isStatic) backing->addModifier(TokenKind::KwStatic);
    event->addChild(backing);
}

void Parser::parseEventAccessors(
    ast::AstNode* event,
    const lexer::Token& name,
    ast::TypeSyntax* eventType,
    std::size_t beginIndex,
    bool isPartial)
{
    consume();
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
        event->addChild(parseEventAccessor(
            name, eventType, beginIndex, isPartial, isAdd));
    }
    expect(TokenKind::RightBrace, "'}' after event accessors");
}

ast::AstNode* Parser::parseEventAccessor(
    const lexer::Token& name,
    ast::TypeSyntax* eventType,
    std::size_t beginIndex,
    bool isPartial,
    bool isAdd)
{
    const SourceRange range = consume().range();
    ast::AstNode* accessor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, range);
    accessor->setText((isAdd ? "add_" : "remove_") + std::string(name.lexeme()));
    accessor->setAuxiliaryText(
        (isAdd ? "event:add:" : "event:remove:") + std::string(name.lexeme()) +
        (isPartial ? ";partial-method" : ""));
    accessor->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVoid, "void"));
    ast::AstNode* value = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ParameterDeclaration, range);
    value->setText("value");
    value->setTypeSyntax(eventType);
    accessor->addChild(value);
    accessor->setFirst(parseBlock());
    accessor->setRange(tokenRangeFrom(beginIndex));
    return accessor;
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
    bool interfaceContext,
    bool allowReadonlyGetter,
    bool isReferenceReturn,
    bool isPartial)
{
    PropertyParseState state;
    state.type = propertyType;
    state.name = &name;
    state.accessibility = accessibility;
    state.beginIndex = beginIndex;
    state.isNew = isNew;
    state.isStatic = isStatic;
    state.isVirtual = isVirtual;
    state.isOverride = isOverride;
    state.isAbstract = isAbstract;
    state.isSealed = isSealed;
    state.interfaceContext = interfaceContext;
    state.allowReadonlyGetter = allowReadonlyGetter;
    state.isReferenceReturn = isReferenceReturn;
    state.isPartial = isPartial;
    state.propertyName = std::string(name.lexeme());
    state.backingName = "__nsc_backing_" + state.propertyName + "$stage38";
    state.property = createPropertyDeclaration(state);
    expect(TokenKind::LeftBrace, "'{'");
    parsePropertyAccessors(state);
    expect(TokenKind::RightBrace, "'}'");
    parsePropertyInitializer(state);
    finalizePropertyDeclaration(state);
    state.property->setRange(tokenRangeFrom(beginIndex));
    return state.property;
}

ast::AstNode* Parser::createPropertyDeclaration(PropertyParseState& state)
{
    ast::AstNode* property = arena_.make<ast::AstNode>(
        ast::AstNodeKind::PropertyDeclaration, state.name->range());
    property->setText(state.propertyName);
    property->setTypeSyntax(state.type);
    property->setAuxiliaryText(
        state.isPartial ? "property;partial-property" : "property");
    if (state.accessibility != TokenKind::Invalid)
    {
        property->setTokenKind(state.accessibility);
        property->addModifier(state.accessibility);
    }
    if (state.isStatic) property->addModifier(TokenKind::KwStatic);
    if (state.isNew) property->addModifier(TokenKind::KwNew);
    if (state.isVirtual) property->addModifier(TokenKind::KwVirtual);
    if (state.isOverride) property->addModifier(TokenKind::KwOverride);
    if (state.isAbstract) property->addModifier(TokenKind::KwAbstract);
    if (state.isSealed) property->addModifier(TokenKind::KwSealed);
    if (isInvalidMemberObjectType(state.type))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.type->range(),
            "property cannot have object type 'void'");
    return property;
}

void Parser::parsePropertyAccessors(PropertyParseState& state)
{
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (ast::AstNode* accessor = parsePropertyAccessor(state))
            state.accessors.push_back(accessor);
        else
        {
            synchronizeStatement();
            if (at(TokenKind::RightBrace)) break;
        }
    }
}

ast::AstNode* Parser::parsePropertyAccessor(PropertyParseState& state)
{
    TokenKind accessorAccessibility = TokenKind::Invalid;
    if (isAccessibilityModifier(current().kind()))
    {
        accessorAccessibility = consume().kind();
        if (state.interfaceContext)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[index_ - 1].range(),
                "interface property accessors cannot narrow accessibility in Stage 38");
    }
    const bool accessorReadonly = consumeIf(TokenKind::KwReadonly);
    const bool getter = at(TokenKind::KwGet);
    const bool initOnly = atContextualKeyword("init");
    const bool setter = at(TokenKind::KwSet) || initOnly;
    if (!getter && !setter)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected get, set or init accessor in property declaration");
        return nullptr;
    }
    if (accessorReadonly && (!getter || !state.allowReadonlyGetter))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[index_ - 1].range(),
            getter
                ? "a readonly property getter is valid only on an instance struct property"
                : "readonly is valid only on a property get accessor");
    const SourceRange range = consume().range();
    if ((getter && state.sawGet) || (setter && state.sawSet))
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            range,
            getter ? "duplicate get accessor" : "duplicate set accessor");
    state.sawGet = state.sawGet || getter;
    state.sawSet = state.sawSet || setter;
    const TokenKind effective = accessorAccessibility != TokenKind::Invalid
        ? accessorAccessibility : state.accessibility;
    ast::AstNode* accessor = createPropertyAccessorNode(
        state, getter, initOnly, accessorReadonly, effective, range);
    parseThrowsClause(accessor);
    parsePropertyAccessorBody(state, accessor, getter, range);
    accessor->setRange(tokenRangeFrom(state.beginIndex));
    return accessor;
}

ast::AstNode* Parser::createPropertyAccessorNode(
    PropertyParseState& state,
    bool getter,
    bool initOnly,
    bool accessorReadonly,
    TokenKind accessibility,
    SourceRange range)
{
    ast::TypeSyntax* returnType = getter
        ? state.type
        : arena_.make<ast::TypeSyntax>(range, TokenKind::KwVoid, "void");
    ast::AstNode* accessor = makeAccessorMethod(
        (getter ? "get_" : "set_") + state.propertyName,
        returnType, accessibility, state.isStatic, state.isVirtual,
        state.isOverride, state.isAbstract, state.isSealed,
        std::string("property:") + (getter ? "get:" : "set:") +
            state.propertyName + (initOnly ? ":init" : ""),
        range);
    if (state.isPartial)
        accessor->setAuxiliaryText(accessor->auxiliaryText() + ";partial-method");
    if (getter && state.isReferenceReturn) accessor->setReference(true);
    if (state.isNew) accessor->addModifier(TokenKind::KwNew);
    if (accessorReadonly && getter && state.allowReadonlyGetter)
        accessor->addModifier(TokenKind::KwReadonly);
    if (!getter)
    {
        ast::AstNode* value = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, range);
        value->setText("value");
        value->setTypeSyntax(state.type);
        accessor->addChild(value);
    }
    return accessor;
}

void Parser::parsePropertyAccessorBody(
    PropertyParseState& state,
    ast::AstNode* accessor,
    bool getter,
    SourceRange accessorRange)
{
    if (consumeIf(TokenKind::Semicolon))
    {
        if (state.isPartial) return;
        if (state.interfaceContext || state.isAbstract)
        {
            if (!accessor->hasModifier(TokenKind::KwAbstract))
                accessor->addModifier(TokenKind::KwAbstract);
            return;
        }
        state.sawAutomatic = true;
        const bool owned = state.type != nullptr &&
            state.type->ownershipQualifier() ==
                ast::OwnershipQualifierSyntaxKind::Owned;
        accessor->setFirst(
            getter ? makeAutoGetterBody(state.backingName, accessorRange)
                   : makeAutoSetterBody(state.backingName, accessorRange, owned));
        return;
    }
    if (at(TokenKind::LeftBrace))
    {
        state.sawExplicit = true;
        if (state.isAbstract)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "abstract/interface property accessors cannot declare bodies");
        accessor->setFirst(parseBlock());
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "property accessor requires ';' or a block body");
    synchronizeStatement();
}

void Parser::parsePropertyInitializer(PropertyParseState& state)
{
    if (!consumeIf(TokenKind::Equal)) return;
    state.initializer = at(TokenKind::LeftBrace)
        ? parseAggregateInitializer() : parseExpression();
    expect(TokenKind::Semicolon, "';'");
    if (!state.sawAutomatic || state.sawExplicit ||
        state.interfaceContext || state.isAbstract)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.property->range(),
            "a property initializer requires a non-abstract automatic property");
}

void Parser::finalizePropertyDeclaration(PropertyParseState& state)
{
    if (!state.sawGet)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.property->range(),
            "set-only properties are not supported: property access resolves through the get "
            "accessor, so a property without one cannot be bound. Declare a get accessor, or "
            "use an ordinary method such as 'void SetX(T value)'");
    if (state.sawAutomatic && state.sawExplicit)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.property->range(),
            "a Stage 38 property cannot mix automatic and explicit accessors");
    if (state.interfaceContext && !state.sawExplicit &&
        !state.property->hasModifier(TokenKind::KwAbstract))
        state.property->addModifier(TokenKind::KwAbstract);
    if (state.sawAutomatic && !state.interfaceContext && !state.isAbstract)
    {
        ast::AstNode* backing = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration, state.name->range());
        backing->setText(state.backingName);
        backing->setAuxiliaryText("property-backing:" + state.propertyName);
        backing->setTypeSyntax(state.type);
        backing->setTokenKind(TokenKind::KwPrivate);
        backing->addModifier(TokenKind::KwPrivate);
        if (state.isStatic) backing->addModifier(TokenKind::KwStatic);
        backing->setFirst(state.initializer);
        state.property->addChild(backing);
    }
    for (ast::AstNode* accessor : state.accessors)
        state.property->addChild(accessor);
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
    bool interfaceContext,
    bool allowReadonlyGetter)
{
    IndexerParseState state;
    state.type = elementType;
    state.accessibility = accessibility;
    state.beginIndex = beginIndex;
    state.isNew = isNew;
    state.isVirtual = isVirtual;
    state.isOverride = isOverride;
    state.isAbstract = isAbstract;
    state.isSealed = isSealed;
    state.isReferenceReturn = isReferenceReturn;
    state.interfaceContext = interfaceContext;
    state.allowReadonlyGetter = allowReadonlyGetter;
    state.indexer = createIndexerDeclaration(state);
    parseIndexerParameters(state);
    expect(TokenKind::LeftBrace, "'{'");
    parseIndexerAccessors(state);
    expect(TokenKind::RightBrace, "'}'");
    if (state.interfaceContext && !state.sawBody &&
        !state.indexer->hasModifier(TokenKind::KwAbstract))
        state.indexer->addModifier(TokenKind::KwAbstract);
    if (!state.sawGet)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.indexer->range(),
            "set-only indexers are not part of the Stage 38 subset");
    state.indexer->setRange(tokenRangeFrom(beginIndex));
    return state.indexer;
}

ast::AstNode* Parser::createIndexerDeclaration(IndexerParseState& state)
{
    const SourceRange range = consume().range();
    ast::AstNode* indexer = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IndexerDeclaration, range);
    indexer->setText("Item");
    indexer->setAuxiliaryText("indexer");
    indexer->setTypeSyntax(state.type);
    indexer->setReference(state.isReferenceReturn);
    if (state.accessibility != TokenKind::Invalid)
    {
        indexer->setTokenKind(state.accessibility);
        indexer->addModifier(state.accessibility);
    }
    if (state.isVirtual) indexer->addModifier(TokenKind::KwVirtual);
    if (state.isNew) indexer->addModifier(TokenKind::KwNew);
    if (state.isOverride) indexer->addModifier(TokenKind::KwOverride);
    if (state.isAbstract) indexer->addModifier(TokenKind::KwAbstract);
    if (state.isSealed) indexer->addModifier(TokenKind::KwSealed);
    return indexer;
}

void Parser::parseIndexerParameters(IndexerParseState& state)
{
    expect(TokenKind::LeftBracket, "'['");
    if (!at(TokenKind::RightBracket))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parseParameter())
                state.parameters.push_back(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightBracket, "']'");
    if (state.parameters.empty())
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            state.indexer->range(),
            "indexer must declare at least one index parameter");
}

void Parser::parseIndexerAccessors(IndexerParseState& state)
{
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (ast::AstNode* accessor = parseIndexerAccessor(state))
            state.indexer->addChild(accessor);
        else
        {
            synchronizeStatement();
            if (at(TokenKind::RightBrace)) break;
        }
    }
}

ast::AstNode* Parser::parseIndexerAccessor(IndexerParseState& state)
{
    TokenKind accessorAccessibility = TokenKind::Invalid;
    if (isAccessibilityModifier(current().kind()))
    {
        accessorAccessibility = consume().kind();
        if (state.interfaceContext)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                tokens_[index_ - 1].range(),
                "interface indexer accessors cannot narrow accessibility in Stage 38");
    }
    const bool accessorReadonly = consumeIf(TokenKind::KwReadonly);
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
        return nullptr;
    }
    if (accessorReadonly && (!getter || !state.allowReadonlyGetter))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[index_ - 1].range(),
            getter
                ? "a readonly indexer getter is valid only on an instance struct indexer"
                : "readonly is valid only on an indexer get accessor");
    const SourceRange range = consume().range();
    if ((getter && state.sawGet) || (setter && state.sawSet))
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            range,
            getter ? "duplicate indexer get accessor" : "duplicate indexer set accessor");
    state.sawGet = state.sawGet || getter;
    state.sawSet = state.sawSet || setter;
    if (state.isReferenceReturn && setter)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "a reference-returning indexer is get-only and cannot declare a setter");
    const TokenKind effective = accessorAccessibility != TokenKind::Invalid
        ? accessorAccessibility : state.accessibility;
    ast::AstNode* accessor = createIndexerAccessorNode(
        state, getter, initOnly, accessorReadonly, effective, range);
    parseThrowsClause(accessor);
    parseIndexerAccessorBody(state, accessor, range);
    accessor->setRange(tokenRangeFrom(state.beginIndex));
    return accessor;
}

ast::AstNode* Parser::createIndexerAccessorNode(
    IndexerParseState& state,
    bool getter,
    bool initOnly,
    bool accessorReadonly,
    TokenKind accessibility,
    SourceRange range)
{
    ast::TypeSyntax* returnType = getter
        ? state.type
        : arena_.make<ast::TypeSyntax>(range, TokenKind::KwVoid, "void");
    ast::AstNode* accessor = makeAccessorMethod(
        getter ? "get_Item" : "set_Item",
        returnType, accessibility, false, state.isVirtual, state.isOverride,
        state.isAbstract, state.isSealed,
        std::string("indexer:") + (getter ? "get:Item" : "set:Item") +
            (initOnly ? ":init" : ""),
        range);
    if (getter) accessor->setReference(state.isReferenceReturn);
    if (state.isNew) accessor->addModifier(TokenKind::KwNew);
    if (accessorReadonly && getter && state.allowReadonlyGetter)
        accessor->addModifier(TokenKind::KwReadonly);
    for (ast::AstNode* parameter : state.parameters)
        accessor->addChild(parameter);
    if (!getter)
    {
        ast::AstNode* value = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, range);
        value->setText("value");
        value->setTypeSyntax(state.type);
        accessor->addChild(value);
    }
    return accessor;
}

void Parser::parseIndexerAccessorBody(
    IndexerParseState& state,
    ast::AstNode* accessor,
    SourceRange accessorRange)
{
    if (consumeIf(TokenKind::Semicolon))
    {
        if (state.interfaceContext || state.isAbstract)
        {
            if (!accessor->hasModifier(TokenKind::KwAbstract))
                accessor->addModifier(TokenKind::KwAbstract);
        }
        else
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                accessorRange,
                "concrete Stage 38 indexers require explicit accessor bodies");
        return;
    }
    if (at(TokenKind::LeftBrace))
    {
        state.sawBody = true;
        if (state.isAbstract)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "abstract/interface indexer accessors cannot declare bodies");
        accessor->setFirst(parseBlock());
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "indexer accessor requires ';' or a block body");
    synchronizeStatement();
}

ast::AstNode* Parser::parseOperatorTail(
    ast::TypeSyntax* returnType,
    TokenKind accessibility,
    bool isStatic,
    std::size_t beginIndex,
    bool allowBodyless)
{
    const SourceRange operatorKeywordRange = consume().range(); // operator
    if (!isMemberOperatorToken(current().kind()))
    {
        const std::string_view spelling = memberOperatorSpelling(current().kind());
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
    // NS304-T08-F04: name the operator method (operator<symbol>) so namespace/operator lookup can
    // find the extension operator; the operator: metadata keeps the trait/overload classification.
    method->setText("operator" + std::string(memberOperatorSpelling(operation.kind())));
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

    parseCallableParameterList(method);
    parseThrowsClause(method);
    parseFloatingPointModeClause(method);
    parseFfiUnwindClause(method);
    method->setText(memberOperatorMethodName(operation.kind(), method->children().size()));

    validateOperatorArity(method, operation);
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

    parseOperatorBody(method, allowBodyless);
    if (method->first() != nullptr && method->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyMemberFloatingPointMode(method->first(), method->floatingPointMode());
    method->setRange(tokenRangeFrom(beginIndex));
    return method;
}

void Parser::parseCallableParameterList(ast::AstNode* callable)
{
    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parseParameter())
                callable->addChild(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
}

void Parser::validateOperatorArity(
    ast::AstNode* method,
    const lexer::Token& operation)
{
    const std::size_t arity = method->children().size();
    const bool unaryOnly = operation.kind() == TokenKind::Bang ||
        operation.kind() == TokenKind::Tilde;
    const bool binaryOnly = operation.kind() != TokenKind::Plus &&
        operation.kind() != TokenKind::Minus && operation.kind() != TokenKind::Bang &&
        operation.kind() != TokenKind::Tilde && operation.kind() != TokenKind::PlusPlus &&
        operation.kind() != TokenKind::MinusMinus;
    if ((unaryOnly && arity != 1) || (binaryOnly && arity != 2) ||
        ((operation.kind() == TokenKind::Plus || operation.kind() == TokenKind::Minus) &&
         arity != 1 && arity != 2) ||
        ((operation.kind() == TokenKind::PlusPlus ||
          operation.kind() == TokenKind::MinusMinus) && arity != 1))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            method->range(),
            "user-defined operator has invalid arity for its token");
}

void Parser::parseOperatorBody(ast::AstNode* method, bool allowBodyless)
{
    if (at(TokenKind::LeftBrace)) method->setFirst(parseBlock());
    else if (allowBodyless && consumeIf(TokenKind::Semicolon))
        method->addModifier(TokenKind::KwAbstract);
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "user-defined operator requires a body");
        if (!consumeIf(TokenKind::Semicolon)) synchronizeStatement();
    }
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

    parseCallableParameterList(method);
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
    std::size_t beginIndex,
    bool isPartial)
{
    const lexer::Token& name = consume();
    ast::AstNode* constructor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConstructorDeclaration,
        name.range());
    constructor->setText(std::string(className));
    if (isStatic) constructor->addModifier(TokenKind::KwStatic);
    if (accessibility != TokenKind::Invalid)
    {
        constructor->setTokenKind(accessibility);
        constructor->addModifier(accessibility);
    }
    if (isNoHeap) constructor->addModifier(TokenKind::KwNoHeap);
    if (peek().kind() != TokenKind::RightParen && isStatic)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            peek().range(),
            "static constructors cannot declare parameters");
    parseCallableParameterList(constructor);
    parseThrowsClause(constructor);
    parseConstructorInitializer(constructor, isStatic);
    parseConstructorBody(constructor, isPartial);
    if (isPartial)
    {
        std::string marker = constructor->auxiliaryText();
        if (!marker.empty()) marker += ';';
        marker += "partial-method";
        constructor->setAuxiliaryText(std::move(marker));
    }
    constructor->setRange(tokenRangeFrom(beginIndex));
    return constructor;
}

void Parser::parseConstructorInitializer(
    ast::AstNode* constructor,
    bool isStatic)
{
    if (!consumeIf(TokenKind::Colon)) return;
    const SourceRange begin = tokens_[index_ - 1].range();
    if (isStatic)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            begin,
            "static constructors cannot declare base(...) or this(...) initializers");
    const bool baseInitializer = at(TokenKind::KwBase);
    const bool thisInitializer = at(TokenKind::KwThis);
    if (!baseInitializer && !thisInitializer)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "constructor initializer must name base(...) or this(...)");
        return;
    }
    consume();
    ast::AstNode* initializer = arena_.make<ast::AstNode>(
        baseInitializer
            ? ast::AstNodeKind::BaseConstructorInitializer
            : ast::AstNodeKind::ThisConstructorInitializer,
        begin);
    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* argument = parseArgument())
                initializer->addChild(argument);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    initializer->setRange(
        SourceRange(begin.begin(), tokens_[index_ - 1].range().end()));
    constructor->setSecond(initializer);
}

void Parser::parseConstructorBody(
    ast::AstNode* constructor,
    bool isPartial)
{
    if (at(TokenKind::LeftBrace))
    {
        constructor->setFirst(parseBlock());
        return;
    }
    if (isPartial)
    {
        expect(TokenKind::Semicolon, "';'");
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "constructor declarations require a body in Stage 34");
    expect(TokenKind::LeftBrace, "constructor body");
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
    bool allowBodyless,
    bool isPartialMethod)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, name.range());
    method->setTypeSyntax(returnType);
    method->setText(std::string(name.lexeme()));
    applyMethodModifiers(
        method, accessibility, isNew, isStatic, isVirtual, isOverride,
        isAbstract, isSealed, isExported, isExternC, isInline,
        isForceInline, isNoInline, isNoHeap, isUnsafe);
    parseGenericParameterList(method);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, method);
    parseCallableParameterList(method);
    parseThrowsClause(method);
    parseFloatingPointModeClause(method);
    parseFfiUnwindClause(method);
    parseGenericConstraintClauses(method);
    parseMethodBody(method, isAbstract, allowBodyless, isPartialMethod);
    if (method->first() != nullptr && method->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyMemberFloatingPointMode(method->first(), method->floatingPointMode());
    method->setRange(tokenRangeFrom(beginIndex));
    return method;
}

void Parser::applyMethodModifiers(
    ast::AstNode* method,
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
    bool isUnsafe)
{
    if (accessibility != TokenKind::Invalid)
    {
        method->setTokenKind(accessibility);
        method->addModifier(accessibility);
    }
    if (isStatic) method->addModifier(TokenKind::KwStatic);
    if (isNew) method->addModifier(TokenKind::KwNew);
    if (isVirtual) method->addModifier(TokenKind::KwVirtual);
    if (isOverride) method->addModifier(TokenKind::KwOverride);
    if (isAbstract) method->addModifier(TokenKind::KwAbstract);
    if (isSealed) method->addModifier(TokenKind::KwSealed);
    if (isExported) method->addModifier(TokenKind::KwExport);
    if (isExternC) method->addModifier(TokenKind::KwExtern);
    if (isInline) method->addModifier(TokenKind::KwInline);
    if (isForceInline) method->addModifier(TokenKind::KwForceInline);
    if (isNoInline) method->addModifier(TokenKind::KwNoInline);
    if (isNoHeap) method->addModifier(TokenKind::KwNoHeap);
    method->setUnsafe(isUnsafe);
}

void Parser::parseMethodBody(
    ast::AstNode* method,
    bool isAbstract,
    bool allowBodyless,
    bool isPartialMethod)
{
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
        else expect(TokenKind::Semicolon, "';'");
        return;
    }
    if (at(TokenKind::LeftBrace))
    {
        method->setFirst(parseBlock());
        return;
    }
    if (allowBodyless || isPartialMethod)
    {
        expect(TokenKind::Semicolon, "';'");
        if (!isPartialMethod) method->addModifier(TokenKind::KwAbstract);
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "non-abstract class methods require a body in Stage 35");
    if (!consumeIf(TokenKind::Semicolon)) synchronizeStatement();
}

ast::AstNode* Parser::parseFunctionTail(
    ast::TypeSyntax* returnType,
    const lexer::Token& name,
    bool externalOnly)
{
    const std::size_t beginIndex = index_ > 0 ? index_ - 1 : 0;
    ast::AstNode* function = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FunctionDeclaration, name.range());
    function->setTypeSyntax(returnType);
    function->setText(std::string(name.lexeme()));
    function->setFlag(externalOnly);
    parseGenericParameterList(function);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, function);
    const bool variadic = parseVariadicCallableParameters(function);
    function->setVariadic(variadic);
    parseThrowsClause(function);
    parseFloatingPointModeClause(function);
    parseFfiUnwindClause(function);
    if (variadic && !externalOnly)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            function->range(),
            "'...' is valid only in a body-less extern C declaration");
    parseGenericConstraintClauses(function);
    validateGlobalFunctionConstraints(function);
    parseGlobalFunctionBody(function, externalOnly);
    if (function->first() != nullptr && function->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        applyMemberFloatingPointMode(
            function->first(), function->floatingPointMode());
    function->setRange(tokenRangeFrom(beginIndex));
    return function;
}

bool Parser::parseVariadicCallableParameters(ast::AstNode* callable)
{
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
                    diagnostics_.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        current().range(),
                        "a variadic C declaration requires at least one fixed parameter before '...'");
                consume();
                variadic = true;
                break;
            }
            if (ast::AstNode* parameter = parseParameter())
            {
                callable->addChild(parameter);
                sawFixedParameter = true;
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    return variadic;
}

void Parser::validateGlobalFunctionConstraints(ast::AstNode* function)
{
    if (function->genericConstraints().empty()) return;
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        function->genericConstraints().front().range,
        "a generic constraint on a global function is not supported yet: a global generic "
        "function is not materialized, so the clause would never be checked; declare '" +
            function->text() + "' as a static method of a class");
    function->clearGenericConstraints();
}

void Parser::parseGlobalFunctionBody(
    ast::AstNode* function,
    bool externalOnly)
{
    if (externalOnly)
    {
        if (consumeIf(TokenKind::Semicolon)) return;
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected ';'");
        if (at(TokenKind::LeftBrace)) (void)parseBlock();
        else synchronizeGlobal();
        return;
    }
    if (consumeIf(TokenKind::Semicolon)) return;
    if (at(TokenKind::LeftBrace))
    {
        function->setFirst(parseBlock());
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected ';' or function body");
    synchronizeGlobal();
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
    parseParameterContracts(parameter);
    if (consumeIf(TokenKind::Equal))
    {
        parameter->setFirst(parseAssignmentExpression());
        parameter->setRange(tokenRangeFrom(beginIndex));
    }
    return parameter;
}

void Parser::parseParameterContracts(ast::AstNode* parameter)
{
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
                appendParameterContract(
                    parameter, "ffi:length:" + std::string(consume().lexeme()));
        }
        else if (atContextualKeyword("scoped") || atContextualKeyword("escaping"))
        {
            appendParameterContract(
                parameter, "ffi:callback:" + std::string(consume().lexeme()));
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
}

void Parser::appendParameterContract(
    ast::AstNode* parameter,
    std::string contract)
{
    std::string marker = parameter->auxiliaryText();
    if (!marker.empty()) marker += '|';
    marker += std::move(contract);
    parameter->setAuxiliaryText(std::move(marker));
}
}
