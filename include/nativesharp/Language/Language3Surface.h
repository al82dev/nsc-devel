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

// EN: Freeze 3.0 C-06: the deliberate rejection of a list pattern in a `case` label.
// ES: Congelamiento 3.0 C-06: el rechazo deliberado de un list pattern en una etiqueta `case`.
inline constexpr std::string_view kListPatternCaseLabelMessage =
    "list patterns are supported in 'match' and 'is', not in 'case' labels; "
    "use a match expression over the value";

std::span<const Language3SurfaceEntry> language3Surface() noexcept;
const Language3SurfaceEntry& language3SurfaceEntry(Language3Feature feature) noexcept;
std::string language3SurfaceManifest();
std::string language3SurfaceDiagnostic(Language3Feature feature);
}
