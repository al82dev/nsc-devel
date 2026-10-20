#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
std::string ProjectGenericModel::functionTypeSpelling(const ast::TypeSyntax* syntax) const
{
    language::FunctionType function;
    function.carrier = syntax->functionCarrier() ==
            ast::FunctionTypeCarrierSyntaxKind::Pointer
        ? language::FunctionCarrier::Pointer
        : language::FunctionCarrier::Reference;
    switch (syntax->functionCallingConvention())
    {
        case ast::FunctionCallingConventionSyntaxKind::NativeSharp:
            function.callingConvention = language::CallingConvention::NativeSharp; break;
        case ast::FunctionCallingConventionSyntaxKind::C:
            function.callingConvention = language::CallingConvention::C; break;
        case ast::FunctionCallingConventionSyntaxKind::StdCall:
            function.callingConvention = language::CallingConvention::StdCall; break;
        case ast::FunctionCallingConventionSyntaxKind::FastCall:
            function.callingConvention = language::CallingConvention::FastCall; break;
        case ast::FunctionCallingConventionSyntaxKind::VectorCall:
            function.callingConvention = language::CallingConvention::VectorCall; break;
        case ast::FunctionCallingConventionSyntaxKind::Win64:
            function.callingConvention = language::CallingConvention::Win64; break;
    }
    for (const ast::TypeSyntax* parameter : syntax->typeArguments())
        function.parameters.push_back(typeSpelling(parameter));
    function.result = typeSpelling(syntax->functionReturnType());
    function.errorType = syntax->functionErrorType() == nullptr
        ? std::string{} : typeSpelling(syntax->functionErrorType());
    function.lifetime = syntax->functionLifetimeName();
    function.variadic = syntax->functionIsVariadic();
    function.isUnsafe = syntax->functionIsUnsafe();
    function.isNoHeap = syntax->functionIsNoHeap();
    function.isAsync = syntax->functionIsAsync();
    function.nullable = syntax->functionIsNullable();
    return language::functionTypeIdentity(function);
}

std::string ProjectGenericModel::typeSpelling(const ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr) return "<missing>";
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue))
        return "const(" + constType + "=" + constValue + ")";
    std::string result;
    if (syntax->isFunction())
    {
        result = functionTypeSpelling(syntax);
    }
    else if (syntax->isTuple())
    {
        result.push_back('(');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(syntax->typeArguments()[index]);
        }
        result.push_back(')');
    }
    else result = syntax->baseName();
    if (!syntax->isFunction() && !syntax->isTuple() &&
        !syntax->typeArguments().empty())
    {
        result.push_back('<');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(syntax->typeArguments()[index]);
        }
        result.push_back('>');
    }
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) result.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) result += "[]";
        else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
            result += '[' + std::string(
                modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) result += "[*]";
        else result += "[fixed]";
    }
    if (syntax->isPackExpansion()) result += "...";
    return result;
}

std::string ProjectGenericModel::typeSpecializationIdentity(const ast::TypeSyntax* syntax) const
{
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue))
        return "const:" + constType + "=" + constValue;
    if (syntax != nullptr)
    {
        if (const auto resolved = resolvedTypeIdentityBySyntax_.find(syntax);
            resolved != resolvedTypeIdentityBySyntax_.end())
        {
            return resolved->second;
        }
    }

    const std::string spelling = typeSpelling(syntax);
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        return spelling;
    }

    // The spelling remains the public/readable type argument. The cache identity additionally
    // records where an unresolved source name came from, so AppA.Payload and AppB.Payload cannot
    // reuse one clone merely because both were written as `Payload`. A substituted argument
    // carries its original site explicitly; a direct call uses the current site. The persistent
    // identity uses the module name, never the graph-order-dependent ordinal (the ordinal remains
    // lookup-only). Length-prefix module and namespace so punctuation cannot make identities
    // concatenate alike.
    const std::size_t projectOrdinal =
        syntax->originProjectOrdinal().value_or(currentInstantiationProjectOrdinal_);
    const std::string_view moduleName = projectOrdinal < projectModuleNames_.size()
        ? std::string_view(projectModuleNames_[projectOrdinal])
        : std::string_view("<invalid-project>");
    const std::string namespaceName = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : currentInstantiationNamespace_);

    return spelling + "@origin-module[" + std::to_string(moduleName.size()) + "]=" +
        std::string(moduleName) +
        ";namespace[" + std::to_string(namespaceName.size()) + "]=" + namespaceName;
}

