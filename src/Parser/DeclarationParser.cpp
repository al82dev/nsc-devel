#include "nativesharp/Parser/Parser.h"
#include "KnownGenericTypeScope.h"
#include "ParsedVariantCase.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}

// Parses the compilation-unit facade while guaranteeing progress after a failed declaration.
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

// Dispatches one namespace member, preserving contextual attributes and async markers.
ast::AstNode* Parser::parseNamespaceMember()
{
    // NS304-T05-F08: `[FrameStorage(Stack|Arena|Heap)]` attribute selects the async frame storage
    // (surface #1). It is parsed here and recorded in the callable marker for the member model.
    std::string frameStorage;
    parseFrameStorageAttribute(frameStorage);
    // NS304-T05-F01: `async` se consume como modifier (ya no se rechaza con NS3049) y se guarda
    // como marcador "async" en el auxiliaryText del callable global; el lowering es T05-F03.
    bool isAsync = false;
    if (atContextualKeyword("async"))
    {
        isAsync = true;
        consume();
    }
    // NS304-T08-F06: `public`/`internal` on an extension block selects the namespace-function
    // accessibility for every member (the receiver's methods keep their own visibility).
    if (at(TokenKind::KwPublic) || at(TokenKind::KwInternal))
    {
        const TokenKind accessibility = consume().kind();
        if (atContextualKeyword("extension"))
        {
            return parseExtensionDeclaration(accessibility);
        }
        --index_;
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
    applyNamespaceCallableAttributes(declaration, isAsync, frameStorage);
    return declaration;
}

// Parses an explicit import and delegates its bounded token and AST construction steps.
ast::AstNode* Parser::parseImportDirective(bool reexport)
{
    const std::size_t beginIndex = index_;
    consume(); // contextual `import`
    ExplicitImportKind kind = parseExplicitImportKind();

    lexer::Token first = current();
    lexer::Token last = first;
    std::string target;
    if (!parseExplicitImportTarget(kind, first, last, target)) return nullptr;

    std::string alias;
    if (!parseExplicitImportAlias(alias, last)) return nullptr;
    if ((kind == ExplicitImportKind::Extensions ||
         kind == ExplicitImportKind::RestrictedGlob) && !alias.empty())
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidUsingTarget,
            diagnostics::DiagnosticSeverity::Error,
            tokenRangeFrom(beginIndex),
            "extension and restricted-glob imports cannot declare an alias");
    }
    expect(TokenKind::Semicolon, "';'");
    return makeExplicitImport(
        beginIndex, reexport, kind, first, last, std::move(target), std::move(alias));
}

Parser::ExplicitImportKind Parser::parseExplicitImportKind()
{
    if (atContextualKeyword("module"))
    {
        consume();
        return ExplicitImportKind::Module;
    }
    if (atContextualKeyword("symbol"))
    {
        consume();
        return ExplicitImportKind::Symbol;
    }
    if (atContextualKeyword("trait"))
    {
        consume();
        return ExplicitImportKind::Trait;
    }
    if (atContextualKeyword("extensions"))
    {
        consume();
        return ExplicitImportKind::Extensions;
    }
    return ExplicitImportKind::Module;
}

bool Parser::parseExplicitImportTarget(
    ExplicitImportKind& kind,
    lexer::Token& first,
    lexer::Token& last,
    std::string& target)
{
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "module, symbol or trait import target");
        synchronizeGlobal();
        return false;
    }
    first = current();
    last = first;
    target = std::string(first.lexeme());
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
    return true;
}

bool Parser::parseExplicitImportAlias(std::string& alias, lexer::Token& last)
{
    if (!at(TokenKind::KwAs) && !atContextualKeyword("as")) return true;
    consume();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "local import alias");
        synchronizeGlobal();
        return false;
    }
    alias = std::string(current().lexeme());
    last = consume();
    return true;
}

ast::AstNode* Parser::makeExplicitImport(
    std::size_t beginIndex,
    bool reexport,
    ExplicitImportKind kind,
    const lexer::Token& first,
    const lexer::Token& last,
    std::string target,
    std::string alias)
{
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

ast::AstNode* Parser::parseExtensionDeclaration(TokenKind accessibility)
{
    const std::size_t beginIndex = index_;
    const SourceRange keywordRange = consume().range();
    TokenKind receiverPass = TokenKind::Invalid;
    if (atContextualKeyword("ref")) receiverPass = consume().kind();
    else if (at(TokenKind::KwIn)) receiverPass = consume().kind();

    std::vector<std::string> receiverGenericArguments = scanExtensionGenericArguments();
    for (const std::string& name : receiverGenericArguments) knownTypeNames_.insert(name);
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
    for (const std::string& name : receiverGenericArguments)
        extension->addGenericParameter(name);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, extension);
    parseGenericConstraintClauses(extension);
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
        addExtensionMember(
            extension, parseGlobalDeclaration(), receiverType, receiverPass,
            accessibility, receiverGenericArguments);
        if (index_ == before) consume();
    }
    parsingExtensionBody_ = previousExtensionContext;
    expect(TokenKind::RightBrace, "'}' after extension block");
    extension->setRange(tokenRangeFrom(beginIndex));
    return extension;
}

