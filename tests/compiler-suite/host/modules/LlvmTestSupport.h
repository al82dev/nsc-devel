#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::llvm_support
{
using namespace nativesharp;

inline std::string caseId(std::size_t contract, std::size_t variant)
{
    std::ostringstream output;
    output << "TF-LLVM-" << std::setw(3) << std::setfill('0') << ((contract - 1) * 8 + variant);
    return output.str();
}

inline std::string contractId(std::size_t contract)
{
    std::ostringstream output;
    output << "CF-LLVM-" << std::setw(3) << std::setfill('0') << contract;
    return output.str();
}

inline void addLlvmContract(TestRegistry& registry, std::size_t contract, std::string layer,
    const std::function<void(TestContext&, std::size_t)>& function,
    std::vector<std::size_t> excludedVariants = {}, std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        if (std::find(excludedVariants.begin(), excludedVariants.end(), variant) !=
            excludedVariants.end())
        {
            continue;
        }
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("llvm");
        caseTags.push_back("variant-" + std::to_string(variant));
        registry.add(TestCase{caseId(contract, variant), contractId(contract), "Host.LLVM", layer,
            variant == 8 ? "NIGHTLY" : "PR", std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            TestDisposition::Required});
    }
}
}