const ProjectGenericModel::DeclaredTypeIdentity*
ProjectGenericModel::resolveModuleQualifiedDeclaredTypeIdentity(
    std::string_view writtenName,
    std::size_t projectOrdinal) const
{
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator == std::string_view::npos) return nullptr;
    const std::string_view requestedModule(writtenName.data(), moduleSeparator);
    const std::string_view requestedType(
        writtenName.data() + moduleSeparator + 2U,
        writtenName.size() - moduleSeparator - 2U);
    if (requestedModule.empty() || requestedType.empty()) return nullptr;

    const DeclaredTypeIdentity* exact = nullptr;
    for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
    {
        if (declaration.sourceName != requestedType ||
            !declaration.identityKey.starts_with(std::string(requestedModule) + "::") ||
            (declaration.projectOrdinal != projectOrdinal && !declaration.isPublic))
            continue;
        if (exact != nullptr) return nullptr;
        exact = &declaration;
    }
    return exact;
}

const ProjectGenericModel::DeclaredTypeIdentity* ProjectGenericModel::resolveDeclaredTypeIdentity(
    const ast::TypeSyntax* syntax,
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    const GenericLookupScope& scope) const
{
    if (syntax == nullptr || syntax->isFunction() ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        !syntax->typeArguments().empty())
    {
        return nullptr;
    }
    // Module-qualified names are an exact identity request, not a namespace-relative spelling. The
    // ordinary ProjectNameLookup already honors this form, and generic specialization must do so
    // before it builds its cache key. Otherwise a reconstructed type such as
    // Provider::Contracts.Value falls back to a source-origin spelling and produces a different
    // closed generic name from the producer that published it.
    const std::string& writtenName = syntax->baseName();
    if (const std::size_t moduleSeparator = writtenName.find("::");
        moduleSeparator != std::string::npos)
    {
        return resolveModuleQualifiedDeclaredTypeIdentity(writtenName, projectOrdinal);
    }

    projectOrdinal = syntax->originProjectOrdinal().value_or(projectOrdinal);
    const std::string effectiveNamespace = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : std::string(namespaceName));
    const std::vector<std::string> namespaceSegments = splitDotted(effectiveNamespace);
    const std::vector<std::string> noContainingTypes;

    // A substituted node carries the original project/namespace, but not its using directives.
    // Its declaration identity was normally copied before substitution. If an older/unannotated
    // node reaches this fallback, do not accidentally apply the template's imports to the
    // consumer's source name.
    const std::vector<std::string> noImports;
    const std::unordered_map<std::string, std::string> noAliases;
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = noContainingTypes;
    site.aliases = syntax->originProjectOrdinal().has_value() ? &noAliases : &scope.aliases.read();
    site.importedNamespaces = syntax->originProjectOrdinal().has_value()
        ? std::span<const std::string>(noImports)
        : std::span<const std::string>(scope.importedNamespaces.read());

    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, syntax->baseName()))
    {
        std::vector<const DeclaredTypeIdentity*> identities;
        for (const std::string& spelling : rung.spellings)
        {
            for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
            {
                if (declaration.sourceName != spelling) continue;

                const bool sameProject = declaration.projectOrdinal == projectOrdinal;
                bool visible = false;
                switch (rung.tier)
                {
                    case QualifiedNameTier::EnclosingType:
                    case QualifiedNameTier::LexicalNamespace:
                    case QualifiedNameTier::Global:
                        visible = sameProject;
                        break;
                    case QualifiedNameTier::ImportedNamespace:
                        visible = sameProject ||
                            (declaration.isPublic &&
                             projectVisible(projectOrdinal, declaration.projectOrdinal));
                        break;
                    case QualifiedNameTier::ReferencedModule:
                        visible = !sameProject && declaration.isPublic &&
                            projectVisible(projectOrdinal, declaration.projectOrdinal);
                        break;
                }
                if (!visible || std::find_if(
                        identities.begin(), identities.end(), [&](const DeclaredTypeIdentity* identity) {
                            return identity != nullptr && identity->identityKey == declaration.identityKey;
                        }) != identities.end())
                {
                    continue;
                }
                identities.push_back(&declaration);
            }
        }
        if (identities.size() == 1) return identities.front();
        if (identities.size() > 1) return nullptr;
    }
    return nullptr;
}

