#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
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
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* definitionSyntax = nullptr;
    std::vector<NamespaceFunctionPart> parts;
    bool isReferenceReturn = false;
    bool isVariadic = false;
    bool isExternC = false;
};

class ProjectNamespaceFunctionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const NamespaceFunctionSymbol> functions() const noexcept;
    std::span<const std::size_t> findFunctions(
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view name) const noexcept;
    bool isBuilt() const noexcept;

private:
    std::vector<NamespaceFunctionSymbol> functions_;
    std::unordered_map<std::string, std::vector<std::size_t>> functionsByName_;
    bool built_ = false;
};
}