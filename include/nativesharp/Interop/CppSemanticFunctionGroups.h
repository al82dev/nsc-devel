#pragma once

#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppDeferredFunctionCandidate.h"

#include <cstddef>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nativesharp::frontend { struct CppFunctionDemand; class ProjectNameLookup; class ProjectCompilation; }

namespace nativesharp::interop
{
struct LibraryMetadata;
namespace metadata_detail { struct CDependencyFunctionSnapshot; }
struct CppSemanticFunctionGroup
{
    frontend::NamespaceFunctionSymbol symbol;
    std::vector<std::size_t> inventoryIndices;
};

struct CppSemanticFunctionGroups
{
    std::vector<CppSemanticFunctionGroup> groups;
    std::vector<std::size_t> unprojectedIndices;
};

struct CppSemanticFunctionGroupFailure
{
    std::size_t firstIndex;
    std::size_t conflictingIndex;
    std::string message;
};

using CppSemanticFunctionGroupsResult =
    std::variant<CppSemanticFunctionGroups, CppSemanticFunctionGroupFailure>;

// Groups candidates for semantic lookup; this does not prove ODR or binary compatibility.
CppSemanticFunctionGroupsResult groupCppSemanticFunctions(
    std::span<const CppSemanticInventoryEntry> entries);

// Restricts lookup to one C++ qualified name while checking aliases of its external symbols.
// All returned inventory indices refer to the original, unfiltered input.
CppSemanticFunctionGroupsResult selectCppSemanticFunctions(
    std::span<const CppSemanticInventoryEntry> entries, std::size_t projectOrdinal,
    std::string_view qualifiedName);

struct CppSemanticDemandCandidates
{
    std::size_t demandIndex;
    std::size_t projectOrdinal;
    std::string namespaceName;
    CppSemanticFunctionGroups candidates;
    std::optional<CppSemanticFunctionGroupFailure> deferredConflict;
};

struct CppSemanticDemandFailure
{
    std::size_t demandIndex;
    CppSemanticFunctionGroupFailure conflict;
};

using CppSemanticDemandSelection =
    std::variant<std::vector<CppSemanticDemandCandidates>, CppSemanticDemandFailure>;

CppSemanticDemandSelection selectCppSemanticDemands(
    std::span<const CppSemanticInventoryEntry> entries,
    std::span<const frontend::CppFunctionDemand> demands, bool deferConflicts = false);

struct CppPublishedTemplateFunctionCandidates
{
    std::string templateKey;
    // Index into ProjectCompilation::cppTemplateDemands(), not the filtered demand list.
    std::size_t templateDemandIndex;
    CppSemanticDemandCandidates selection;
};
// Candidate groups only: local shadowing and overload selection still happen later.
// Inventory indices and deferred conflicts retain their original provenance.
std::vector<CppPublishedTemplateFunctionCandidates> selectCppPublishedTemplateFunctions(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticInventoryEntry> inventory);
// Captures projected external C candidates, retaining their original reference.
// Deferred conflicts and unprojected declarations remain outside this result.
bool captureCppTemplateFunctionCandidates(const compilation::ProjectGraph& graph,
    std::span<const CppPublishedTemplateFunctionCandidates> candidates,
    std::span<const CppSemanticInventoryEntry> inventory,
    std::vector<metadata_detail::CDependencyFunctionSnapshot>& output, std::string& error);

// Returns candidate indices for enums appearing in functions selected by the supplied demands.
// Unprojected and conflicting overloads are included because later resolution may need them.
std::variant<std::vector<std::size_t>, CppSignatureEnumFailure> selectCppDemandedEnumCandidates(
    std::span<const CppSignatureEnumDependency> dependencies,
    std::span<const CppSemanticDemandCandidates> demands, std::size_t functionCount,
    std::size_t enumCandidateCount);

struct CppDeferredFunctionFailure
{
    std::size_t selectionIndex;
    std::string message;
};

using CppDeferredFunctionResult =
    std::variant<std::vector<CppDeferredFunctionCandidate>, CppDeferredFunctionFailure>;

// The copied declarations outlive the inventory; syntax remains owned by the compilation.
CppDeferredFunctionResult materializeCppDeferredFunctions(
    std::span<const CppSemanticInventoryEntry> entries,
    std::span<const frontend::CppFunctionDemand> demands,
    std::span<const CppSemanticDemandCandidates> selections);

bool provideCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error);

// Isolated route: record signatures are projected only for types installed as imported C++ records.
bool provideExperimentalCppFunctionsWithRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error);
bool provideCppFunctionsWithPrecompiledCatalogs(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error);
}
