#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
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

std::string sanitizeSymbol(std::string value)
{
    for (char& character : value)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) == 0 && character != '_') character = '_';
    }
    return value;
}

std::string namespaceFunctionMangledName(const NamespaceFunctionSymbol& symbol)
{
    std::string result = "__nsc_function_" +
        compilation::symbolFragmentForIdentityKey(
            symbol.moduleName + "::" + symbol.namespaceName) +
        "__" + sanitizeSymbol(symbol.name);
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        if (parameter.ownership == sema::OwnershipKind::Owned) result += "__owned";
        else if (parameter.ownership == sema::OwnershipKind::Borrowed) result += "__borrowed";
        if (parameter.passKind != ParameterPassKind::Value)
            result += "__" + std::string(parameterPassKindKeyword(parameter.passKind));
        result += "__" + compilation::symbolFragmentForIdentityKey(parameter.typeKey);
    }
    if (symbol.isVariadic) result += "__variadic";
    if (symbol.isExternC) result += "__extern_c";
    return result;
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

struct FunctionLookupEnvironment final
{
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
};

std::vector<std::string> splitName(std::string_view name)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin < name.size())
    {
        const std::size_t dot = name.find('.', begin);
        const std::size_t end = dot == std::string_view::npos ? name.size() : dot;
        if (end != begin) segments.emplace_back(name.substr(begin, end - begin));
        if (dot == std::string_view::npos) break;
        begin = dot + 1;
    }
    return segments;
}

std::string joinName(std::span<const std::string> segments)
{
    std::string result;
    for (const std::string& segment : segments)
    {
        if (!result.empty()) result.push_back('.');
        result += segment;
    }
    return result;
}

std::optional<std::string> expressionName(const ast::AstNode* expression)
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
        return expression->text().empty() ? std::nullopt
                                        : std::optional<std::string>(expression->text());
    if (expression->kind() != ast::AstNodeKind::MemberExpression ||
        expression->first() == nullptr || expression->text().empty()) return std::nullopt;
    std::optional<std::string> prefix = expressionName(expression->first());
    if (!prefix.has_value()) return std::nullopt;
    prefix->push_back('.');
    *prefix += expression->text();
    return prefix;
}
}

bool ProjectNamespaceFunctionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    functions_.clear();
    functionsByName_.clear();
    referenceBindings_.clear();
    referenceBindingBySyntax_.clear();
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
        candidate.callableIdentityKey = "function:" + candidate.moduleName + "::" +
            candidate.namespaceName + "." + candidate.signatureKey +
            (candidate.isExternC ? "|extern-c" : "|native") +
            (candidate.isVariadic ? "|variadic" : "");
        candidate.mangledName = namespaceFunctionMangledName(candidate);

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

    auto visibleProjects = [&](std::size_t projectOrdinal) -> std::span<const std::size_t>
    {
        if (projectOrdinal >= nameLookup.modules().size()) return {};
        return nameLookup.modules()[projectOrdinal]->visibleProjectOrdinals();
    };
    auto namespaceExists = [&](std::size_t projectOrdinal, std::string_view namespaceName)
    {
        const std::vector<std::string> segments = splitName(namespaceName);
        if (projectOrdinal < nameLookup.modules().size() &&
            nameLookup.modules()[projectOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
        {
            return true;
        }
        for (const std::size_t visibleProject : visibleProjects(projectOrdinal))
        {
            if (visibleProject < nameLookup.modules().size() &&
                nameLookup.modules()[visibleProject]->globalNamespace().findNamespace(segments) != nullptr)
            {
                return true;
            }
        }
        return false;
    };
    auto resolveNamespace = [&](std::size_t projectOrdinal,
                                std::string_view lexicalNamespace,
                                const FunctionLookupEnvironment& environment,
                                std::string written) -> std::optional<std::string>
    {
        const std::size_t firstDot = written.find('.');
        const std::string first = written.substr(0, firstDot);
        const auto alias = environment.aliases.find(first);
        if (alias != environment.aliases.end())
        {
            written = alias->second +
                (firstDot == std::string::npos ? std::string{} : written.substr(firstDot));
            return namespaceExists(projectOrdinal, written)
                ? std::optional<std::string>(std::move(written)) : std::nullopt;
        }

        std::vector<std::string> lexicalSegments = splitName(lexicalNamespace);
        while (true)
        {
            const std::string prefix = joinName(lexicalSegments);
            const std::string candidate = prefix.empty() ? written : prefix + "." + written;
            if (namespaceExists(projectOrdinal, candidate)) return candidate;
            if (lexicalSegments.empty()) break;
            lexicalSegments.pop_back();
        }
        return std::nullopt;
    };
    auto candidatesInNamespace = [&](std::size_t projectOrdinal,
                                     std::string_view namespaceName,
                                     std::string_view functionName)
    {
        std::vector<std::size_t> candidates;
        const auto appendProject = [&](std::size_t visibleProject)
        {
            for (const std::size_t ordinal :
                 findFunctions(visibleProject, namespaceName, functionName))
            {
                if (ordinal >= functions_.size()) continue;
                if (visibleProject == projectOrdinal ||
                    functions_[ordinal].accessibility == NamespaceFunctionAccessibility::Public)
                {
                    candidates.push_back(ordinal);
                }
            }
        };
        appendProject(projectOrdinal);
        for (const std::size_t visibleProject : visibleProjects(projectOrdinal))
            if (visibleProject != projectOrdinal) appendProject(visibleProject);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        return candidates;
    };

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPointer : units)
    {
        if (unitPointer == nullptr || unitPointer->syntaxTree() == nullptr) continue;
        const ProjectSyntaxUnit& unit = *unitPointer;
        std::unordered_set<const ast::AstNode*> visited;
        std::function<void(const ast::AstNode*, const std::string&, const FunctionLookupEnvironment&)>
            walkNode;
        std::function<void(const ast::AstNode*, const std::string&, const FunctionLookupEnvironment&)>
            walkScope;

        walkScope = [&](const ast::AstNode* scopeNode,
                        const std::string& lexicalNamespace,
                        const FunctionLookupEnvironment& inherited)
        {
            if (scopeNode == nullptr) return;
            FunctionLookupEnvironment environment = inherited;
            for (const ast::AstNode* child : scopeNode->children())
            {
                if (child == nullptr) continue;
                if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
                {
                    const std::optional<std::string> resolved = resolveNamespace(
                        unit.projectOrdinal(), lexicalNamespace, environment, child->text());
                    if (resolved.has_value() &&
                        std::find(environment.importedNamespaces.begin(),
                                  environment.importedNamespaces.end(), *resolved) ==
                            environment.importedNamespaces.end())
                    {
                        environment.importedNamespaces.push_back(*resolved);
                    }
                }
                else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                         !child->auxiliaryText().empty())
                {
                    const std::optional<std::string> resolved = resolveNamespace(
                        unit.projectOrdinal(), lexicalNamespace, environment,
                        child->auxiliaryText());
                    if (resolved.has_value()) environment.aliases[child->text()] = *resolved;
                }
            }

            for (const ast::AstNode* child : scopeNode->children())
            {
                if (child == nullptr ||
                    child->kind() == ast::AstNodeKind::UsingNamespaceDirective ||
                    child->kind() == ast::AstNodeKind::UsingAliasDirective)
                {
                    continue;
                }
                if (child->kind() == ast::AstNodeKind::NamespaceDeclaration)
                {
                    const std::vector<std::string> segments = splitName(child->text());
                    std::string nestedNamespace = lexicalNamespace;
                    for (const std::string& segment : segments)
                    {
                        if (!nestedNamespace.empty()) nestedNamespace.push_back('.');
                        nestedNamespace += segment;
                    }
                    walkScope(child, nestedNamespace, environment);
                }
                else
                {
                    walkNode(child, lexicalNamespace, environment);
                }
            }
        };

        walkNode = [&](const ast::AstNode* node,
                       const std::string& lexicalNamespace,
                       const FunctionLookupEnvironment& environment)
        {
            if (node == nullptr || !visited.insert(node).second) return;
            if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
            {
                std::string nestedNamespace = lexicalNamespace;
                for (const std::string& segment : splitName(node->text()))
                {
                    if (!nestedNamespace.empty()) nestedNamespace.push_back('.');
                    nestedNamespace += segment;
                }
                walkScope(node, nestedNamespace, environment);
                return;
            }

            if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr)
            {
                const std::optional<std::string> written = expressionName(node->first());
                if (written.has_value())
                {
                    const std::size_t lastDot = written->find_last_of('.');
                    const bool qualified = lastDot != std::string::npos;
                    const std::string functionName = qualified
                        ? written->substr(lastDot + 1) : *written;
                    std::vector<std::size_t> candidates;
                    if (qualified)
                    {
                        const std::optional<std::string> resolvedNamespace = resolveNamespace(
                            unit.projectOrdinal(), lexicalNamespace, environment,
                            written->substr(0, lastDot));
                        if (resolvedNamespace.has_value())
                        {
                            candidates = candidatesInNamespace(
                                unit.projectOrdinal(), *resolvedNamespace, functionName);
                        }
                    }
                    else
                    {
                        candidates = candidatesInNamespace(
                            unit.projectOrdinal(), lexicalNamespace, functionName);
                        if (candidates.empty())
                        {
                            for (const std::string& imported : environment.importedNamespaces)
                            {
                                std::vector<std::size_t> importedCandidates = candidatesInNamespace(
                                    unit.projectOrdinal(), imported, functionName);
                                candidates.insert(candidates.end(),
                                                  importedCandidates.begin(), importedCandidates.end());
                            }
                            std::sort(candidates.begin(), candidates.end());
                            candidates.erase(
                                std::unique(candidates.begin(), candidates.end()), candidates.end());
                        }
                    }

                    if (!candidates.empty())
                    {
                        NamespaceFunctionReferenceBinding binding;
                        binding.callSyntax = node;
                        binding.calleeSyntax = node->first();
                        binding.projectOrdinal = unit.projectOrdinal();
                        binding.fileId = unit.fileId();
                        binding.lexicalNamespace = lexicalNamespace;
                        binding.writtenName = *written;
                        binding.qualified = qualified;
                        binding.candidateFunctionOrdinals = std::move(candidates);
                        referenceBindingBySyntax_.emplace(node, referenceBindings_.size());
                        referenceBindings_.push_back(std::move(binding));
                    }
                }
            }

            walkNode(node->first(), lexicalNamespace, environment);
            walkNode(node->second(), lexicalNamespace, environment);
            walkNode(node->third(), lexicalNamespace, environment);
            for (const ast::AstNode* child : node->children())
                walkNode(child, lexicalNamespace, environment);
        };

        walkScope(unit.syntaxTree(), {}, FunctionLookupEnvironment{});
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

const NamespaceFunctionReferenceBinding* ProjectNamespaceFunctionModel::referenceBinding(
    const ast::AstNode* callSyntax) const noexcept
{
    const auto found = referenceBindingBySyntax_.find(callSyntax);
    if (found == referenceBindingBySyntax_.end()) return nullptr;
    return &referenceBindings_[found->second];
}

std::span<const NamespaceFunctionReferenceBinding>
ProjectNamespaceFunctionModel::referenceBindings() const noexcept
{
    return referenceBindings_;
}

bool ProjectNamespaceFunctionModel::isBuilt() const noexcept
{
    return built_;
}
}