#pragma once

#include <cstdint>
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
#include <utility>
#include <vector>

namespace nativesharp::frontend { class ProjectCompilation; struct NamespaceStateSymbol; struct PrecompiledSyntaxModule; struct CppFunctionDemand; }

namespace nativesharp::interop
{
struct CppSemanticInventoryEntry;
struct CppCEnumSnapshot;
struct LibraryMetadata;
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
    std::uint64_t byteSize = 0;
    std::uint64_t byteAlignment = 0;
    std::optional<std::string> clangUsr{};
    std::optional<std::uint64_t> bitOffset{};
    std::optional<std::uint64_t> bitWidth{};
    bool pointeeVolatile = false;
    // T19-038: the field is a C `_Atomic T*`. / El campo es un `_Atomic T*` de C.
    bool pointeeAtomic = false;
    // Physical element stride for each dimension of a non-contiguous C++ vector array.
    std::vector<std::uint64_t> cppArrayElementStrides{};
    // Clang type size and natural alignment for each array-view dimension.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> cppArrayPhysicalLayouts{};
    // T19-012. C++ access of the field. / Acceso C++ del campo.
    CppAccess access = CppAccess::Public;
    std::uint8_t pointerLevelQualifiers = 0; // C1-02: intermediate pointer level qualifiers (Language/PointerLevelQualifiers.h).
    bool atomicStorage = false; // C3-04: a C `_Atomic T` field. / Un campo C `_Atomic T`.
    // C3-05: the Microsoft storage unit a volatile access uses (absent when it is the field window). / La
    //     unidad de Microsoft que usa un acceso volatile (ausente si es la ventana del campo).
    std::optional<std::uint64_t> volatileUnitByteOffset{};
    std::optional<std::uint64_t> volatileUnitBitOffset{};
    std::optional<std::uint64_t> volatileUnitBytes{};
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
    CppAccess access = CppAccess::Public;
    // EN: T19-036. A C++ thread_local static field, accessed as Windows TLS; when it is not constant-initialized,
    //     through the CRT's on-demand TLS initialization, as Clang does.
    // ES: T19-036. Un campo estatico thread_local C++, accedido como TLS de Windows; sin inicializacion constante,
    //     con la inicializacion TLS bajo demanda del CRT, como en Clang.
    bool threadLocal = false;
    bool tlsOnDemand = false;
    // EN: T19-005. C++ reference storage: the symbol holds the address of the referent, loaded at each access.
    // ES: T19-005. Almacenamiento referencia C++: el simbolo guarda la direccion del referente, cargada en cada acceso.
    bool referenceStorage = false;
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
    // Effective option reported by the owning Clang session, if available.
    std::optional<bool> microsoftVolatileSemantics{};
};

struct CppRecordValueObservation
{
    CppRecordValueTraits traits;
    CppRecordValueOperations operations;
};
// The entry must come from the current inventory and a uniquely selected
// definition. Missing or ambiguous source sessions yield no observation.
std::optional<CppRecordValueObservation> observeCppRecordValueForEntry(
    const CppSemanticRecordEntry& entry, std::span<const CppProjectSessions> projects);

struct CppSemanticRecordFailure
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::string message;
};

using CppSemanticRecordsResult = std::variant<std::vector<CppSemanticRecordEntry>, CppSemanticRecordFailure>;

CppSemanticRecordsResult collectCppSemanticRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects);

// Produces an owner followed by internally named anonymous C aggregates, or a diagnostic.
// The input observation is never modified; this does not install or publish the results.
std::variant<std::vector<CppRecordDeclaration>, std::string> materializeCppAnonymousCRecords(
    const CppRecordDeclaration& owner);

enum class CppRecordGroupStatus { ForwardOnly, OneDefinition, MultipleDefinitionsUnverified,
                                  MultipleDefinitionsPointerCompatible, Conflict };
