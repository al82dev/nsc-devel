#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectDumper.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectGraphDumper.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Compilation/ProjectSourceSet.h"
#include "nativesharp/Compilation/SdkManifest.h"
#include "nativesharp/Compilation/SimpleXmlParser.h"
#include "nativesharp/Compilation/TypeIdentity.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::compilation_support
{
using namespace nativesharp::compilation;

using ContractFunction = std::function<void(TestContext&, std::size_t)>;

inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1U) * 8U + variant;
    std::ostringstream stream;
    stream << "TF-COMPILATION-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-COMPILATION-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline void addCompilationContract(
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
        caseTags.push_back("compilation");
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
            "Host.Compilation",
            layer,
            variant == 8 ? "NIGHTLY" : "PR",
            std::move(caseTags),
            [function, variant](TestContext& context)
            {
                function(context, variant);
            },
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
            ("nsc_compilation_suite_" + std::string(purpose) + "_" + std::to_string(ordinal));
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

inline TypeIdentity sampleIdentity(
    std::string module = "Game",
    std::string name = "Player",
    std::uint32_t arity = 0U)
{
    return TypeIdentity(std::move(module), {"Game", "Entities"}, {}, std::move(name), arity);
}

inline TypePartDescriptor samplePart(std::size_t ordinal, bool partial = true)
{
    return TypePartDescriptor{
        sampleIdentity(),
        TypeDeclarationKind::Class,
        DeclaredAccessibility::Unspecified,
        partial,
        false,
        false,
        false,
        std::nullopt,
        ordinal,
        static_cast<std::uint32_t>(10U + ordinal),
        "Player" + std::to_string(ordinal) + ".ns"};
}

inline ProjectSourceSet emptySourceSet(const std::filesystem::path& directory)
{
    ProjectSourceSet sources(directory);
    std::string error;
    sources.finalize(error, true);
    return sources;
}

inline Project makeProject(
    const std::filesystem::path& projectFile,
    std::string module,
    std::vector<std::filesystem::path> projectReferences = {},
    std::vector<LibraryReference> libraryReferences = {},
    std::vector<SdkManifest> sdkReferences = {},
    ProjectOutputKind output = ProjectOutputKind::Executable,
    std::string package = {})
{
    return Project(
        projectFile,
        std::move(module),
        output,
        emptySourceSet(projectFile.parent_path()),
        std::move(projectReferences),
        std::move(libraryReferences),
        std::move(sdkReferences),
        nativesharp::language::PanicProfile::Abort,
        {},
        std::move(package));
}

inline std::optional<Project> loadProject(
    const std::filesystem::path& path,
    std::string& error,
    const ProjectLoadOptions& options = {})
{
    return ProjectLoader{}.load(path, options, error);
}

inline bool contains(std::string_view text, std::string_view fragment)
{
    return text.find(fragment) != std::string_view::npos;
}

inline std::string projectDocument(
    std::string_view properties = {},
    std::string_view items = {})
{
    return "<Project><PropertyGroup>" + std::string(properties) +
        "</PropertyGroup><ItemGroup>" + std::string(items) + "</ItemGroup></Project>";
}

inline std::string sdkDocument(std::string_view body = {}, std::string_view attributes = {})
{
    return "<NativeSharpSdk Name=\"Windows\" FormatVersion=\"1\" " +
        std::string(attributes) + ">" + std::string(body) + "</NativeSharpSdk>";
}

inline bool equalProjectOrder(const ProjectGraph& graph, const std::vector<std::string>& modules)
{
    if (graph.projects().size() != modules.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < modules.size(); ++index)
    {
        if (graph.projects()[index].moduleName() != modules[index])
        {
            return false;
        }
    }
    return true;
}
}
