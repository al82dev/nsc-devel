#include "nativesharp/Language/Language3Surface.h"

#include <array>

namespace nativesharp::language
{
namespace
{
constexpr std::array<Language3SurfaceEntry, 16> kSurface = {{
    {Language3Feature::RecordStructs, "record-structs", Language3Disposition::Implemented,
     "productive parser, layout, metadata and code-generation vertical"},
    {Language3Feature::PrimaryConstructors, "primary-constructors", Language3Disposition::Implemented,
     "lowered to ordinary fields and constructors before name binding"},
    {Language3Feature::RawStringLiterals, "raw-string-literals", Language3Disposition::Implemented,
     "delimiter-aware UTF-8 literal with exact multiline contents"},
    {Language3Feature::AsyncAwait, "async-await", Language3Disposition::DeferredTo4,
     "requires productive state-machine lowering, cleanup, pinning, cancellation and debugger metadata"},
    {Language3Feature::AsyncStreams, "async-streams", Language3Disposition::DeferredTo4,
     "requires an async iterator ABI with backpressure, cancellation and deterministic cleanup"},
    {Language3Feature::YieldGenerators, "yield-generators", Language3Disposition::DeferredTo4,
     "requires productive generator frames, suspension ownership and deterministic cleanup"},
    {Language3Feature::DefaultInterfaceImplementations, "default-interface-implementations",
     Language3Disposition::DeferredTo4,
     "requires versioned dispatch, coherence and metadata rules for interface bodies"},
    {Language3Feature::ListPatterns, "list-patterns", Language3Disposition::DeferredTo4,
     "requires exhaustiveness and usefulness analysis integrated with borrow and move semantics"},
    {Language3Feature::StaticConstructors, "static-constructors", Language3Disposition::DeferredTo4,
     "requires thread-safe once initialization, reentrancy and failure semantics"},
    {Language3Feature::Decimal, "decimal", Language3Disposition::DeferredTo4,
     "requires a frozen value ABI plus arithmetic, overflow, conversion and formatting contracts"},
    {Language3Feature::ExtensionMethods, "extension-methods", Language3Disposition::Implemented,
     "static namespace extensions with deterministic import lookup and no receiver-type mutation"},
    {Language3Feature::TraitOperators, "trait-operators", Language3Disposition::Implemented,
     "stable lang-item witnesses with builtin precedence and intrinsic short-circuit control flow"},
    {Language3Feature::MoveOnlyDefaults, "move-only-defaults", Language3Disposition::Implemented,
     "value types require explicit Copy and named consuming reads require move(value)"},
    {Language3Feature::LongDouble, "long-double", Language3Disposition::RejectedFor3,
     "Windows x64 MSVC ABI aliases long double to double; use double in NativeSharp 3.0"},
    {Language3Feature::ConditionDeclarations, "condition-declarations", Language3Disposition::DeferredTo4,
     "requires scope, definite-assignment and cleanup rules across true and false edges"},
    {Language3Feature::StaticLocals, "static-locals", Language3Disposition::DeferredTo4,
     "requires thread-safe once initialization, reentrancy, destruction and module-lifetime rules"}
}};

std::string_view dispositionName(Language3Disposition disposition) noexcept
{
    switch (disposition)
    {
        case Language3Disposition::Implemented: return "implemented";
        case Language3Disposition::DeferredTo4: return "deferred-4.0";
        case Language3Disposition::RejectedFor3: return "rejected-3.0";
    }
    return "unknown";
}
}

std::span<const Language3SurfaceEntry> language3Surface() noexcept
{
    return kSurface;
}

const Language3SurfaceEntry& language3SurfaceEntry(Language3Feature feature) noexcept
{
    for (const Language3SurfaceEntry& entry : kSurface)
    {
        if (entry.feature == feature) return entry;
    }
    return kSurface.front();
}

std::string language3SurfaceManifest()
{
    std::string manifest = "NSLANGSURFACE 3.0\n";
    manifest += "target=x86_64-pc-windows-msvc\n";
    manifest += "policy=implemented-or-explicitly-deferred-no-placeholder-syntax\n";
    for (const Language3SurfaceEntry& entry : kSurface)
    {
        manifest += std::string(entry.name) + "=" + std::string(dispositionName(entry.disposition)) +
            ":" + std::string(entry.rationale) + "\n";
    }
    return manifest;
}

std::string language3SurfaceDiagnostic(Language3Feature feature)
{
    const Language3SurfaceEntry& entry = language3SurfaceEntry(feature);
    std::string result(entry.name);
    if (entry.disposition == Language3Disposition::DeferredTo4)
        result += " is not part of the frozen NativeSharp 3.0 surface; deferred to 4.0 because ";
    else if (entry.disposition == Language3Disposition::RejectedFor3)
        result += " is not supported by NativeSharp 3.0 because ";
    else
        result += " is implemented in NativeSharp 3.0: ";
    result += entry.rationale;
    return result;
}
}
