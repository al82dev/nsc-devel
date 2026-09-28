#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
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
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::vector<MethodParameterSymbol> parameters;
    std::string signatureKey;
    std::string callableIdentityKey;
    std::string mangledName;
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* definitionSyntax = nullptr;
    std::vector<NamespaceFunctionPart> parts;
    bool isReferenceReturn = false;
    bool isVariadic = false;
    bool isExternC = false;
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
};

class ProjectNamespaceFunctionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);

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
    std::span<const NamespaceFunctionReferenceBinding> referenceBindings() const noexcept;
    bool isBuilt() const noexcept;

private:
    std::vector<NamespaceFunctionSymbol> functions_;
    std::vector<NamespaceStateSymbol> states_;
    std::unordered_map<std::string, std::size_t> stateByName_;
    std::unordered_map<std::string, std::vector<std::size_t>> functionsByName_;
    std::vector<NamespaceFunctionReferenceBinding> referenceBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> referenceBindingBySyntax_;
    bool built_ = false;
};
}