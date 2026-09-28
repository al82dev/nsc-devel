#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
sema::OwnershipKind ownershipKindFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr) return sema::OwnershipKind::None;
    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None: return sema::OwnershipKind::None;
        case ast::OwnershipQualifierSyntaxKind::Owned: return sema::OwnershipKind::Owned;
        case ast::OwnershipQualifierSyntaxKind::Borrowed: return sema::OwnershipKind::Borrowed;
    }
    return sema::OwnershipKind::None;
}

std::string ownershipSignaturePrefix(sema::OwnershipKind ownership)
{
    switch (ownership)
    {
        case sema::OwnershipKind::None: return {};
        case sema::OwnershipKind::Owned: return "owned ";
        case sema::OwnershipKind::Borrowed: return "borrowed ";
    }
    return {};
}

ParameterPassKind parameterPassKind(const ast::AstNode& parameter) noexcept
{
    if (!parameter.auxiliaryText().starts_with("pass:")) return ParameterPassKind::Value;
    const std::string_view spelling(parameter.auxiliaryText().data() + 5,
                                    parameter.auxiliaryText().size() - 5);
    if (spelling == "ref") return ParameterPassKind::Ref;
    if (spelling == "out") return ParameterPassKind::Out;
    return ParameterPassKind::In;
}

std::string nameLookupKey(
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    std::string_view name)
{
    return std::to_string(projectOrdinal) + "\x1f" + std::string(namespaceName) +
        "\x1f" + std::string(name);
}

std::string mergeKey(const NamespaceFunctionSymbol& symbol)
{
    return symbol.moduleName + "\x1f" + symbol.namespaceName + "\x1f" + symbol.signatureKey;
}

void reportPrevious(
    const NamespaceFunctionSymbol& previous,
    diagnostics::DiagnosticEngine& diagnostics)
{
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        previous.range,
        "previous declaration of namespace function '" + previous.signatureKey + "' is here");
}

bool compatibleDeclarations(
    const NamespaceFunctionSymbol& symbol,
    const NamespaceFunctionDeclaration& declaration,
    std::string_view returnTypeKey,
    sema::OwnershipKind returnOwnership) noexcept
{
    const ast::AstNode* syntax = declaration.syntax;
    return symbol.accessibility == declaration.accessibility &&
        symbol.declaredTypeKey == returnTypeKey &&
        symbol.declaredOwnership == returnOwnership &&
        symbol.isReferenceReturn == syntax->isReference() &&
        symbol.isVariadic == syntax->isVariadic() &&
        symbol.isExternC == syntax->flag();
}
}

bool ProjectNamespaceFunctionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    functions_.clear();
    functionsByName_.clear();
    built_ = false;
    const std::size_t initialErrorCount = diagnostics.errorCount();
    std::unordered_map<std::string, std::size_t> symbolByIdentity;

    for (const NamespaceFunctionDeclaration& declaration : nameLookup.namespaceFunctions())
    {
        if (declaration.syntax == nullptr) continue;
        const ast::AstNode& syntax = *declaration.syntax;

        NamespaceFunctionSymbol candidate;
        candidate.accessibility = declaration.accessibility;
        candidate.projectOrdinal = declaration.projectOrdinal;
        candidate.sourceOrdinal = declaration.sourceOrdinal;
        candidate.fileId = declaration.fileId;
        candidate.range = declaration.range;
        candidate.sourcePath = declaration.sourcePath;
        candidate.moduleName = declaration.moduleName;
        candidate.namespaceName = declaration.namespaceName;
        candidate.name = declaration.name;
        candidate.declaredTypeSyntax = syntax.typeSyntax();
        candidate.declaredTypeKey = memberModel.canonicalTypeKey(syntax.typeSyntax(), nameLookup);
        candidate.declaredOwnership = ownershipKindFromSyntax(syntax.typeSyntax());
        candidate.declarationSyntax = declaration.syntax;
        candidate.definitionSyntax = declaration.hasBody ? declaration.syntax : nullptr;
        candidate.isReferenceReturn = syntax.isReference();
        candidate.isVariadic = syntax.isVariadic();
        candidate.isExternC = syntax.flag();
        candidate.parts.push_back(NamespaceFunctionPart{
            declaration.accessibility,
            declaration.sourceOrdinal,
            declaration.fileId,
            declaration.range,
            declaration.sourcePath,
            declaration.syntax,
            declaration.hasBody});

        std::unordered_map<std::string, const ast::AstNode*> parameterNames;
        for (const ast::AstNode* parameterNode : syntax.children())
        {
            if (parameterNode == nullptr ||
                parameterNode->kind() != ast::AstNodeKind::ParameterDeclaration)
            {
                continue;
            }

            MethodParameterSymbol parameter;
            parameter.name = parameterNode->text();
            parameter.passKind = parameterPassKind(*parameterNode);
            parameter.typeSyntax = parameterNode->typeSyntax();
            parameter.typeKey = memberModel.canonicalTypeKey(parameter.typeSyntax, nameLookup);
            parameter.ownership = ownershipKindFromSyntax(parameter.typeSyntax);
            parameter.fileId = declaration.fileId;
            parameter.range = parameterNode->range();
            parameter.syntax = parameterNode;
            parameter.defaultValueSyntax = parameterNode->first();
            if (parameter.passKind != ParameterPassKind::Value)
            {
                parameter.pointeeTypeKey = parameter.typeKey.empty() || parameter.typeKey.back() != '*'
                    ? parameter.typeKey
                    : parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
            }

            const auto [previous, inserted] = parameterNames.emplace(parameter.name, parameterNode);
            if (!inserted)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter.range,
                    "duplicate parameter '" + parameter.name + "' in namespace function '" +
                        declaration.name + "'");
                diagnostics.report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    previous->second->range(),
                    "previous parameter declaration is here");
            }
            candidate.parameters.push_back(std::move(parameter));
        }

        candidate.signatureKey = candidate.name + '(';
        for (std::size_t index = 0; index < candidate.parameters.size(); ++index)
        {
            if (index != 0) candidate.signatureKey.push_back(',');
            const MethodParameterSymbol& parameter = candidate.parameters[index];
            candidate.signatureKey += ownershipSignaturePrefix(parameter.ownership);
            if (parameter.passKind != ParameterPassKind::Value)
            {
                candidate.signatureKey += parameterPassKindKeyword(parameter.passKind);
                candidate.signatureKey.push_back(' ');
            }
            candidate.signatureKey += parameter.typeKey;
        }
        candidate.signatureKey.push_back(')');

        const std::string identity = mergeKey(candidate);
        const auto existing = symbolByIdentity.find(identity);
        if (existing == symbolByIdentity.end())
        {
            const std::size_t ordinal = functions_.size();
            symbolByIdentity.emplace(identity, ordinal);
            functionsByName_[nameLookupKey(
                candidate.projectOrdinal, candidate.namespaceName, candidate.name)].push_back(ordinal);
            functions_.push_back(std::move(candidate));
            continue;
        }

        NamespaceFunctionSymbol& symbol = functions_[existing->second];
        const bool compatible = compatibleDeclarations(
            symbol,
            declaration,
            candidate.declaredTypeKey,
            candidate.declaredOwnership);
        if (!compatible)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ConflictingFunctionDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                declaration.range,
                "declaration of namespace function '" + candidate.signatureKey +
                    "' conflicts with its previous declaration");
            reportPrevious(symbol, diagnostics);
            continue;
        }
        if (declaration.hasBody && symbol.definitionSyntax != nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateFunctionDefinition,
                diagnostics::DiagnosticSeverity::Error,
                declaration.range,
                "namespace function '" + candidate.signatureKey + "' already has a definition");
            reportPrevious(symbol, diagnostics);
            continue;
        }

        symbol.parts.push_back(candidate.parts.front());
        if (declaration.hasBody) symbol.definitionSyntax = declaration.syntax;
    }

    for (const NamespaceFunctionSymbol& symbol : functions_)
    {
        if (symbol.definitionSyntax == nullptr && !symbol.isExternC)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::MissingFunctionDefinition,
                diagnostics::DiagnosticSeverity::Error,
                symbol.range,
                "namespace function '" + symbol.signatureKey + "' is declared but never defined");
        }
    }

    built_ = diagnostics.errorCount() == initialErrorCount;
    return built_;
}

std::span<const NamespaceFunctionSymbol> ProjectNamespaceFunctionModel::functions() const noexcept
{
    return functions_;
}

std::span<const std::size_t> ProjectNamespaceFunctionModel::findFunctions(
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    std::string_view name) const noexcept
{
    const auto found = functionsByName_.find(nameLookupKey(projectOrdinal, namespaceName, name));
    if (found == functionsByName_.end()) return {};
    return found->second;
}

bool ProjectNamespaceFunctionModel::isBuilt() const noexcept
{
    return built_;
}
}