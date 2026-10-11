#pragma once

#include "nsc_suite/TestRegistry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::linker_support
{
struct LinkerContractDescriptor final
{
    std::string_view name;
    std::string_view layer;
};

inline constexpr std::array<LinkerContractDescriptor, 40> kLinkerContracts{{
    {"COFF archive decimal fields headers and alignment", "unit"},
    {"COFF first linker member and symbol table", "unit"},
    {"Complete COFF archive verification", "unit"},
    {"Linker environment temporary and path encoding", "unit"},
    {"NSLink discovery identity and executable selection", "integration"},
    {"Librarian discovery and selection", "integration"},
    {"Response file serialization", "unit"},
    {"Object and import-library input validation", "unit"},
    {"Linker child process log and exit handling", "integration"},
    {"Import library definition and generation", "integration"},
    {"Static library creation", "integration"},
    {"Dynamic library link arguments and products", "integration"},
    {"Executable link subsystem entry and PDB", "integration"},
    {"PDB primitive parsing ranges and block sizes", "unit"},
    {"PDB MSF superblock stream directory and bounds", "unit"},
    {"PDB identity DBI modules publics sources and checksums", "unit"},
    {"PE primitive reads sections RVA and strings", "unit"},
    {"PE DOS COFF optional header and section validation", "unit"},
    {"PE export directory and names", "unit"},
    {"PE CodeView RSDS debug contract", "unit"},
    {"PE import directory thunks libraries and symbols", "unit"},
    {"PE executable and dynamic-library classification", "unit"},
    {"Executable temporary directory and COFF verification", "unit"},
    {"Executable incremental identity state and reuse", "integration"},
    {"Executable build from LLVM modules", "integration"},
    {"Executable build from precompiled objects", "integration"},
    {"Executable runtime imports and extra libraries", "integration"},
    {"Executable configuration result and verification", "integration"},
    {"Library temporary outputs COFF and DLL anchor", "unit"},
    {"Static library build from LLVM modules", "integration"},
    {"Dynamic library build from LLVM modules", "integration"},
    {"Static library objects cache runtime and dependency flattening", "integration"},
    {"Dynamic library objects imports exports PDB and verification", "integration"},
    {"Library builder configuration cache telemetry and results", "unit"},
    {"Windows runtime import catalogue and conditional support", "unit"},
    {"External import merge deduplication and import libraries", "integration"},
    {"Windows runtime library resolution and configured override", "unit"},
    {"Windows runtime library ABI verification", "unit"},
    {"Independent Windows runtime library construction and publication", "integration"},
    {"Cross-product cleanup and no-partial-publication safety", "integration"},
}};

/**
 * Formats one stable Phase 1 case identifier. The mapping is arithmetic and
 * preserves exactly eight consecutive cases per contract in O(1) time.
 */
inline std::string caseId(std::size_t contract, std::size_t variant)
{
    std::ostringstream output;
    output << "TF-LINKER-" << std::setw(3) << std::setfill('0') << ((contract - 1) * 8 + variant);
    return output.str();
}

/** Formats one stable Phase 1 contract identifier in O(1) time. */
inline std::string contractId(std::size_t contract)
{
    std::ostringstream output;
    output << "CF-LINKER-" << std::setw(3) << std::setfill('0') << contract;
    return output.str();
}

/** Returns the frozen descriptor for a one-based contract ordinal. */
inline const LinkerContractDescriptor& contractDescriptor(std::size_t contract)
{
    return kLinkerContracts.at(contract - 1);
}

/** Builds the exact independent-oracle description frozen by Phase 1. */
inline std::string oracleDescription(std::size_t contract)
{
    return "independent structural oracle for " + std::string(contractDescriptor(contract).name);
}

/**
 * Registers one executable case with its frozen case, contract, component,
 * layer, profile and oracle lineage. The supplied function is the real oracle.
 */
inline void addExecutableCase(TestRegistry& registry, std::size_t contract, std::size_t variant,
    const std::function<void(TestContext&, std::size_t)>& function,
    TestDisposition disposition = TestDisposition::Required)
{
    const LinkerContractDescriptor& descriptor = contractDescriptor(contract);
    const std::string identifier = contractId(contract);
    registry.add(TestCase{
        caseId(contract, variant),
        identifier,
        "Host.Linker",
        std::string(descriptor.layer),
        variant == 8 ? "NIGHTLY" : "PR",
        {"linker", identifier, "variant-" + std::to_string(variant), oracleDescription(contract)},
        [function, variant](TestContext& context) { function(context, variant); },
        disposition,
    });
}

/**
 * Registers the selected executable variants of one mixed contract.
 */
inline void addMixedContract(TestRegistry& registry, std::size_t contract,
    std::initializer_list<std::size_t> executableVariants,
    const std::function<void(TestContext&, std::size_t)>& function)
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        const bool executable =
            std::find(executableVariants.begin(), executableVariants.end(), variant) != executableVariants.end();
        if (executable)
        {
            addExecutableCase(registry, contract, variant, function);
        }
    }
}
}
