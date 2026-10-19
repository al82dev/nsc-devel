#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h"
#include "nativesharp/Frontend/NamespaceStateInitializerKind.h"

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
}
