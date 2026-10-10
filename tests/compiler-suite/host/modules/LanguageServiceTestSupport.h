#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/LanguageService/LanguageService.h"

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

namespace nsc::suite::language_service_support
{
using ContractFunction = std::function<void(TestContext&, std::size_t)>;

inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1U) * 8U + variant;
    std::ostringstream stream;
    stream << "TF-LANGUAGE-SERVICE-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-LANGUAGE-SERVICE-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline void addLanguageServiceContract(
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
        caseTags.push_back("language-service");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool known = std::ranges::find(knownVariants, variant) != knownVariants.end();
        if (known) caseTags.push_back("known-product-defect");
        registry.add(TestCase{
            caseId(contractOrdinal, variant),
            contractId(contractOrdinal),
            "Host.LanguageService",
            layer,
            variant == 8 ? "NIGHTLY" : "PR",
            std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            known ? TestDisposition::KnownProductDefect : TestDisposition::Required});
    }
}

inline void addInfrastructureContract(
    TestRegistry& registry,
    std::size_t contractOrdinal,
    std::string layer,
    std::string reason)
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        registry.add(TestCase{
            caseId(contractOrdinal, variant),
            contractId(contractOrdinal),
            "Host.LanguageService",
            layer,
            variant == 8 ? "NIGHTLY" : "PR",
            {"language-service", "infrastructure-pending", "variant-" + std::to_string(variant)},
            [reason](TestContext& context) { context.expect(false, reason); },
            TestDisposition::InfrastructurePending});
    }
}

class TemporaryTree final
{
public:
    explicit TemporaryTree(std::string_view purpose)
    {
        const std::size_t ordinal = nextOrdinal_.fetch_add(1U, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("nsc_language_service_suite_" + std::string(purpose) + "_" + std::to_string(ordinal));
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
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path;
    }

private:
    inline static std::atomic_size_t nextOrdinal_{0U};
    std::filesystem::path root_;
};

class SemanticFixture final
{
public:
    explicit SemanticFixture(std::size_t jobs = 1U)
        : tree_("semantic")
    {
        project_ = tree_.write(
            "Stage27App.nsproj",
            "<Project>\n"
            "  <PropertyGroup><Name>Stage27App</Name></PropertyGroup>\n"
            "  <ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>\n"
            "</Project>\n");
        declaration_ = tree_.write(
            "A_StateAndOverloads.ns",
            "namespace Demo;\n\n"
            "public partial class Counter\n"
            "{\n"
            "    int Value;\n\n"
            "    public int Add(int delta)\n"
            "    {\n"
            "        return this.Value + delta;\n"
            "    }\n\n"
            "    public float Add(float delta)\n"
            "    {\n"
            "        return 0.0f;\n"
            "    }\n"
            "}\n");
        use_ = tree_.write(
            "B_Run.ns",
            "namespace Demo;\n\n"
            "public partial class Counter\n"
            "{\n"
            "    public int Run(int Value)\n"
            "    {\n"
            "        int total = Add(Value);\n"
            "        int answer = Add(total);\n"
            "        return answer;\n"
            "    }\n"
            "}\n");
        snapshot_ = service_.analyzeProject(project_, jobs);
    }

    nativesharp::language_service::LanguageService& service() noexcept { return service_; }
    const std::filesystem::path& root() const noexcept { return tree_.root(); }
    const std::filesystem::path& project() const noexcept { return project_; }
    const std::filesystem::path& declaration() const noexcept { return declaration_; }
    const std::filesystem::path& use() const noexcept { return use_; }
    const nativesharp::language_service::AnalysisSnapshot& snapshot() const noexcept { return snapshot_; }

private:
    TemporaryTree tree_;
    nativesharp::language_service::LanguageService service_;
    std::filesystem::path project_;
    std::filesystem::path declaration_;
    std::filesystem::path use_;
    nativesharp::language_service::AnalysisSnapshot snapshot_;
};
}
