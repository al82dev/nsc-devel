#pragma once

#include <span>
#include <string>
#include <string_view>

namespace nativesharp::language
{
enum class Language3Feature
{
    RecordStructs,
    PrimaryConstructors,
    RawStringLiterals,
    AsyncAwait,
    AsyncStreams,
    YieldGenerators,
    DefaultInterfaceImplementations,
    ListPatterns,
    StaticConstructors,
    Decimal,
    ExtensionMethods,
    TraitOperators,
    MoveOnlyDefaults,
    LongDouble,
    ConditionDeclarations,
    StaticLocals
};

enum class Language3Disposition
{
    Implemented,
    PlannedFor304,
    DeferredTo4,
    RejectedFor3
};

enum class StaticInitializationState
{
    Uninitialized,
    Initializing,
    Initialized,
    Failed
};

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

struct Language3SurfaceEntry final
{
    Language3Feature feature;
    std::string_view name;
    Language3Disposition disposition;
    std::string_view rationale;
};

std::span<const Language3SurfaceEntry> language3Surface() noexcept;
const Language3SurfaceEntry& language3SurfaceEntry(Language3Feature feature) noexcept;
std::string language3SurfaceManifest();
std::string language3SurfaceDiagnostic(Language3Feature feature);
}