std::vector<std::string> Parser::scanExtensionGenericArguments() const
{
    std::vector<std::string> arguments;
    for (std::size_t cursor = index_; cursor < tokens_.size(); ++cursor)
    {
        if (tokens_[cursor].kind() == TokenKind::Less)
        {
            for (std::size_t inner = cursor + 1;
                 inner < tokens_.size() && tokens_[inner].kind() != TokenKind::Greater &&
                 tokens_[inner].kind() != TokenKind::EndOfFile; ++inner)
            {
                if (tokens_[inner].kind() == TokenKind::Identifier)
                    arguments.emplace_back(tokens_[inner].lexeme());
            }
            break;
        }
        if (tokens_[cursor].kind() == TokenKind::LeftBrace ||
            tokens_[cursor].kind() == TokenKind::Semicolon ||
            tokens_[cursor].kind() == TokenKind::EndOfFile)
        {
            break;
        }
    }
    return arguments;
}

ast::AstNode* Parser::makeExtensionReceiver(
    ast::TypeSyntax* receiverType,
    TokenKind receiverPass)
{
    ast::AstNode* receiver = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ParameterDeclaration, receiverType->range());
    receiver->setText("__nsc_extension_this");
    ast::TypeSyntax* parameterType = cloneDeclarationType(receiverType);
    if (receiverPass != TokenKind::Invalid)
    {
        parameterType->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Pointer, nullptr, parameterType->range()});
        receiver->setAuxiliaryText(receiverPass == TokenKind::KwIn ? "pass:in" : "pass:ref");
    }
    receiver->setTypeSyntax(parameterType);
    return receiver;
}

void Parser::addExtensionMember(
    ast::AstNode* extension,
    ast::AstNode* member,
    ast::TypeSyntax* receiverType,
    TokenKind receiverPass,
    TokenKind accessibility,
    const std::vector<std::string>& genericArguments)
{
    if (member == nullptr) return;
    if (member->kind() == ast::AstNodeKind::FunctionDeclaration)
    {
        if (member->hasModifier(TokenKind::KwStatic))
            member->setAuxiliaryText("ns3-extension-static");
        else
        {
            member->prependChild(makeExtensionReceiver(receiverType, receiverPass));
            member->setAuxiliaryText("ns3-extension");
        }
        if (accessibility != TokenKind::Invalid)
        {
            member->setTokenKind(accessibility);
            member->addModifier(accessibility);
        }
        for (const std::string& name : genericArguments) member->addGenericParameter(name);
        extension->addChild(member);
        return;
    }
    if (member->kind() == ast::AstNodeKind::MethodDeclaration &&
        member->auxiliaryText().find("ns3-extension-static") != std::string::npos)
    {
        extension->addChild(member);
        return;
    }
    if (member->kind() == ast::AstNodeKind::PropertyDeclaration)
    {
        addExtensionPropertyAccessors(extension, member, receiverType, receiverPass);
        return;
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        member->range(),
        "an extension block may contain only namespace-function declarations");
}

void Parser::addExtensionPropertyAccessors(
    ast::AstNode* extension,
    ast::AstNode* property,
    ast::TypeSyntax* receiverType,
    TokenKind receiverPass)
{
    const std::string propertyName = property->text();
    for (ast::AstNode* accessor : property->children())
    {
        if (accessor == nullptr || accessor->kind() != ast::AstNodeKind::MethodDeclaration)
            continue;
        const bool isGetter = accessor->text() == "get_" + propertyName;
        const bool isSetter = accessor->text() == "set_" + propertyName;
        if (!isGetter && !isSetter) continue;
        ast::AstNode* function = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FunctionDeclaration, accessor->range());
        function->setText(isGetter ? propertyName : accessor->text());
        function->setTypeSyntax(cloneDeclarationType(accessor->typeSyntax()));
        function->setReference(accessor->isReference());
        function->setFirst(accessor->first());
        function->setTokenKind(accessor->tokenKind());
        function->prependChild(makeExtensionReceiver(receiverType, receiverPass));
        for (ast::AstNode* parameter : accessor->children())
            function->addChild(cloneDeclarationNode(parameter));
        function->setAuxiliaryText("ns3-extension");
        extension->addChild(function);
    }
}

// Parses a block-scoped or file-scoped namespace and preserves declaration order and recovery.
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

// Dispatches namespace aliases and type aliases after consuming `using`.
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
        return parseUsingAliasDirective(beginIndex, first);
    }
    return parseUsingNamespaceDirective(beginIndex, first);
}

// Parses a generic or non-generic type alias through the shared type grammar.
ast::AstNode* Parser::parseUsingAliasDirective(
    std::size_t beginIndex,
    const lexer::Token& first)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingAliasDirective,
        first.range());
    node->setText(std::string(first.lexeme()));
    consume();
    // Item 276. An alias may bind type parameters exactly where a generic declaration does.
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
    ast::TypeSyntax* target = parseType();
    if (target == nullptr)
    {
        synchronizeGlobal();
        return nullptr;
    }
    expect(TokenKind::Semicolon, "';'");
    node->setRange(tokenRangeFrom(beginIndex));
    node->setTypeSyntax(target);
    knownTypeNames_.insert(std::string(first.lexeme()));
    if (node->genericParameters().empty() && target->modifiers().empty() &&
        target->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None &&
        target->typeArguments().empty() && !lexer::isBuiltinTypeKeyword(target->baseTokenKind()))
    {
        node->setAuxiliaryText(target->baseName());
    }
    return node;
}

