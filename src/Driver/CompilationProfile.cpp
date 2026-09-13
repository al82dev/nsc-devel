#include "nativesharp/Driver/CompilationProfile.h"

namespace nativesharp::driver
{
std::optional<CompilationProfile> parseCompilationProfile(std::string_view name) noexcept
{
    if (name == "debug") return CompilationProfile::Debug;
    if (name == "release") return CompilationProfile::Release;
    return std::nullopt;
}

std::string_view compilationProfileName(CompilationProfile profile) noexcept
{
    switch (profile)
    {
        case CompilationProfile::Debug:
            return "debug";
        case CompilationProfile::Release:
            return "release";
    }
    return {};
}

CompilationProfileSettings resolveCompilationProfile(
    std::optional<CompilationProfile> profile,
    std::optional<llvm_backend::LlvmOptimizationLevel> explicitOptimization,
    std::optional<llvm_backend::DebugInformationKind> explicitDebugInformation) noexcept
{
    CompilationProfileSettings resolved;
    if (profile == CompilationProfile::Debug)
    {
        resolved.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0;
        resolved.debugInformation = llvm_backend::DebugInformationKind::Full;
    }
    else if (profile == CompilationProfile::Release)
    {
        resolved.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O2;
        resolved.debugInformation = llvm_backend::DebugInformationKind::LineTablesOnly;
    }

    if (explicitOptimization.has_value())
        resolved.optimizationLevel = *explicitOptimization;
    if (explicitDebugInformation.has_value())
        resolved.debugInformation = *explicitDebugInformation;
    return resolved;
}
}
