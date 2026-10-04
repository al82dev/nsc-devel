#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectGenericModel;
struct NamespaceFunctionPart final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    std::string sourcePath;
    const ast::AstNode* syntax = nullptr;
    bool hasBody = false;
};

struct NamespaceFunctionSymbol final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    std::string sourcePath;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    const ast::TypeSyntax* declaredTypeSyntax = nullptr;
    std::string declaredTypeKey;
    const ast::TypeSyntax* declaredErrorTypeSyntax = nullptr;
    std::string declaredErrorTypeKey;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::vector<MethodParameterSymbol> parameters;
    std::string signatureKey;
    std::string callableIdentityKey;
    std::string mangledName;
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* definitionSyntax = nullptr;
    std::vector<NamespaceFunctionPart> parts;
    bool isReferenceReturn = false;
    std::string returnLifetimeName;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    std::vector<language::EscapeClass> parameterEscapeSummary;
    sema::EffectSet effects;
    bool isVariadic = false;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isExternC = false;
    bool isExportedC = false;
    bool isPrecompiled = false;
    bool isExtension = false;
};

struct PrecompiledNamespaceFunctionIdentity final
{
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    std::string signatureKey;
    std::vector<language::EscapeClass> parameterEscapeSummary;
};

enum class NamespaceStateInitializerKind
{
    Zero,
    Integer,
    Floating,
    Boolean,
    String,
    Null
};

struct NamespaceStateSymbol final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    std::string sourcePath;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    const ast::TypeSyntax* declaredTypeSyntax = nullptr;
    std::string declaredTypeKey;
    std::string identityKey;
    std::string storageName;
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* initializerSyntax = nullptr;
    NamespaceStateInitializerKind initializerKind = NamespaceStateInitializerKind::Zero;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
    std::string stringValue;
    bool isConst = false;
    bool isPrecompiled = false;
};

struct NamespaceFunctionReferenceBinding final
{
    const ast::AstNode* callSyntax = nullptr;
    const ast::AstNode* calleeSyntax = nullptr;
    std::size_t projectOrdinal = 0;
    FileId fileId;
    std::string lexicalNamespace;
    std::string writtenName;
    bool qualified = false;
    std::vector<std::size_t> candidateFunctionOrdinals;
    bool extension = false;
    const ast::AstNode* receiverSyntax = nullptr;
};

struct NamespaceStateReferenceBinding final
{
    const ast::AstNode* expressionSyntax = nullptr;
    std::size_t projectOrdinal = 0;
    FileId fileId;
    std::string lexicalNamespace;
    std::string writtenName;
    bool qualified = false;
    std::vector<std::size_t> candidateStateOrdinals;
};

class ProjectNamespaceFunctionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel = nullptr);
    bool bindPrecompiledEscapeSummaries(
        std::span<const PrecompiledNamespaceFunctionIdentity> identities,
        std::string& error);

    std::span<const NamespaceFunctionSymbol> functions() const noexcept;
    std::span<const NamespaceStateSymbol> states() const noexcept;
    const NamespaceStateSymbol* findState(
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view name) const noexcept;
    std::span<const std::size_t> findFunctions(
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view name) const noexcept;
    const NamespaceFunctionReferenceBinding* referenceBinding(
        const ast::AstNode* callSyntax) const noexcept;
    const NamespaceFunctionReferenceBinding* extensionBinding(
        const ast::AstNode* memberSyntax) const noexcept;
    std::span<const NamespaceFunctionReferenceBinding> referenceBindings() const noexcept;
    const NamespaceStateReferenceBinding* stateReferenceBinding(
        const ast::AstNode* expressionSyntax) const noexcept;
    std::span<const NamespaceStateReferenceBinding> stateReferenceBindings() const noexcept;
    bool isBuilt() const noexcept;

private:
    std::vector<NamespaceFunctionSymbol> functions_;
    std::vector<NamespaceStateSymbol> states_;
    std::unordered_map<std::string, std::size_t> stateByName_;
    std::unordered_map<std::string, std::vector<std::size_t>> functionsByName_;
    std::vector<NamespaceFunctionReferenceBinding> referenceBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> referenceBindingBySyntax_;
    std::vector<NamespaceFunctionReferenceBinding> extensionBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> extensionBindingBySyntax_;
    std::vector<NamespaceStateReferenceBinding> stateReferenceBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> stateReferenceBindingBySyntax_;
    bool built_ = false;
};
}
