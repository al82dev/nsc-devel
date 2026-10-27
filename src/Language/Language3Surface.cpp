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
    // EN: Freeze 3.0 C-06: these five entries were published as planned-3.0.4 while their
    //     productive lowering already shipped; each disposition below was confirmed by compiling
    //     and running a program that uses the feature.
    // ES: Congelamiento 3.0 C-06: estas cinco entradas se publicaban como planned-3.0.4 cuando su
    //     lowering productivo ya existia; cada disposicion se confirmo compilando y ejecutando un
    //     programa que usa la capacidad.
    {Language3Feature::AsyncAwait, "async-await", Language3Disposition::Implemented,
     "async callables lower to a future constructor and a poll state machine; await suspends, resumes and propagates errors"},
    {Language3Feature::AsyncStreams, "async-streams", Language3Disposition::Implemented,
     "AsyncIterator<T, E> with MoveNextAsync, cooperative cancellation and deterministic cleanup of active finally regions"},
    {Language3Feature::YieldGenerators, "yield-generators", Language3Disposition::Implemented,
     "Iterator<T> generator frames with MoveNext, yield break and early-stop destruction of live locals"},
    {Language3Feature::DefaultInterfaceImplementations, "default-interface-implementations",
     Language3Disposition::Implemented,
     "interface and trait method bodies dispatched through the interface map, most-derived default wins"},
    {Language3Feature::ListPatterns, "list-patterns", Language3Disposition::Implemented,
     "list patterns in match and is with length-domain exhaustiveness and redundancy analysis; not in case labels"},
    {Language3Feature::StaticConstructors, "static-constructors", Language3Disposition::Implemented,
     "thread-safe once initialization with reentrancy and terminal failure"},
    {Language3Feature::Decimal, "decimal", Language3Disposition::Implemented,
     "frozen 128-bit value ABI (coefficient 96 + scale + sign) with arithmetic, overflow, conversion, formatting and FFI contracts"},
    {Language3Feature::ExtensionMethods, "extension-methods", Language3Disposition::Implemented,
     "static namespace extensions with deterministic import lookup and no receiver-type mutation"},
    {Language3Feature::TraitOperators, "trait-operators", Language3Disposition::Implemented,
     "stable lang-item witnesses with builtin precedence and intrinsic short-circuit control flow"},
    // EN: Freeze 3.0 C-06: the published rule said every value type required an explicit Copy;
    //     the implemented rule (ProjectMemberModel::typeIsImplicitlyCopyable) copies a struct
    //     whose fields are all copyable and makes a struct with an owned field, a variant or an
    //     opaque type move-only. The manifest now publishes the implemented rule.
    // ES: Congelamiento 3.0 C-06: la regla publicada decia que todo tipo valor exigia Copy
    //     explicito; la implementada (ProjectMemberModel::typeIsImplicitlyCopyable) copia un
    //     struct cuyos campos son todos copiables y hace move-only a un struct con un campo owned,
    //     a un variant o a un tipo opaco. El manifiesto publica ahora la regla implementada.
    // EN: Freeze 3.0 DEC-07/DEC-08: C-11 made a struct with a destructor move-only and C-14 made a
    //     future move-only with `await` consuming the local; both are kept, so the rule names them.
    //     ProjectMemberModel::typeIsImplicitlyCopyable rejects a destructor, an owned field and a
    //     move-only-value marker without `copy`; ProjectMemberModel::typeIsMoveOnly adds `future:`.
    // ES: Congelamiento 3.0 DEC-07/DEC-08: C-11 hizo move-only a un struct con destructor y C-14 a un
    //     future, con `await` consumiendo el local; ambos se conservan, asi que la regla los nombra.
    //     ProjectMemberModel::typeIsImplicitlyCopyable rechaza un destructor, un campo owned y una
    //     marca move-only-value sin `copy`; ProjectMemberModel::typeIsMoveOnly agrega `future:`.
    {Language3Feature::MoveOnlyDefaults, "move-only-defaults", Language3Disposition::Implemented,
     "structs without a destructor whose fields are all copyable copy implicitly; structs with a "
     "destructor or owned fields, variants and opaque types without the copy modifier, and futures "
     "are move-only; named consuming reads require move(value) and await consumes its future"},
    {Language3Feature::LongDouble, "long-double", Language3Disposition::RejectedFor3,
     "Windows x64 MSVC ABI aliases long double to double; use double in NativeSharp 3.0"},
    {Language3Feature::ConditionDeclarations, "condition-declarations", Language3Disposition::Implemented,
     "edge-scoped if, while and switch locals with definite assignment and cleanup"},
    {Language3Feature::StaticLocals, "static-locals", Language3Disposition::Implemented,
     "module storage with thread-safe once initialization and unload destruction"}
}};

std::string_view dispositionName(Language3Disposition disposition) noexcept
{
    switch (disposition)
    {
        case Language3Disposition::Implemented: return "implemented";
        case Language3Disposition::PlannedFor304: return "planned-3.0.4";
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
    manifest += "policy=implemented-planned-or-explicitly-deferred-no-placeholder-syntax\n";
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
    if (entry.disposition == Language3Disposition::PlannedFor304)
        result += " is planned for NativeSharp 3.0.4 but is not implemented because ";
    else if (entry.disposition == Language3Disposition::DeferredTo4)
        result += " is not part of the frozen NativeSharp 3.0 surface; deferred to 4.0 because ";
    else if (entry.disposition == Language3Disposition::RejectedFor3)
        result += " is not supported by NativeSharp 3.0 because ";
    else
        result += " is implemented in NativeSharp 3.0: ";
    result += entry.rationale;
    return result;
}
}
