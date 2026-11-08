#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppGlobalDeclaration.h"

#include <filesystem>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nativesharp::frontend
{
struct CppFunctionDemand;
struct CppImportedTypeAlias;
struct CppImportedTypeDeclaration;
struct NamespaceStateSymbol;
struct PrecompiledSyntaxModule;
class ProjectNameLookup;
class ProjectCompilation;
}

namespace nativesharp::interop
{
struct LibraryMetadata;
namespace metadata_detail { struct CDependencyGlobalSnapshot; struct CDependencyAliasSnapshot; }
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

// Internal C definitions receive a per-project/reference link symbol, not a C ABI name.
std::string cppCInlineLinkSymbol(const std::filesystem::path& projectFile,
    std::size_t referenceIndex, std::string_view name);

// The supplied graph must already contain its final set of projects. No declaration is
// discarded or merged; an unsupported projection remains attached to its declaration.
CppSemanticInventoryResult collectCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

enum class CppSignatureEnumStatus { Matched, Missing, Unprojectable };
struct CppSignatureEnumDependency
{
    std::size_t functionIndex;
    std::optional<std::size_t> parameterIndex;
    std::size_t indirectionDepth;
    CppSignatureEnumStatus status;
    std::optional<std::size_t> candidateIndex;
};
struct CppSignatureEnumFailure { std::string message; };
using CppSignatureEnumResult = std::variant<std::vector<CppSignatureEnumDependency>, CppSignatureEnumFailure>;
// Inventory indices are retained; this does not itself select functions or prove ABI compatibility.
CppSignatureEnumResult collectCppSignatureEnumDependencies(
    const compilation::ProjectGraph& graph, std::span<const CppSemanticInventoryEntry> functions,
    std::span<const frontend::CppImportedTypeDeclaration> enumCandidates);

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
// Reconstructs the alias under its producer project. The caller maps the saved
// reference identity to a reference index; this does not install the alias.
bool reconstructCppPrecompiledCAlias(const metadata_detail::CDependencyAliasSnapshot& alias,
    const compilation::ProjectGraph& graph, std::size_t projectOrdinal, std::size_t referenceIndex,
    std::string_view targetTriple, frontend::CppImportedTypeAlias& output, std::string& error);
struct CppPublishedTemplateAliasCandidates
{
    std::string templateKey;
    std::size_t candidateIndex;
    // Indices into the supplied alias inventory; these are not resolved bindings.
    std::vector<std::size_t> inventoryIndices;
    // Present only when name binding expanded an alias for this exact syntax.
    const frontend::CppImportedTypeAlias* resolvedAlias = nullptr;
    bool hasNameBinding = false;
};
std::vector<CppPublishedTemplateAliasCandidates> selectCppPublishedTemplateAliases(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticTypeAliasEntry> inventory);
// Retains matching observations when a generic body has not been bound yet;
// those observations remain candidates until the consumer resolves the name.
bool captureCppTemplateAliasCandidates(const compilation::ProjectGraph& graph,
    std::span<const CppPublishedTemplateAliasCandidates> candidates,
    std::span<const CppSemanticTypeAliasEntry> inventory,
    std::vector<metadata_detail::CDependencyAliasSnapshot>& output, std::string& error);
bool provideCppTypeAliases(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeAlias>& output, std::string& error);
bool provideCppTypeAliasesWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules,
    std::vector<frontend::CppImportedTypeAlias>& output, std::string& error);

struct CppSemanticEnumEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppEnumDeclaration declaration;
};
using CppSemanticEnumInventoryResult =
    std::variant<std::vector<CppSemanticEnumEntry>, CppSemanticInventoryFailure>;
// Anonymous enums and every observation remain distinct until their scope and
// declarations have been resolved; values are not inserted into NSC here.
CppSemanticEnumInventoryResult collectCppSemanticEnums(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);
struct CppCEnumSnapshot
{
    std::string identityKey;
    std::string targetTriple;
    CppEnumDeclaration declaration;
};
struct CppCEnumSnapshotFailure { std::string message; };
using CppCEnumCatalogResult = std::variant<std::vector<CppCEnumSnapshot>, CppCEnumSnapshotFailure>;
CppCEnumCatalogResult captureCppCEnumCatalog(
    std::span<const CppSemanticEnumEntry> inventory, std::size_t projectOrdinal,
    std::string_view moduleName, std::span<const std::string> requiredIdentityKeys);
bool provideCppEnumTypes(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& types, std::string& error);
bool provideCppInteropTypes(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& types, std::string& error);

struct CppSemanticEnumConstant
{
    std::size_t projectOrdinal;
    std::string name;
    std::string typeKey;
    std::uint64_t bits;
    std::optional<std::string> clangUsr;
};
struct CppSemanticEnumConstantFailure { std::string message; };
using CppSemanticEnumConstantResult =
    std::variant<CppSemanticEnumConstant, CppSemanticEnumConstantFailure>;
// Converts an exact C enumerator to the value representation used by NSC.
// It does not insert the name or establish constant-expression semantics.
CppSemanticEnumConstantResult projectCppEnumConstant(
    const CppSemanticEnumEntry& entry, std::size_t enumeratorIndex);
bool provideCppSemanticEnumConstants(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error);

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
    bool pointerPointeeVolatile = false;
    // Absent if the originating sessions do not provide a single known value.
    std::optional<bool> microsoftVolatileSemantics;
    std::optional<std::uint64_t> storageSizeBits;
    std::optional<std::uint64_t> declarationAlignmentBits;
    std::optional<std::string> clangUsr;
    bool threadLocal = false;
    // Distinct from volatile; set only when the imported storage is C atomic.
    bool atomicStorage = false;
};
struct CppSemanticGlobalFailure { std::string message; };
using CppSemanticGlobalResult = std::variant<CppSemanticGlobal, CppSemanticGlobalFailure>;

// Projection retains unsupported declarations as failures; it does not create storage or an accessor.
CppSemanticGlobalResult projectCppSemanticGlobal(
    const CppGlobalDeclaration& global, std::size_t projectOrdinal, std::string_view moduleName);
// Reprojects a saved C global; it does not install storage or resolve a DLL's TLS origin.
CppSemanticGlobalResult projectCppPrecompiledCGlobal(
    const metadata_detail::CDependencyGlobalSnapshot& global, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup);
struct CppSemanticGlobalEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppGlobalDeclaration declaration;
    CppSemanticGlobalResult projection;
    // Record storage is only projected under the C object model.
    bool cLanguage = false;
    // Effective option of the originating session; absent when unavailable.
    std::optional<bool> microsoftVolatileSemantics;
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
struct CppPublishedTemplateGlobalCandidates
{
    std::string templateKey;
    std::size_t templateDemandIndex;
    std::size_t projectOrdinal;
    std::string namespaceName;
    CppSemanticGlobalSelection selection;
};
// Names remain candidates until body analysis resolves locals and members.
std::vector<CppPublishedTemplateGlobalCandidates> selectCppPublishedTemplateGlobals(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticGlobalEntry> inventory);
bool captureCppTemplateGlobalCandidates(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::span<const CppSemanticGlobalEntry> inventory,
    std::vector<metadata_detail::CDependencyGlobalSnapshot>& output, std::string& error);
bool provideCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error);
bool provideCppSemanticValues(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error);
bool provideCppSemanticValuesWithLookup(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error);
}
