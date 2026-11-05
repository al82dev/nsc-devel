#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppGlobalDeclaration.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nativesharp::frontend
{
struct CppFunctionDemand;
struct NamespaceStateSymbol;
}

namespace nativesharp::interop
{
struct CppSemanticInventoryEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppFunctionDeclaration declaration;
    CppSemanticFunctionResult projection;
};

struct CppSemanticInventoryFailure
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::string message;
};

using CppSemanticInventoryResult =
    std::variant<std::vector<CppSemanticInventoryEntry>, CppSemanticInventoryFailure>;

// The supplied graph must already contain its final set of projects. No declaration is
// discarded or merged; an unsupported projection remains attached to its declaration.
CppSemanticInventoryResult collectCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

// Preserve every observation and its owning project/reference. Repeated names are not
// merged here: compatibility and visibility must be resolved before binding an alias.
struct CppSemanticTypeAliasEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppTypeAliasDeclaration declaration;
};
using CppSemanticTypeAliasInventoryResult =
    std::variant<std::vector<CppSemanticTypeAliasEntry>, CppSemanticInventoryFailure>;
CppSemanticTypeAliasInventoryResult collectCppSemanticTypeAliases(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

struct CppSemanticGlobal
{
    std::size_t projectOrdinal;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    std::string typeKey;
    std::string canonicalCppType;
    std::string externalSymbol;
    bool isVolatile = false;
    bool isConst = false;
    bool pointerPointeeReadonly = false;
    bool dllImport = false;
};
struct CppSemanticGlobalFailure { std::string message; };
using CppSemanticGlobalResult = std::variant<CppSemanticGlobal, CppSemanticGlobalFailure>;

// Projection retains unsupported declarations as failures; it does not create storage or an accessor.
CppSemanticGlobalResult projectCppSemanticGlobal(
    const CppGlobalDeclaration& global, std::size_t projectOrdinal, std::string_view moduleName);
struct CppSemanticGlobalEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppGlobalDeclaration declaration;
    CppSemanticGlobalResult projection;
};
using CppSemanticGlobalInventoryResult =
    std::variant<std::vector<CppSemanticGlobalEntry>, CppSemanticInventoryFailure>;
CppSemanticGlobalInventoryResult collectCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

// One requested name and all its origins. A candidate is supplied only when every
// external redeclaration and alias of its symbol agrees and can be projected.
// Indices always refer to the caller's inventory, including unsupported entries.
struct CppSemanticGlobalSelection
{
    std::vector<std::size_t> declarationIndices;
    std::vector<std::size_t> aliasIndices;
    std::vector<std::size_t> unsupportedIndices;
    std::vector<std::size_t> internalIndices;
    std::optional<CppSemanticGlobal> candidate;
    std::string conflict;
};
CppSemanticGlobalSelection selectCppSemanticGlobal(
    std::span<const CppSemanticGlobalEntry> entries, std::size_t projectOrdinal,
    std::string_view qualifiedName);
bool provideCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error);
}