// Parses a namespace import target and retains its qualified source spelling.
ast::AstNode* Parser::parseUsingNamespaceDirective(
    std::size_t beginIndex,
    const lexer::Token& first)
{
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

// Parses interface or trait declarations while retaining the generic parameter scope.
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
    parseInterfaceBaseTypes(node);
    parseGenericConstraintClauses(node);

    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    parseInterfaceBody(node, isTrait);
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

// Parses the optional comma-separated interface base list.
void Parser::parseInterfaceBaseTypes(ast::AstNode* declaration)
{
    if (!consumeIf(TokenKind::Colon)) return;
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
        if (declaration->typeSyntax() == nullptr)
        {
            declaration->setTypeSyntax(specification);
        }
        declaration->addTypeSyntax(specification);
    }
    while (consumeIf(TokenKind::Comma));
}

// Parses interface members and retains trait object-safety diagnostics and recovery progress.
void Parser::parseInterfaceBody(ast::AstNode* declaration, bool isTrait)
{
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
            declaration->addChild(member);
        }
        if (index_ == before)
        {
            consume();
        }
    }
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
    validateTraitImplementationTarget(node, target);
    parseGenericConstraintClauses(node);
    if (!expect(TokenKind::LeftBrace, "'{' after impl target"))
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    parseTraitImplementationBody(node);
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

void Parser::validateTraitImplementationTarget(
    ast::AstNode* declaration,
    ast::TypeSyntax* target)
{
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
        return;
    }
    declaration->setText(target->baseName());
}

void Parser::parseTraitImplementationBody(ast::AstNode* declaration)
{
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* member =
            ((atContextualKeyword("type") && peek().kind() == TokenKind::Identifier) ||
             at(TokenKind::KwConst))
            ? parseAssociatedItem(true)
            : parseClassMember(declaration->text());
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
            else if (associated) declaration->addChild(member);
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
                declaration->addChild(member);
            }
        }
        if (index_ == before) consume();
    }
}

// Parses a delegate declaration while preserving generic scope and callable clauses.
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
    parseDelegateParameters(node);
    parseThrowsClause(node);
    parseFfiUnwindClause(node);
    parseGenericConstraintClauses(node);
    expect(TokenKind::Semicolon, "';'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

void Parser::parseDelegateParameters(ast::AstNode* declaration)
{
    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parseParameter(); parameter != nullptr)
            {
                declaration->addChild(parameter);
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
}

void Parser::parseVariantPayload(ParsedVariantCase& item, const lexer::Token& caseName)
{
    if (!consumeIf(TokenKind::LeftParen)) return;
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

void Parser::parseVariantCases(
    std::vector<ParsedVariantCase>& cases,
    std::unordered_set<std::string>& caseNames)
{
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "variant case name");
            consume();
            continue;
        }
        const lexer::Token& caseName = consume();
        ParsedVariantCase item{std::string(caseName.lexeme()), caseName.range(), nullptr};
        const bool uniqueCase = caseNames.insert(item.name).second;
        if (!uniqueCase)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                caseName.range(),
                "duplicate variant case '" + item.name + "'");
        }
        parseVariantPayload(item, caseName);
        if (uniqueCase) cases.push_back(std::move(item));
        if (!consumeIf(TokenKind::Comma) && !at(TokenKind::RightBrace))
            expect(TokenKind::Comma, "',' between variant cases");
    }
}

void Parser::validateVariantGeneratedNames(const std::vector<ParsedVariantCase>& cases)
{
    std::unordered_set<std::string> generatedNames = {"CaseTag"};
    for (const ParsedVariantCase& item : cases)
    {
        const std::array<std::string, 2> names = {item.name, "Is" + item.name};
        for (const std::string& generatedName : names)
        {
            if (!generatedNames.insert(generatedName).second)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    item.range,
                    "variant case '" + item.name +
                        "' collides with generated member '" + generatedName + "'");
            }
        }
    }
}

ast::TypeSyntax* Parser::makeVariantTagType(
    SourceRange range,
    TokenKind tagKind,
    std::string_view tagTypeName)
{
    return arena_.make<ast::TypeSyntax>(
        range, tagKind, std::string(tagTypeName));
}

ast::TypeSyntax* Parser::makeVariantSelfType(
    SourceRange range,
    std::string_view variantName,
    const ast::AstNode* declaration)
{
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        range, TokenKind::Identifier, std::string(variantName));
    for (const std::string& parameter : declaration->genericParameters())
    {
        type->addTypeArgument(arena_.make<ast::TypeSyntax>(
            range, TokenKind::Identifier, parameter));
    }
    return type;
}

ast::AstNode* Parser::makeVariantBlock(SourceRange range)
{
    return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
}

ast::AstNode* Parser::makeVariantReturn(ast::AstNode* expression, SourceRange range)
{
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ReturnStatement, range);
    statement->setFirst(expression);
    return statement;
}

ast::AstNode* Parser::makeVariantMove(ast::AstNode* expression, SourceRange range)
{
    ast::AstNode* move = arena_.make<ast::AstNode>(ast::AstNodeKind::MoveExpression, range);
    move->setFirst(expression);
    return move;
}

void Parser::markVariantPublic(ast::AstNode* member)
{
    member->setTokenKind(TokenKind::KwPublic);
    member->addModifier(TokenKind::KwPublic);
}