struct CppRecordGroup
{
    std::size_t projectOrdinal;
    compilation::TypeIdentity identity;
    std::vector<std::size_t> inventoryIndices;
    CppRecordGroupStatus status = CppRecordGroupStatus::ForwardOnly;
    std::optional<std::string> clangUsr = std::nullopt;
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

// EN: T19-005. An enumerator of an unnamed enum of a C++ record, a constant of the record with the enum's integer
//     type and access.
// ES: T19-005. Un enumerador de un enum sin nombre de un record C++, una constante del record con el tipo entero y el
//     acceso del enum.
struct CppRecordUnnamedEnumConstant
{
    std::string name;
    std::string typeKey;
    std::uint64_t bits = 0;
    CppAccess access = CppAccess::Public;
    bool operator==(const CppRecordUnnamedEnumConstant&) const = default;
};

// Owned C declaration data for a future metadata codec; it is not itself serialized.
struct CppCRecordSnapshot
{
    std::string identityKey;
    std::string moduleName;
    std::vector<std::string> namespaceSegments;
    std::vector<std::string> containingRecords;
    std::string name;
    std::string targetTriple;
    std::optional<std::string> clangUsr{};
    // Present in the versioned provenance catalogue when the source was observed.
    std::optional<CppRecordDeclaration::SourceProvenance> sourceProvenance;
    std::optional<bool> microsoftVolatileSemantics{};
    CppRecordKind kind = CppRecordKind::Struct;
    // EN: T19-005. Private or Protected for a non-public C++ record nested in another one; Public otherwise.
    // ES: T19-005. Private o Protected para un record C++ no publico anidado en otro; Public en otro caso.
    CppAccess nestedAccess = CppAccess::Public;
    bool complete = false;
    bool valueLayoutUsable = false;
    // Observed from every live definition by Clang when publishing this
    // catalogue. For C++ it requires a plain trivial standard-layout struct.
    // This is a syntax observation, not a complete ABI proof.
    bool plainReplaySyntax = false;
    bool packedReplaySyntax = false;
    // Present only for a C++ record with an observed, eligible value contract.
    // Its identity must agree with clangUsr; dimensions and fields remain below.
    std::optional<CppRecordValueObservation> cppValueObservation;
    // EN: T19-036 (T12-001). A C++ record that Clang returns through memory on the Microsoft ABI (not trivial for
    //     MSVC); set only with cppValueObservation. / ES: Un record C++ que Clang devuelve en memoria en el ABI de
    //     Microsoft (no trivial para MSVC); solo con cppValueObservation.
    bool cppIndirectReturn = false;
    // T19-005: the enumerators of its unnamed enums. / Los enumeradores de sus enums sin nombre.
    std::vector<CppRecordUnnamedEnumConstant> unnamedEnumConstants;
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    std::vector<CppRecordField> fields;
};
struct CppCRecordSnapshotFailure { std::string message; };
using CppCRecordSnapshotResult = std::variant<CppCRecordSnapshot, CppCRecordSnapshotFailure>;
CppCRecordSnapshotResult captureCppCRecordSnapshot(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group);
// The caller selects C groups. Returns no partial catalog on failure.
using CppCRecordCatalogResult = std::variant<std::vector<CppCRecordSnapshot>, CppCRecordSnapshotFailure>;
CppCRecordCatalogResult captureCppCRecordCatalog(
    std::span<const CppSemanticRecordEntry> inventory, std::span<const CppRecordGroup> groups);
struct CppCRecordClosure
{
    std::vector<CppCRecordSnapshot> records;
    std::vector<std::string> enumIdentityKeys;
    // EN: NSMeta X4-06. The C/C++ references (of the closure's project) whose C++ records are not byte values and stay
    //     out of the catalogue; their preprocessed source travels as a source session.
    // ES: NSMeta X4-06. Las referencias C/C++ (del proyecto del cierre) cuyos records C++ no son valores de bytes y no
    //     entran al catalogo; su fuente preprocesada viaja como sesion de fuente.
    std::vector<std::pair<std::size_t, std::string>> sourceSessionRecords{}; // (reference index, identity key)
};
using CppCRecordClosureResult = std::variant<CppCRecordClosure, CppCRecordSnapshotFailure>;
// Seeds and transitive field references are resolved within one project.
CppCRecordClosureResult collectCppCRecordClosure(
    std::span<const CppSemanticRecordEntry> inventory, std::span<const CppRecordGroup> groups,
    std::size_t projectOrdinal, std::span<const std::string> seedIdentityKeys,
    std::span<const CppProjectSessions> cppValueSessions = {},
    // NSMeta X4-06: references that already travel as source sessions; their records stay out of the catalogue.
    std::span<const std::size_t> sourceSessionReferences = {});
bool provideCppCRecordMetadataCatalog(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::vector<std::uint8_t>& output, std::string& error);
bool selectCppTypesWithPrecompiledCatalogs(const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
bool providePrecompiledCParserTypes(const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
bool reconstructCImportedFields(std::span<const CppCRecordSnapshot> records,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& output,
    std::string& error);
bool reconstructCEnumConstants(std::span<const CppCEnumSnapshot> enums,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::NamespaceStateSymbol>& output,
    std::string& error);
bool reconstructPrecompiledCMembers(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& fields,
    std::vector<frontend::NamespaceStateSymbol>& constants, std::string& error);
bool provideCppFieldsWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& output,
    std::string& error);
// T19-036: the live C++ static fields and those a precompiled template names. / Los vivos y los de una plantilla.
bool provideCppStaticFieldsWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedStaticField>& output,
    std::string& error);
bool provideCppValuesWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& output, std::string& error);

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
    struct Reconciliation
    {
        std::size_t projectOrdinal;
        std::string liveIdentityKey;
        std::string precompiledIdentityKey;
    };
    std::vector<CppSemanticRecordEntry> inventory;
    CppRecordGroups groups;
    std::vector<Reconciliation> reconciliations;
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
