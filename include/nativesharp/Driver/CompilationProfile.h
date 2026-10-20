#pragma once

#include "nativesharp/Driver/CompilationProfileSettings.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <optional>
#include <string_view>

namespace nativesharp::driver
{
enum class CompilationProfile
{
    Debug,
    Release,
};

std::optional<CompilationProfile> parseCompilationProfile(std::string_view name) noexcept;
std::string_view compilationProfileName(CompilationProfile profile) noexcept;
CompilationProfileSettings resolveCompilationProfile(
    std::optional<CompilationProfile> profile,
    std::optional<llvm_backend::LlvmOptimizationLevel> explicitOptimization,
    std::optional<llvm_backend::DebugInformationKind> explicitDebugInformation) noexcept;
}
