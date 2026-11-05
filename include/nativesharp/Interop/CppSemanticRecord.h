#pragma once

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/CppImportedInstanceField.h"
#include "nativesharp/Frontend/CppImportedStaticField.h"

#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace nativesharp::frontend { class ProjectCompilation; }

namespace nativesharp::interop
{
struct CppSemanticInventoryEntry;
// Anonymous contexts cannot be addressed by a NativeSharp source name.
std::optional<compilation::TypeIdentity> projectCppRecordIdentity(
    const CppRecordDeclaration& record, std::string_view moduleName);
std::optional<compilation::TypeIdentity> projectCppRecordTypeIdentity(
    const CppType& type, std::string_view moduleName);

// Isolated field metadata; no object lifetime or field access is enabled by this projection.
struct CppSemanticInstanceField
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    CppScalarTypeMapping type;
    std::uint64_t byteOffset;
    bool pointeeReadonly = false;
};
std::optional<CppSemanticInstanceField> projectCppSemanticInstanceField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup = nullptr);
frontend::CppImportedInstanceField toCppImportedInstanceField(const CppSemanticInstanceField& field);
struct CppSemanticStaticField
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    CppScalarTypeMapping type;
    bool dllImport;
    bool pointeeReadonly = false;
};
std::optional<CppSemanticStaticField> projectCppSemanticStaticField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup = nullptr);
frontend::CppImportedStaticField toCppImportedStaticField(const CppSemanticStaticField& field);
bool provideExperimentalCppStaticFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticField>& output, std::string& error);
bool provideExperimentalCppInstanceFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceField>& output, std::string& error);

struct CppSemanticRecordEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string targetTriple;
    CppRecordDeclaration declaration;
    std::optional<compilation::TypeIdentity> identity;
};

struct CppSemanticRecordFailure
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::string message;
};

using CppSemanticRecordsResult = std::variant<std::vector<CppSemanticRecordEntry>, CppSemanticRecordFailure>;

CppSemanticRecordsResult collectCppSemanticRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

enum class CppRecordGroupStatus { ForwardOnly, OneDefinition, MultipleDefinitionsUnverified,
                                  MultipleDefinitionsPointerCompatible, Conflict };
struct CppRecordGroup
{
    std::size_t projectOrdinal;
    compilation::TypeIdentity identity;
    std::vector<std::size_t> inventoryIndices;
    CppRecordGroupStatus status = CppRecordGroupStatus::ForwardOnly;
};
struct CppRecordGroups
{
    std::vector<CppRecordGroup> groups;
    std::vector<std::size_t> unprojectedIndices;
};
CppRecordGroups groupCppSemanticRecords(std::span<const CppSemanticRecordEntry> inventory);

enum class CppRecordStructuralVerdict { Equivalent, Different, Unavailable };
struct CppRecordStructuralComparison
{
    std::size_t groupIndex;
    std::size_t comparedDefinitions;
    CppRecordStructuralVerdict verdict;
};
struct CppRecordStructuralComparisonFailure { std::string message; };
using CppRecordStructuralComparisonResult =
    std::variant<std::vector<CppRecordStructuralComparison>, CppRecordStructuralComparisonFailure>;
// Evidence only: structural equivalence is not a proof of ODR or complete ABI equivalence.
CppRecordStructuralComparisonResult compareCppRecordDefinitions(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects);
using CppRecordRefinementResult = std::variant<CppRecordGroups, CppRecordStructuralComparisonFailure>;
// Experimental evidence permits pointer transport only; it does not establish ODR or value ABI.
CppRecordRefinementResult refineCppRecordGroupsForPointerTransport(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects);
// Only chooses a repeated definition when its source and captured member contracts agree.
std::optional<std::size_t> selectCppRecordMemberDefinition(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group);

enum class CppSignatureRecordStatus { Matched, Unprojectable, Missing };
struct CppSignatureRecordDependency
{
    std::size_t functionIndex;
    std::optional<std::size_t> parameterIndex;
    std::size_t indirectionDepth;
    CppSignatureRecordStatus status;
    std::optional<std::size_t> groupIndex;
};
struct CppSignatureRecordFailure { std::string message; };
using CppSignatureRecordResult = std::variant<std::vector<CppSignatureRecordDependency>, CppSignatureRecordFailure>;
CppSignatureRecordResult collectCppSignatureRecordDependencies(
    const compilation::ProjectGraph& graph, std::span<const CppSemanticInventoryEntry> functions,
    const CppRecordGroups& groups);

struct CppRecordObservationInputs
{
    std::vector<frontend::CppImportedTypeDeclaration> types;
    std::vector<std::size_t> groupByImportedType;
    std::vector<std::size_t> deferredGroups;
};
struct CppRecordObservationFailure { std::string message; };
using CppRecordObservationResult = std::variant<CppRecordObservationInputs, CppRecordObservationFailure>;
CppRecordObservationResult prepareCppRecordObservation(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups);

// Isolated provider for exercising the final project graph; not enabled by the driver.
bool provideExperimentalCppRecordTypes(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
bool provideExperimentalCppRecordCandidates(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);

struct CppRecordBindingSelection
{
    std::vector<std::size_t> referencedGroups;
    std::vector<std::size_t> expressionCandidateGroups;
};
struct CppRecordBindingFailure { std::string message; };
using CppRecordBindingResult = std::variant<CppRecordBindingSelection, CppRecordBindingFailure>;

CppRecordBindingResult selectCppRecordBindings(
    const frontend::ProjectNameLookup& lookup, const CppRecordGroups& groups,
    std::span<const std::size_t> groupByImportedType);

struct CppRecordObservation
{
    std::vector<CppSemanticRecordEntry> inventory;
    CppRecordGroups groups;
    CppRecordBindingSelection selection;
    std::vector<frontend::CppImportedTypeDeclaration> referencedEnumTypes;
    std::vector<std::string> collidingEnumIdentities;
    std::vector<std::size_t> deferredGroups;
    std::vector<std::size_t> collidingGroups;
    std::vector<std::size_t> referencedCollidingGroups;
    std::vector<std::size_t> expressionCollidingGroups;
};
using CppRecordObservationRunResult = std::variant<CppRecordObservation, CppRecordObservationFailure>;
CppRecordObservationRunResult observeCppRecords(const frontend::ProjectCompilation& compilation);

// Experimental: prepare lookup candidates after observing native collisions and explicit uses.
// Expression candidates are conservative and are not proof of a resolved expression use.
CppRecordObservationResult selectExperimentalCppRecordTypes(const CppRecordObservation& observation);
bool selectExperimentalCppRecordTypesForCompilation(
    const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
}
