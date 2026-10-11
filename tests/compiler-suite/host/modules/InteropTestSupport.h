#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryCapabilityAudit.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::interop_support
{
using namespace nativesharp;

inline std::string caseId(std::size_t contract, std::size_t variant)
{
    std::ostringstream output;
    output << "TF-INTEROP-" << std::setw(3) << std::setfill('0') << ((contract - 1) * 8 + variant);
    return output.str();
}

inline std::string contractId(std::size_t contract)
{
    std::ostringstream output;
    output << "CF-INTEROP-" << std::setw(3) << std::setfill('0') << contract;
    return output.str();
}

inline void addInteropContract(TestRegistry& registry, std::size_t contract, std::string layer,
    const std::function<void(TestContext&, std::size_t)>& function,
    std::vector<std::size_t> defectVariants = {}, std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("interop");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool defect = std::find(defectVariants.begin(), defectVariants.end(), variant) != defectVariants.end();
        if (defect) caseTags.push_back("known-product-defect");
        registry.add(TestCase{caseId(contract, variant), contractId(contract), "Host.Interop", layer,
            variant == 8 ? "NIGHTLY" : "PR", std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            defect ? TestDisposition::KnownProductDefect : TestDisposition::Required});
    }
}

inline interop::LibraryMetadata minimalMetadata(std::string module = "Test.Module")
{
    interop::LibraryMetadata metadata;
    metadata.producerVersion = "3.0.0";
    metadata.moduleName = std::move(module);
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    return metadata;
}

inline interop::LibraryMetadataType publicStruct(std::string key, std::string name)
{
    interop::LibraryMetadataType type;
    type.kind = interop::LibraryMetadataTypeKind::Struct;
    type.accessibility = 3;
    type.identityKey = std::move(key);
    type.fullyQualifiedName = std::move(name);
    type.size = 4;
    type.alignment = 4;
    return type;
}

inline std::string abiReport(const interop::LibraryMetadata& metadata)
{
    std::ostringstream output;
    interop::LibraryAbiReporter::write(metadata, output);
    return output.str();
}
}