void Parser::markVariantPrivate(ast::AstNode* member)
{
    member->setTokenKind(TokenKind::KwPrivate);
    member->addModifier(TokenKind::KwPrivate);
}

void Parser::addVariantStorage(
    ast::AstNode* declaration,
    const std::vector<ParsedVariantCase>& cases,
    SourceRange range,
    TokenKind tagKind,
    std::string_view tagTypeName,
    std::string_view variantName,
    std::string& payloadFieldName)
{
    ast::AstNode* tagField = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, range);
    tagField->setText("__nsc_variant_tag");
    tagField->setTypeSyntax(makeVariantTagType(range, tagKind, tagTypeName));
    tagField->setAuxiliaryText("variant-tag");
    markVariantPrivate(tagField);
    declaration->addChild(tagField);
    const bool hasPayload = std::ranges::any_of(
        cases, [](const ParsedVariantCase& item) { return item.payloadType != nullptr; });
    payloadFieldName = "__nsc_variant_payload";
    if (!hasPayload) return;
    const std::string payloadTypeName = "__nsc_variant_payload_" + std::string(variantName);
    ast::AstNode* payloadDeclaration = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StructDeclaration, range);
    payloadDeclaration->setText(payloadTypeName);
    payloadDeclaration->setAuxiliaryText("union,variant-payload");
    payloadDeclaration->setTokenKind(TokenKind::KwPrivate);
    for (const ParsedVariantCase& item : cases)
    {
        if (item.payloadType == nullptr) continue;
        ast::AstNode* field = arena_.make<ast::AstNode>(
            ast::AstNodeKind::FieldDeclaration, item.range);
        field->setText("__nsc_case_" + item.name);
        field->setTypeSyntax(item.payloadType);
        field->setAuxiliaryText("variant-case-payload:" + item.name);
        markVariantPublic(field);
        payloadDeclaration->addChild(field);
    }
    declaration->addChild(payloadDeclaration);
    ast::AstNode* payloadField = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, range);
    payloadField->setText(payloadFieldName);
    payloadField->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        range, TokenKind::Identifier, payloadTypeName));
    payloadField->setAuxiliaryText("variant-payload-carrier");
    markVariantPrivate(payloadField);
    declaration->addChild(payloadField);
}

void Parser::addVariantFactory(
    ast::AstNode* declaration,
    const ParsedVariantCase& item,
    std::size_t caseIndex,
    SourceRange variantRange,
    std::string_view variantName,
    std::string_view payloadFieldName)
{
    ast::AstNode* factory = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, item.range);
    factory->setText(item.name);
    factory->setTypeSyntax(makeVariantSelfType(variantRange, variantName, declaration));
    factory->setAuxiliaryText(
        "variant-case-factory:" + item.name + ":" + std::to_string(caseIndex));
    markVariantPublic(factory);
    factory->addModifier(TokenKind::KwStatic);
    if (item.payloadType != nullptr)
    {
        ast::AstNode* parameter = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, item.range);
        parameter->setText("value");
        parameter->setTypeSyntax(item.payloadType);
        factory->addChild(parameter);
    }
    ast::AstNode* body = makeVariantBlock(item.range);
    ast::AstNode* defaultValue = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DefaultValueExpression, item.range);
    defaultValue->setTypeSyntax(makeVariantSelfType(variantRange, variantName, declaration));
    body->addChild(makeVariableDeclaration(
        makeVariantSelfType(variantRange, variantName, declaration),
        "__nsc_variant_result", defaultValue, false));
    body->addChild(makeExpressionStatement(makeAssignmentExpression(
        makeMemberExpression(
            makeIdentifierExpression(item.range, "__nsc_variant_result"),
            "__nsc_variant_tag"),
        makeIntegerLiteralExpression(item.range, caseIndex))));
    if (item.payloadType != nullptr)
    {
        ast::AstNode* payload = makeMemberExpression(
            makeIdentifierExpression(item.range, "__nsc_variant_result"),
            std::string(payloadFieldName));
        payload = makeMemberExpression(payload, "__nsc_case_" + item.name);
        ast::AstNode* payloadValue = makeVariantMove(
            makeIdentifierExpression(item.range, "value"), item.range);
        payloadValue->setAuxiliaryText("variant-payload-transfer");
        body->addChild(makeExpressionStatement(makeAssignmentExpression(payload, payloadValue)));
    }
    ast::AstNode* resultMove = makeVariantMove(
        makeIdentifierExpression(item.range, "__nsc_variant_result"), item.range);
    resultMove->setAuxiliaryText("variant-result-transfer");
    body->addChild(makeVariantReturn(resultMove, item.range));
    factory->setFirst(body);
    declaration->addChild(factory);
}

void Parser::addVariantTest(
    ast::AstNode* declaration,
    const ParsedVariantCase& item,
    std::size_t caseIndex)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, item.range);
    method->setText("Is" + item.name);
    method->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        item.range, TokenKind::KwBool, "bool"));
    method->setAuxiliaryText(
        "variant-case-test:" + item.name + ":" + std::to_string(caseIndex));
    markVariantPublic(method);
    method->addModifier(TokenKind::KwReadonly);
    ast::AstNode* body = makeVariantBlock(item.range);
    body->addChild(makeVariantReturn(makeBinaryExpression(
        TokenKind::EqualEqual,
        makeIdentifierExpression(item.range, "__nsc_variant_tag"),
        makeIntegerLiteralExpression(item.range, caseIndex)), item.range));
    method->setFirst(body);
    declaration->addChild(method);
}

