#pragma once

#include "nsc_suite/TestRegistry.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::driver_support
{
using ContractFunction = std::function<void(TestContext&, std::size_t)>;

inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1U) * 8U + variant;
    std::ostringstream stream;
    stream << "TF-DRIVER-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-DRIVER-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline void addDriverContract(
    TestRegistry& registry,
    std::size_t contractOrdinal,
    std::string layer,
    const ContractFunction& function,
    std::vector<std::size_t> knownVariants = {},
    std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("driver");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool known = std::find(knownVariants.begin(), knownVariants.end(), variant) !=
            knownVariants.end();
        if (known)
        {
            caseTags.push_back("known-product-defect");
        }
        registry.add(TestCase{
            caseId(contractOrdinal, variant),
            contractId(contractOrdinal),
            "Host.Driver",
            layer,
            variant == 8 ? "NIGHTLY" : "PR",
            std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            known ? TestDisposition::KnownProductDefect : TestDisposition::Required});
    }
}

class TemporaryTree final
{
public:
    explicit TemporaryTree(std::string_view purpose)
    {
        const std::size_t ordinal = nextOrdinal_.fetch_add(1U, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("nsc_driver_suite_" + std::string(purpose) + "_" + std::to_string(ordinal));
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_, error);
    }

    ~TemporaryTree()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TemporaryTree(const TemporaryTree&) = delete;
    TemporaryTree& operator=(const TemporaryTree&) = delete;

    const std::filesystem::path& root() const noexcept { return root_; }

    std::filesystem::path write(std::filesystem::path relative, std::string_view contents) const
    {
        const std::filesystem::path path = root_ / std::move(relative);
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path;
    }

private:
    inline static std::atomic_size_t nextOrdinal_{0U};
    std::filesystem::path root_;
};

inline std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

inline bool contains(std::string_view text, std::string_view fragment)
{
    return text.find(fragment) != std::string_view::npos;
}
}