void ProjectGenericModel::appendTypeModifiers(
    std::string& identity,
    const ast::TypeSyntax* syntax)
{
    if (syntax == nullptr) return;
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) identity.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) identity += "[]";
        else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
            identity += '[' + std::string(
                modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) identity += "[*]";
        else identity += "[fixed]";
    }
}

std::string ProjectGenericModel::namespaceAppend(std::string_view prefix, std::string_view suffix)
{
    if (prefix.empty()) return std::string(suffix);
    if (suffix.empty()) return std::string(prefix);
    return std::string(prefix) + "." + std::string(suffix);
}

std::string ProjectGenericModel::sanitize(std::string_view text)
{
    std::string result;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) result.push_back(static_cast<char>(ch));
        else result.push_back('_');
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) result = "type";
    return result;
}

std::string ProjectGenericModel::methodCalleeName(const ast::AstNode* call)
{
    if (call == nullptr || call->first() == nullptr) return {};
    const ast::AstNode* callee = call->first();
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
        callee->kind() == ast::AstNodeKind::MemberExpression)
        return callee->text();
    return {};
}

ast::AstNode* ProjectGenericModel::methodCalleeSyntax(ast::AstNode* call)
{
    if (call == nullptr) return nullptr;
    return call->first();
}
void ProjectGenericModel::buildVisibility(const compilation::ProjectGraph& graph)
{
    const auto projects = graph.projects();
    projectModuleNames_.clear();
    projectModuleNames_.reserve(projects.size());
    for (const compilation::Project& project : projects)
    {
        projectModuleNames_.push_back(project.moduleName());
    }
    visibleProjects_.assign(projects.size(), {});
    std::unordered_map<std::string, std::size_t> projectByPath;
    const auto pathKey = [](const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
        if (errorCode) canonical = std::filesystem::absolute(path).lexically_normal();
        std::string key = canonical.generic_string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return key;
    };
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
        projectByPath.emplace(pathKey(projects[ordinal].projectFile()), ordinal);

    for (std::size_t start = 0; start < projects.size(); ++start)
    {
        std::vector<bool> reachable(projects.size(), false);
        std::function<void(std::size_t)> visit = [&](std::size_t ordinal)
        {
            for (const std::filesystem::path& reference : projects[ordinal].projectReferences())
            {
                const auto found = projectByPath.find(pathKey(reference));
                if (found == projectByPath.end() || reachable[found->second]) continue;
                reachable[found->second] = true;
                visit(found->second);
            }
        };
        visit(start);
        for (std::size_t ordinal = 0; ordinal < reachable.size(); ++ordinal)
            if (reachable[ordinal]) visibleProjects_[start].push_back(ordinal);
    }
}

bool ProjectGenericModel::projectVisible(std::size_t from, std::size_t target) const noexcept
{
    if (from == target) return true;
    if (from >= visibleProjects_.size()) return false;
    return std::find(visibleProjects_[from].begin(), visibleProjects_[from].end(), target) !=
        visibleProjects_[from].end();
}

ProjectGenericModel::TemplateState* ProjectGenericModel::stateForSyntax(
    const ast::AstNode* syntax) noexcept
{
    const auto found = stateBySyntax_.find(syntax);
    return found == stateBySyntax_.end() ? nullptr : found->second;
}
}