void Parser::addVariantExtract(
    ast::AstNode* declaration,
    const ParsedVariantCase& item,
    std::size_t caseIndex,
    std::string_view payloadFieldName)
{
    if (item.payloadType == nullptr) return;
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, item.range);
    method->setText("__nsc_variant_extract_" + item.name);
    method->setTypeSyntax(item.payloadType);
    method->setAuxiliaryText(
        "variant-case-extract:" + item.name + ":" + std::to_string(caseIndex));
    markVariantPublic(method);
    ast::AstNode* body = makeVariantBlock(item.range);
    ast::AstNode* payloadMove = makeVariantMove(makeMemberExpression(
        makeIdentifierExpression(item.range, std::string(payloadFieldName)),
        "__nsc_case_" + item.name), item.range);
    payloadMove->setAuxiliaryText("variant-payload-extract");
    body->addChild(makeVariantReturn(payloadMove, item.range));
    method->setFirst(body);
    declaration->addChild(method);
}

void Parser::addVariantCaseMembers(
    ast::AstNode* declaration,
    const ParsedVariantCase& item,
    std::size_t caseIndex,
    SourceRange variantRange,
    std::string_view variantName,
    std::string_view payloadFieldName)
{
    addVariantFactory(
        declaration, item, caseIndex, variantRange, variantName, payloadFieldName);
    addVariantTest(declaration, item, caseIndex);
    addVariantExtract(declaration, item, caseIndex, payloadFieldName);
}

void Parser::addVariantTagMethod(
    ast::AstNode* declaration,
    SourceRange range,
    TokenKind tagKind,
    std::string_view tagTypeName)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, range);
    method->setText("CaseTag");
    method->setTypeSyntax(makeVariantTagType(range, tagKind, tagTypeName));
    method->setAuxiliaryText("variant-case-tag");
    markVariantPublic(method);
    method->addModifier(TokenKind::KwReadonly);
    ast::AstNode* body = makeVariantBlock(range);
    body->addChild(makeVariantReturn(
        makeIdentifierExpression(range, "__nsc_variant_tag"), range));
    method->setFirst(body);
    declaration->addChild(method);
}

void Parser::setVariantMetadata(
    ast::AstNode* declaration,
    const std::vector<ParsedVariantCase>& cases,
    std::string_view tagTypeName)
{
    const bool explicitCopy = declaration->auxiliaryText().find("copy") != std::string::npos;
    std::string metadata = "variant,move-only-value";
    if (explicitCopy) metadata += ",copy";
    metadata += ",tag=" + std::string(tagTypeName) + ",cases=";
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        if (index != 0) metadata += '|';
        metadata += cases[index].name;
        if (cases[index].payloadType != nullptr) metadata += ":payload";
    }
    declaration->setAuxiliaryText(std::move(metadata));
}

ast::AstNode* Parser::parseVariantDeclaration()
{
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
    std::vector<ParsedVariantCase> cases;
    std::unordered_set<std::string> caseNames;
    parseVariantCases(cases, caseNames);
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
    validateVariantGeneratedNames(cases);
    const TokenKind tagKind = cases.size() <= 0x100U
        ? TokenKind::KwByte
        : cases.size() <= 0x10000U ? TokenKind::KwUShort : TokenKind::KwUInt;
    const std::string tagTypeName = tagKind == TokenKind::KwByte
        ? "byte" : tagKind == TokenKind::KwUShort ? "ushort" : "uint";
    std::string payloadFieldName;
    addVariantStorage(
        node, cases, name.range(), tagKind, tagTypeName, variantName, payloadFieldName);
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
    {
        addVariantCaseMembers(
            node, cases[caseIndex], caseIndex, name.range(), variantName, payloadFieldName);
    }
    addVariantTagMethod(node, name.range(), tagKind, tagTypeName);
    setVariantMetadata(node, cases, tagTypeName);
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

ast::TypeSyntax* Parser::makeOpaqueSelfType(
    SourceRange range,
    std::string_view typeName,
    const ast::AstNode* declaration)
{
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        range, TokenKind::Identifier, std::string(typeName));
    for (const std::string& parameter : declaration->genericParameters())
    {
        type->addTypeArgument(arena_.make<ast::TypeSyntax>(
            range, TokenKind::Identifier, parameter));
    }
    return type;
}

ast::AstNode* Parser::makeOpaqueReturn(ast::AstNode* expression)
{
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ReturnStatement, expression->range());
    statement->setFirst(expression);
    return statement;
}

ast::AstNode* Parser::makeOpaqueMove(ast::AstNode* expression)
{
    ast::AstNode* move = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, expression->range());
    move->setFirst(expression);
    return move;
}

void Parser::addOpaqueRepresentationField(
    ast::AstNode* declaration,
    SourceRange range,
    ast::TypeSyntax* representation,
    std::string_view representationField)
{
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, range);
    field->setText(std::string(representationField));
    field->setTypeSyntax(representation);
    field->setAuxiliaryText("opaque-representation");
    field->setTokenKind(TokenKind::KwPrivate);
    field->addModifier(TokenKind::KwPrivate);
    declaration->addChild(field);
}

