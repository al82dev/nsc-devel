#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticInventory.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend { struct PrecompiledSyntaxModule; }

namespace nativesharp::interop::metadata_detail
{
// Internal catalogue codec. It does not by itself publish an NSMETA section.
// Both functions leave their output unchanged on failure.
bool encodeCRecordCatalog(std::span<const CppCRecordSnapshot> records,
                          std::vector<std::uint8_t>& output, std::string& error);
bool decodeCRecordCatalog(std::span<const std::uint8_t> bytes,
                          std::vector<CppCRecordSnapshot>& output, std::string& error);
bool encodeCTypeCatalog(std::span<const CppCRecordSnapshot> records,
                        std::span<const CppCEnumSnapshot> enums,
                        std::vector<std::uint8_t>& output, std::string& error);
bool decodeCTypeCatalog(std::span<const std::uint8_t> bytes,
                        std::vector<CppCRecordSnapshot>& records,
                        std::vector<CppCEnumSnapshot>& enums, std::string& error);
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
