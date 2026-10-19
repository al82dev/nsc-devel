#pragma once

#include "nativesharp/Language/Language3Disposition.h"
#include "nativesharp/Language/Language3Feature.h"
#include "nativesharp/Language/Language3SurfaceEntry.h"
#include "nativesharp/Language/StaticInitializationState.h"

#include <span>
#include <string>
#include <string_view>

namespace nativesharp::language
{
constexpr std::string_view staticInitializationStateName(
    StaticInitializationState state) noexcept
{
    switch (state)
    {
        case StaticInitializationState::Uninitialized: return "uninitialized";
        case StaticInitializationState::Initializing: return "initializing";
        case StaticInitializationState::Initialized: return "initialized";
        case StaticInitializationState::Failed: return "failed";
    }
    return "unknown";
}

constexpr bool staticInitializationStateIsTerminal(
    StaticInitializationState state) noexcept
{
    return state == StaticInitializationState::Initialized ||
        state == StaticInitializationState::Failed;
}

constexpr bool staticInitializationStateMayRetry(
    StaticInitializationState state) noexcept
{
    return state == StaticInitializationState::Uninitialized;
}

std::span<const Language3SurfaceEntry> language3Surface() noexcept;
const Language3SurfaceEntry& language3SurfaceEntry(Language3Feature feature) noexcept;
std::string language3SurfaceManifest();
std::string language3SurfaceDiagnostic(Language3Feature feature);
}