void Parser::markOpaqueInternal(ast::AstNode* member)
{
    member->setTokenKind(TokenKind::KwInternal);
    member->addModifier(TokenKind::KwInternal);
}

void Parser::addOpaqueFromRepresentation(
    ast::AstNode* declaration,
    SourceRange range,
    std::string_view typeName,
    ast::TypeSyntax* representation,
    std::string_view representationField,
    bool explicitCopy)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, range);
    method->setText("FromRepresentation");
    method->setTypeSyntax(makeOpaqueSelfType(range, typeName, declaration));
    method->setAuxiliaryText("opaque-boundary:from");
    markOpaqueInternal(method);
    method->addModifier(TokenKind::KwStatic);
    ast::AstNode* parameter = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ParameterDeclaration, range);
    parameter->setText("value");
    parameter->setTypeSyntax(representation);
    method->addChild(parameter);
    ast::AstNode* body = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    ast::AstNode* defaultValue = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DefaultValueExpression, range);
    defaultValue->setTypeSyntax(makeOpaqueSelfType(range, typeName, declaration));
    body->addChild(makeVariableDeclaration(
        makeOpaqueSelfType(range, typeName, declaration),
        "__nsc_opaque_result", defaultValue, false));
    body->addChild(makeExpressionStatement(makeAssignmentExpression(
        makeMemberExpression(
            makeIdentifierExpression(range, "__nsc_opaque_result"),
            std::string(representationField)),
        makeIdentifierExpression(range, "value"))));
    ast::AstNode* result = makeIdentifierExpression(range, "__nsc_opaque_result");
    body->addChild(makeOpaqueReturn(explicitCopy ? result : makeOpaqueMove(result)));
    method->setFirst(body);
    declaration->addChild(method);
}

void Parser::addOpaqueToRepresentation(
    ast::AstNode* declaration,
    SourceRange range,
    ast::TypeSyntax* representation,
    std::string_view representationField)
{
    ast::AstNode* method = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MethodDeclaration, range);
    method->setText("ToRepresentation");
    method->setTypeSyntax(representation);
    method->setAuxiliaryText("opaque-boundary:to");
    markOpaqueInternal(method);
    method->addModifier(TokenKind::KwReadonly);
    ast::AstNode* body = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    body->addChild(makeOpaqueReturn(makeIdentifierExpression(
        range, std::string(representationField))));
    method->setFirst(body);
    declaration->addChild(method);
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
    constexpr std::string_view representationField = "__nsc_opaque_value";
    addOpaqueRepresentationField(node, name.range(), representation, representationField);
    addOpaqueFromRepresentation(
        node, name.range(), typeName, representation, representationField, explicitCopy);
    addOpaqueToRepresentation(node, name.range(), representation, representationField);
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

void Parser::applyClassModifiers(
    ast::AstNode* declaration,
    bool isAbstract,
    bool isSealed,
    bool isStatic)
{
    if (isAbstract) declaration->addModifier(TokenKind::KwAbstract);
    if (isSealed) declaration->addModifier(TokenKind::KwSealed);
    if (isStatic) declaration->addModifier(TokenKind::KwStatic);
}

ast::AstNode* Parser::parsePromotedClassParameter(
    ast::AstNode* declaration,
    std::unordered_set<std::string>& names)
{
    TokenKind accessibility = TokenKind::Invalid;
    bool readonlyField = false;
    while (isAccessibilityModifier(current().kind()) || at(TokenKind::KwReadonly))
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
        else accessibility = modifier.kind();
    }
    ast::AstNode* parameter = parseParameter();
    if (parameter == nullptr) return nullptr;
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
    declaration->addChild(field);
    return parameter;
}

void Parser::addClassPrimaryConstructor(
    ast::AstNode* declaration,
    const lexer::Token& name,
    std::string_view className,
    const std::vector<ast::AstNode*>& parameters)
{
    ast::AstNode* constructor = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConstructorDeclaration,
        SourceRange(name.range().begin(), tokens_[index_ - 1].range().end()));
    constructor->setText(std::string(className));
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
        body->addChild(makeExpressionStatement(makeAssignmentExpression(field, value)));
    }
    constructor->setFirst(body);
    declaration->addChild(constructor);
}

void Parser::parseClassPrimaryConstructor(
    ast::AstNode* declaration,
    const lexer::Token& name,
    std::string_view className)
{
    if (!at(TokenKind::LeftParen)) return;
    consume();
    std::vector<ast::AstNode*> parameters;
    std::unordered_set<std::string> names;
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parsePromotedClassParameter(declaration, names))
                parameters.push_back(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    addClassPrimaryConstructor(declaration, name, className, parameters);
    std::string markers = declaration->auxiliaryText();
    markers += (markers.empty() ? "" : ",");
    markers += "primary-fields=" + std::to_string(parameters.size());
    declaration->setAuxiliaryText(std::move(markers));
}

void Parser::parseClassBaseTypes(ast::AstNode* declaration, bool isStatic)
{
    if (!consumeIf(TokenKind::Colon)) return;
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
            declaration->setTypeSyntax(specification);
            first = false;
        }
        declaration->addTypeSyntax(specification);
    }
    while (consumeIf(TokenKind::Comma));
}

