#pragma once

#include "nativesharp/Language/LanguageDisposition.h"
#include "nativesharp/Language/LanguageFeature.h"
#include "nativesharp/Language/LanguageSurfaceEntry.h"
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

// EN: Freeze C-06: the deliberate rejection of a list pattern in a `case` label.
// ES: Congelamiento C-06: el rechazo deliberado de un list pattern en una etiqueta `case`.
inline constexpr std::string_view kListPatternCaseLabelMessage =
    "list patterns are supported in 'match' and 'is', not in 'case' labels; "
    "use a match expression over the value";

std::span<const LanguageSurfaceEntry> languageSurface() noexcept;
const LanguageSurfaceEntry& languageSurfaceEntry(LanguageFeature feature) noexcept;
std::string languageSurfaceManifest();
std::string languageSurfaceDiagnostic(LanguageFeature feature);
}
