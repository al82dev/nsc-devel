#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::language_service
{
std::shared_ptr<frontend::ProjectCompilation> analyzeMemberCompletionOverlay(
    const std::filesystem::path& projectPath,
    std::unordered_map<std::string, std::string> overlays,
    std::size_t jobs,
    std::function<bool()> cancellationCheck,
    std::string& error);
}