void Parser::parseClassBody(
    ast::AstNode* declaration,
    std::string_view className,
    bool isStatic)
{
    // EN: Freeze 3.0 P1: member `where` clauses may name this class's own generic parameters (not
    //     those of an enclosing type).
    // ES: Congelamiento 3.0 P1: las clausulas `where` de miembros pueden nombrar los parametros
    //     propios de esta clase (no los de un tipo que la contiene).
    std::vector<std::string> outerParameters = std::move(enclosingGenericParameters_);
    enclosingGenericParameters_ = declaration->genericParameters();
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
        else member = parseClassMember(className);
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
            declaration->addChild(member);
        }
        if (index_ == before) consume();
    }
    enclosingGenericParameters_ = std::move(outerParameters);
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
        ast::AstNodeKind::ClassDeclaration, name.range());
    node->setText(className);
    node->setFlag(isPartial);
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);
    applyClassModifiers(node, isAbstract, isSealed, isStatic);
    parseClassPrimaryConstructor(node, name, className);
    parseClassBaseTypes(node, isStatic);
    parseGenericConstraintClauses(node);
    if (!expect(TokenKind::LeftBrace, "'{'") )
    {
        synchronizeGlobal();
        node->setRange(tokenRangeFrom(beginIndex));
        return node;
    }
    parseClassBody(node, className, isStatic);
    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

void Parser::appendStructMarker(ast::AstNode* declaration, std::string_view marker)
{
    std::string markers = declaration->auxiliaryText();
    markers += (markers.empty() ? "" : ",");
    markers += marker;
    declaration->setAuxiliaryText(std::move(markers));
}

void Parser::applyStructMarkers(
    ast::AstNode* declaration,
    StructLayoutForm form,
    bool isBorrowedView,
    bool isRecordStruct,
    bool isCopyType)
{
    if (form == StructLayoutForm::Union) appendStructMarker(declaration, "union");
    if (isBorrowedView) appendStructMarker(declaration, "borrowedview");
    if (isRecordStruct) appendStructMarker(declaration, "record");
    if (isCopyType) appendStructMarker(declaration, "copy");
}

ast::AstNode* Parser::parsePromotedStructParameter(
    ast::AstNode* declaration,
    std::unordered_set<std::string>& names,
    bool isRecordStruct)
{
    TokenKind accessibility = TokenKind::Invalid;
    bool readonlyField = false;
    while (isAccessibilityModifier(current().kind()) || at(TokenKind::KwReadonly))
    {
        const lexer::Token& modifier = consume();
        if (modifier.kind() == TokenKind::KwReadonly)
        {
            if (readonlyField)
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range(), "duplicate readonly modifier on promoted field");
            readonlyField = true;
        }
        else if (accessibility != TokenKind::Invalid)
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                modifier.range(), "conflicting accessibility modifiers on promoted field");
        else accessibility = modifier.kind();
    }
    ast::AstNode* parameter = parseParameter();
    if (parameter == nullptr) return nullptr;
    if (!names.insert(parameter->text()).second)
        diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            parameter->range(), "duplicate promoted field '" + parameter->text() + "'");
    if (!parameter->auxiliaryText().empty())
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parameter->range(), "a promoted parameter cannot be params, ref, out, or in");
    if (parameter->typeSyntax() != nullptr &&
        parameter->typeSyntax()->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parameter->range(),
            "owned and borrowed promoted parameters require the T13 ownership "
            "and partial-drop contract; declare the field and constructor explicitly");
    }
    const TokenKind fieldAccessibility = accessibility == TokenKind::Invalid
        ? (isRecordStruct ? TokenKind::KwPublic : TokenKind::KwPrivate) : accessibility;
    if (fieldAccessibility == TokenKind::KwProtected)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parameter->range(),
            "a promoted struct field cannot be protected because structs are not inheritable");
    ast::AstNode* field = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FieldDeclaration, parameter->range());
    field->setText(parameter->text());
    field->setTypeSyntax(parameter->typeSyntax());
    field->setAuxiliaryText("promoted-primary-field");
    field->setTokenKind(fieldAccessibility);
    field->addModifier(fieldAccessibility);
    if (readonlyField) field->addModifier(TokenKind::KwReadonly);
    declaration->addChild(field);
    return parameter;
}

void Parser::parseStructPrimaryConstructor(
    ast::AstNode* declaration,
    const lexer::Token& name,
    std::string_view structName,
    bool isRecordStruct)
{
    if (!at(TokenKind::LeftParen)) return;
    consume();
    std::vector<ast::AstNode*> parameters;
    std::unordered_set<std::string> names;
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter =
                    parsePromotedStructParameter(declaration, names, isRecordStruct))
                parameters.push_back(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    addClassPrimaryConstructor(declaration, name, structName, parameters);
    appendStructMarker(
        declaration, "primary-fields=" + std::to_string(parameters.size()));
}

void Parser::parseStructBaseTypes(ast::AstNode* declaration)
{
    if (!consumeIf(TokenKind::Colon)) return;
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
            declaration->setTypeSyntax(specification);
            first = false;
        }
        declaration->addTypeSyntax(specification);
    }
    while (consumeIf(TokenKind::Comma));
}

