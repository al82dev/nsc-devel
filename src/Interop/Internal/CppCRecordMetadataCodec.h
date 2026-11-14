#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppTypeAliasDeclaration.h"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend { struct PrecompiledSyntaxModule; }

namespace nativesharp::interop::metadata_detail
{
// Private building block for future C dependency snapshots, not an NSMETA format.
// A failed operation leaves its output unchanged.
bool encodeCDependencyType(const CppType& type, std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyType(std::span<const std::uint8_t> bytes, CppType& output, std::string& error);
// A function snapshot retains the producer's reference identity and the actual
// symbol chosen for its linkage. Source locations are diagnostic-only and are
// deliberately not transported. This is not yet an NSMETA section.
struct CDependencyFunctionSnapshot
{
    std::string moduleName;
    std::string referenceIdentity;
    std::string linkSymbol;
    CppFunctionDeclaration declaration;
    // Clang's plan for the fixed prefix and no optional arguments. It anchors
    // later reconstructions but does not classify any aggregate by itself.
    std::shared_ptr<const abi_plan::VariadicCallPlan> variadicBasePlan;
    // EN: T08-D13b. Volatile mode of the producer's reference; it orders accesses through a returned
    //     `volatile T*` in the consumer.
    // ES: T08-D13b. Modo volatile de la referencia del productor; ordena en el consumidor los accesos por un
    //     `volatile T*` devuelto.
    std::optional<bool> microsoftVolatileSemantics;
};
bool encodeCDependencyFunction(const CDependencyFunctionSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyFunction(std::span<const std::uint8_t> bytes,
    CDependencyFunctionSnapshot& output, std::string& error);
// Captures one explicitly bound function from its exact project/reference; it
// does not treat speculative name-lookup candidates as selected declarations.
bool captureCDependencyFunction(std::span<const CppSemanticInventoryEntry> inventory,
    const frontend::NamespaceFunctionSymbol& selected, std::size_t referenceIndex,
    std::string_view referenceIdentity, CDependencyFunctionSnapshot& output, std::string& error);
struct CDependencyGlobalSnapshot
{
    std::string moduleName;
    std::string referenceIdentity;
    std::string linkSymbol;
    std::optional<bool> microsoftVolatileSemantics;
    // Includes a DLL TLS origin supplied by the project reference, even when
    // the C declaration itself has no dllimport attribute.
    bool effectiveDllImport = false;
    CppGlobalDeclaration declaration;
};
bool encodeCDependencyGlobal(const CDependencyGlobalSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyGlobal(std::span<const std::uint8_t> bytes,
    CDependencyGlobalSnapshot& output, std::string& error);
bool captureCDependencyGlobal(std::span<const CppSemanticGlobalEntry> inventory,
    const frontend::NamespaceStateSymbol& selected, std::size_t referenceIndex,
    std::string_view referenceIdentity, CDependencyGlobalSnapshot& output, std::string& error);
struct CDependencyAliasSnapshot
{
    std::string moduleName;
    std::string referenceIdentity;
    CppTypeAliasDeclaration declaration;
};
bool encodeCDependencyAlias(const CDependencyAliasSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyAlias(std::span<const std::uint8_t> bytes,
    CDependencyAliasSnapshot& output, std::string& error);
bool captureCDependencyAlias(std::span<const CppSemanticTypeAliasEntry> inventory,
    const frontend::CppImportedTypeAlias& selected, std::string_view moduleName,
    std::string_view referenceIdentity, CDependencyAliasSnapshot& output, std::string& error);
struct CDependencyCatalog
{
    struct StaticMethod
    {
        std::string ownerIdentityKey;
        std::string ownerClangUsr;
        CDependencyFunctionSnapshot function;
    };
    struct Enumerator
    {
        std::string moduleName;
        std::string referenceIdentity;
        std::string name;
        std::string clangUsr;
        std::string typeKey;
        std::uint64_t bits = 0;
    };
    std::vector<CDependencyFunctionSnapshot> functions;
    std::vector<CDependencyGlobalSnapshot> globals;
    std::vector<CDependencyAliasSnapshot> aliases;
    std::vector<Enumerator> enumerators;
    std::vector<StaticMethod> staticMethods;
};
// Prepared version 7 envelope; publication and version dispatch remain separate.
bool encodeCTypeCatalogWithEnumProvenance(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalogWithEnumProvenance(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error);
// Version 6 envelope: base catalogue plus explicit anonymous C carrier indices.
// These helpers are not connected to NSMETA publication or dispatch yet.
bool encodeCTypeCatalogWithAnonymousCarriers(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalogWithAnonymousCarriers(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error);
// Canonical independent snapshot; not yet embedded in a published library.
bool encodeCDependencyCatalog(const CDependencyCatalog& catalog,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyCatalog(std::span<const std::uint8_t> bytes,
    CDependencyCatalog& output, std::string& error);
// Prepared envelope only. Callers must select it using an explicit metadata
// version and verify target, ownership, binding and ABI projection before use.
bool encodeCTypeCatalogWithDependencies(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalogWithDependencies(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error);
// Explicit-version dispatch, prepared for a future metadata revision. Existing
// readers remain unchanged until verification and ABI projection are connected.
bool decodeCTypeCatalogAndDependencies(std::span<const std::uint8_t> bytes, std::uint16_t formatMinor,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error);
// Internal catalogue codec. It does not by itself publish an NSMETA section.
// Both functions leave their output unchanged on failure.
bool encodeCRecordCatalog(std::span<const CppCRecordSnapshot> records,
                          std::vector<std::uint8_t>& output, std::string& error);
bool decodeCRecordCatalog(std::span<const std::uint8_t> bytes,
                          std::vector<CppCRecordSnapshot>& output, std::string& error);
// Matching serialized contracts are not proof of common declaration provenance.
enum class CRecordContractComparison { Different, Insufficient, Matching };
bool compareCRecordContracts(const CppCRecordSnapshot& left, const CppCRecordSnapshot& right,
                             CRecordContractComparison& result, std::string& error);
// Evidence concerns the root declaration only, not its transitive type closure.
bool compareCRecordSourceEvidence(const CppCRecordSnapshot& left, const CppCRecordSnapshot& right,
                                  CRecordContractComparison& result, std::string& error);
bool compareCEnumSourceEvidence(const CppCEnumSnapshot& left, const CppCEnumSnapshot& right,
                                CRecordContractComparison& result, std::string& error,
                                bool allowEquivalentDistinctSources = false,
                                bool allowBothSourcesMissing = false);
bool compareCRecordClosureEvidence(std::span<const CppCRecordSnapshot> leftRecords,
    std::span<const CppCEnumSnapshot> leftEnums, std::string_view leftRoot,
    std::span<const CppCRecordSnapshot> rightRecords, std::span<const CppCEnumSnapshot> rightEnums,
    std::string_view rightRoot, CRecordContractComparison& result, std::string& error,
    bool allowBothSourcesMissing = false, bool contractsOnly = false);
bool encodeCTypeCatalog(std::span<const CppCRecordSnapshot> records,
                        std::span<const CppCEnumSnapshot> enums,
                        std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalog(std::span<const std::uint8_t> bytes,
                        std::vector<CppCRecordSnapshot>& records,
                        std::vector<CppCEnumSnapshot>& enums, std::string& error);
// Prepared versioned envelope; not used by NSMETA 1.42.
bool encodeCTypeCatalogWithProvenance(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalogWithProvenance(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error);
bool isCTypeCatalogWithProvenance(std::span<const std::uint8_t> bytes) noexcept;
bool decodeVersionedCTypeCatalog(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error,
    std::uint16_t formatMinor);
// When the enclosing metadata is unavailable, validate the complete input and
// refuse ambiguous interpretations instead of guessing from the first byte.
bool decodeVersionedCTypeCatalog(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error);
// Reconstructs declarations for a single producer module; fields and constants
// are installed separately. Output remains unchanged on failure.
bool reconstructCImportedTypes(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, std::string_view sourcePath,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
bool reconstructPrecompiledCImportedTypes(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::string_view targetTriple,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
// EN: T08-D26. Maps each C record or enum of a precompiled producer to the equivalent declaration of the first
//     producer (by project order) that publishes it: same identity (tag, scope, kind, target and USR) and the same
//     declaration contract over its whole type closure. Canonical declarations are not keys of the map.
// ES: T08-D26. Asocia cada record o enum C de un productor precompilado con la declaracion equivalente del primer
//     productor (en el orden de proyectos) que la publica: misma identidad (etiqueta, ambito, clase, destino y USR)
//     y el mismo contrato de declaracion en toda su clausura de tipos. Las canonicas no son claves del mapa.
bool reconcilePrecompiledCTypes(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::map<std::string, std::string>& output, std::string& error);
}
