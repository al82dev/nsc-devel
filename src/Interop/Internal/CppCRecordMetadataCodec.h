#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppTypeAliasDeclaration.h"

#include <cstdint>
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
};
bool encodeCDependencyFunction(const CDependencyFunctionSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyFunction(std::span<const std::uint8_t> bytes,
    CDependencyFunctionSnapshot& output, std::string& error);
struct CDependencyGlobalSnapshot
{
    std::string moduleName;
    std::string referenceIdentity;
    std::string linkSymbol;
    std::optional<bool> microsoftVolatileSemantics;
    CppGlobalDeclaration declaration;
};
bool encodeCDependencyGlobal(const CDependencyGlobalSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyGlobal(std::span<const std::uint8_t> bytes,
    CDependencyGlobalSnapshot& output, std::string& error);
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
struct CDependencyCatalog
{
    std::vector<CDependencyFunctionSnapshot> functions;
    std::vector<CDependencyGlobalSnapshot> globals;
    std::vector<CDependencyAliasSnapshot> aliases;
};
// Canonical independent snapshot; not yet embedded in a published library.
bool encodeCDependencyCatalog(const CDependencyCatalog& catalog,
    std::vector<std::uint8_t>& output, std::string& error);
bool decodeCDependencyCatalog(std::span<const std::uint8_t> bytes,
    CDependencyCatalog& output, std::string& error);
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
bool compareCRecordClosureEvidence(std::span<const CppCRecordSnapshot> leftRecords,
    std::span<const CppCEnumSnapshot> leftEnums, std::string_view leftRoot,
    std::span<const CppCRecordSnapshot> rightRecords, std::span<const CppCEnumSnapshot> rightEnums,
    std::string_view rightRoot, CRecordContractComparison& result, std::string& error);
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
    std::uint16_t formatMinor = 45);
// Reconstructs declarations for a single producer module; fields and constants
// are installed separately. Output remains unchanged on failure.
bool reconstructCImportedTypes(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, std::string_view sourcePath,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
bool reconstructPrecompiledCImportedTypes(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::string_view targetTriple,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error);
}