void Parser::parseRecordDerives(
    ast::AstNode* declaration,
    bool isRecordStruct,
    bool& sawRecordDeriveClause)
{
    if (!atContextualKeyword("derive") || peek().kind() != TokenKind::LeftParen) return;
    const lexer::Token& deriveKeyword = consume();
    const bool duplicateClause = sawRecordDeriveClause;
    if (duplicateClause)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            deriveKeyword.range(), "duplicate derive(...) clause on record struct");
    sawRecordDeriveClause = true;
    if (!isRecordStruct)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            deriveKeyword.range(), "derive(...) is valid only on a record struct");
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
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    derive.range(), "unknown record derive '" + deriveName +
                        "'; expected Equality, Hash, Print, or Clone");
            else if (!derives.insert(deriveName).second)
                diagnostics_.report(
                    diagnostics::DiagnosticId::ExpectedDeclaration,
                    diagnostics::DiagnosticSeverity::Error,
                    derive.range(), "duplicate record derive '" + deriveName + "'");
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    if (!isRecordStruct || duplicateClause || derives.empty()) return;
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
    appendStructMarker(declaration, marker);
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
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StructDeclaration, name.range());
    node->setText(structName);
    applyStructMarkers(node, form, isBorrowedView, isRecordStruct, isCopyType);
    parseGenericParameterList(node);
    KnownGenericTypeScope genericTypeScope(knownTypeNames_, node);
    if (form == StructLayoutForm::Struct)
        parseStructPrimaryConstructor(node, name, structName, isRecordStruct);
    parseStructBaseTypes(node);
    bool sawRecordDeriveClause = false;
    parseRecordDerives(node, isRecordStruct, sawRecordDeriveClause);
    parseGenericConstraintClauses(node);
    parseRecordDerives(node, isRecordStruct, sawRecordDeriveClause);
    return parseStructBody(node, structName, beginIndex);
}

// Parses the shared struct/union body without changing value-type member restrictions.
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

    // EN: Freeze 3.0 P1: member `where` clauses may name this struct's own generic parameters (not
    //     those of an enclosing type).
    // ES: Congelamiento 3.0 P1: las clausulas `where` de miembros pueden nombrar los parametros
    //     propios de este struct (no los de un tipo que lo contiene).
    std::vector<std::string> outerParameters = std::move(enclosingGenericParameters_);
    enclosingGenericParameters_ = node->genericParameters();
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
    enclosingGenericParameters_ = std::move(outerParameters);

    expect(TokenKind::RightBrace, "'}'");
    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

// Parses an enum declaration and records its optional underlying type and ordered members.
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

// Parses an extern C block, extern C function, or vectorcall delegate without changing ABI marks.
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

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExternCDeclaration,
        tokenRangeFrom(beginIndex));

    if (consumeIf(TokenKind::LeftBrace)) parseExternCBlock(node);
    else if (!parseExternCSingle(node)) return node;

    node->setRange(tokenRangeFrom(beginIndex));
    return node;
}

void Parser::parseExternCBlock(ast::AstNode* declaration)
{
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const TokenKind accessibility =
            (at(TokenKind::KwPublic) || at(TokenKind::KwInternal))
                ? consume().kind() : TokenKind::Invalid;
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
        declaration->addChild(function);
    }
    expect(TokenKind::RightBrace, "'}'");
}

bool Parser::parseExternCSingle(ast::AstNode* declaration)
{
    ast::TypeSyntax* returnType = parseType();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "external function name");
        synchronizeGlobal();
        return false;
    }
    const lexer::Token& name = consume();
    declaration->addChild(parseFunctionTail(returnType, name, true));
    return true;
}

// Parses a namespace function or global variable after its declaration modifiers.
ast::AstNode* Parser::parseFunctionOrGlobalVariable(bool deferBlockBody)
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
    if (at(TokenKind::KwOperator) && parsingExtensionBody_)
        return parseExtensionOperatorFunction(type, beginIndex, isReferenceReturn);
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "declaration name");
        synchronizeGlobal();
        return nullptr;
    }

    const lexer::Token& name = consume();
    return finishFunctionOrGlobalVariable(
        beginIndex, type, name, isConst, isReferenceReturn,
        std::move(returnLifetime), deferBlockBody);
}

// Preserves the extension-operator marker, static contract, and reference return flag.
ast::AstNode* Parser::parseExtensionOperatorFunction(
    ast::TypeSyntax* type,
    std::size_t beginIndex,
    bool isReferenceReturn)
{
    ast::AstNode* declaration =
        parseOperatorTail(type, TokenKind::KwPublic, true, beginIndex);
    if (declaration == nullptr) return nullptr;
    declaration->addModifier(TokenKind::KwStatic);
    declaration->setAuxiliaryText(
        declaration->auxiliaryText() + "|ns3-extension-static");
    declaration->setReference(isReferenceReturn);
    return declaration;
}

// Dispatches a named namespace declaration after its shared prefix has been consumed.
ast::AstNode* Parser::finishFunctionOrGlobalVariable(
    std::size_t beginIndex,
    ast::TypeSyntax* type,
    const lexer::Token& name,
    bool isConst,
    bool isReferenceReturn,
    std::string returnLifetime,
    bool deferBlockBody)
{
    if (!isConst && (at(TokenKind::LeftParen) || at(TokenKind::Less)))
    {
        ast::AstNode* function = parseFunctionTail(type, name, false, deferBlockBody);
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
            beginIndex, false, false, isReferenceReturn);
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
}
