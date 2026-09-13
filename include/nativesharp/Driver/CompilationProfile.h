#pragma once

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

struct CompilationProfileSettings final
{
    llvm_backend::LlvmOptimizationLevel optimizationLevel =
        llvm_backend::LlvmOptimizationLevel::O0;
    llvm_backend::DebugInformationKind debugInformation =
        llvm_backend::DebugInformationKind::None;
};

std::optional<CompilationProfile> parseCompilationProfile(std::string_view name) noexcept;
std::string_view compilationProfileName(CompilationProfile profile) noexcept;
CompilationProfileSettings resolveCompilationProfile(
    std::optional<CompilationProfile> profile,
    std::optional<llvm_backend::LlvmOptimizationLevel> explicitOptimization,
    std::optional<llvm_backend::DebugInformationKind> explicitDebugInformation) noexcept;
}
