#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/RuntimeCapabilityReport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::runtime_support
{
using namespace nativesharp;

inline std::string caseId(std::size_t contract, std::size_t variant)
{
    std::ostringstream output;
    output << "TF-RUNTIME-" << std::setw(3) << std::setfill('0') << ((contract - 1) * 8 + variant);
    return output.str();
}

inline std::string contractId(std::size_t contract)
{
    std::ostringstream output;
    output << "CF-RUNTIME-" << std::setw(3) << std::setfill('0') << contract;
    return output.str();
}

inline void addRuntimeContract(TestRegistry& registry, std::size_t contract, std::string layer,
    const std::function<void(TestContext&, std::size_t)>& function,
    std::vector<std::size_t> defectVariants = {}, std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("runtime");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool defect = std::find(defectVariants.begin(), defectVariants.end(), variant) !=
            defectVariants.end();
        if (defect) caseTags.push_back("known-product-defect");
        registry.add(TestCase{caseId(contract, variant), contractId(contract), "Host.Runtime", layer,
            variant == 8 ? "NIGHTLY" : "PR", std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            defect ? TestDisposition::KnownProductDefect : TestDisposition::Required});
    }
}

inline std::uint64_t independentFnv1a(std::string_view value)
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t result = offsetBasis;
    for (const char byte : value)
    {
        result ^= static_cast<unsigned char>(byte);
        result *= prime;
    }
    return result;
}

inline std::string independentHex(std::uint64_t value)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

inline std::string functionDefinition(const runtime::RuntimeAbiSymbol& symbol)
{
    const std::string_view type = symbol.llvmType;
    const std::size_t open = type.find('(');
    const std::size_t close = type.rfind(')');
    std::string result = "define " + std::string(type.substr(0, open)) + " @" +
        std::string(symbol.name) + "(";
    std::string_view parameters = type.substr(open + 1, close - open - 1);
    std::size_t parameterIndex = 0;
    while (!parameters.empty())
    {
        const std::size_t comma = parameters.find(',');
        if (parameterIndex != 0) result += ", ";
        result += parameters.substr(0, comma);
        result += " %p" + std::to_string(parameterIndex++);
        if (comma == std::string_view::npos) break;
        parameters.remove_prefix(comma + 1);
    }
    result += ") {}\n";
    return result;
}

inline std::string imageIr(runtime::RuntimeAbiImageKind imageKind, bool reverse = false)
{
    std::vector<const runtime::RuntimeAbiSymbol*> selected;
    for (const runtime::RuntimeAbiSymbol& symbol : runtime::kRuntimeAbiSymbols)
    {
        const bool entry = symbol.name == "__nativesharp_start";
        const bool required = imageKind == runtime::RuntimeAbiImageKind::Executable ||
            (imageKind == runtime::RuntimeAbiImageKind::SupportLibrary && !entry) ||
            (imageKind == runtime::RuntimeAbiImageKind::EntryPoint && entry);
        if (required) selected.push_back(&symbol);
    }
    if (reverse) std::reverse(selected.begin(), selected.end());
    std::string result;
    for (const runtime::RuntimeAbiSymbol* symbol : selected)
    {
        if (symbol->kind == runtime::RuntimeAbiSymbolKind::Data)
            result += "@" + std::string(symbol->name) + " = global " +
                std::string(symbol->llvmType) + " 0\n";
        else
            result += functionDefinition(*symbol);
    }
    return result;
}

inline std::filesystem::path casePath(std::size_t contract, std::size_t variant)
{
    return std::filesystem::temp_directory_path() /
        ("nsc-host-runtime-" + caseId(contract, variant));
}
}
