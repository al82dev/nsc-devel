#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h"
#include "nativesharp/Frontend/NamespaceStateInitializerKind.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <optional>
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
    bool isCppExternal = false;
    // A C enumerator is a value, not an externally linked variable.
    bool isCppEnumConstant = false;
    bool cppDllImport = false;
    bool isVolatile = false;
    bool isCppReadonly = false;
    bool cppPointerPointeeReadonly = false;
    // Nonempty when a C/C++ declaration cannot yet be represented. Report only
    // if semantic lookup selects it; locals and members may shadow the name.
    std::string cppImportError;
    bool cppPointerPointeeVolatile = false;
    // Only populated for a C/C++ global; absence means the option is unknown.
    std::optional<bool> cppMicrosoftVolatileSemantics;
    std::optional<std::uint64_t> cppStorageSizeBits;
    std::optional<std::uint64_t> cppDeclarationAlignmentBits;
    std::optional<std::string> cppClangUsr;
    // True only when every reconciled global declaration was parsed as C.
    bool cppCGlobal = false;
    // Counterpart established through a reconciled enum, not by the value name alone.
    std::optional<std::string> cppReconciledStateKey;
};
}
