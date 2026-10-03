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
    LongDouble,
    ConditionDeclarations,
    StaticLocals
};

enum class Language3Disposition
{
    Implemented,
    DeferredTo4,
    RejectedFor3
};

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
