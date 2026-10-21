#include "MemberCompletionAnalysis.h"

#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace nativesharp::language_service
{
namespace
{
std::string overlayPathKey(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    std::string result = absolute.lexically_normal().generic_string();
    std::ranges::transform(result, result.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return result;
}
}

std::shared_ptr<frontend::ProjectCompilation> analyzeMemberCompletionOverlay(
    const std::filesystem::path& projectPath,
    std::unordered_map<std::string, std::string> overlays,
    std::size_t jobs,
    std::function<bool()> cancellationCheck,
    std::string& error)
{
    const std::size_t effectiveJobs = std::max<std::size_t>(jobs, 1);
    const compilation::ProjectLoadOptions loadOptions;
    compilation::ProjectLoader projectLoader;
    compilation::ProjectGraphLoader graphLoader(
        effectiveJobs,
        [&projectLoader, loadOptions](const std::filesystem::path& path, std::string& loadError) {
            return projectLoader.load(path, loadOptions, loadError);
        });
    std::optional<compilation::ProjectGraph> graph = graphLoader.load(projectPath, error);
    if (!graph.has_value()) return {};

    auto compilation = std::make_shared<frontend::ProjectCompilation>();
    compilation->setCompilationJobs(effectiveJobs);
    compilation->setProjectLoadOptions(loadOptions);
    compilation->setCancellationCheck(cancellationCheck);
    compilation->setSourceBufferProvider(
        [overlays = std::move(overlays)](const std::filesystem::path& path)
            -> std::optional<std::string> {
            const auto found = overlays.find(overlayPathKey(path));
            return found != overlays.end() ? std::optional<std::string>(found->second)
                                           : std::nullopt;
        });
    if (!compilation::preparePrecompiledProject(
            std::move(*graph), loadOptions, *compilation, error)) return {};
    compilation->compileProjectMethodBodies(projectPath, error);
    return compilation;
}
}
