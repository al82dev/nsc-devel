#include "nativesharp/Language/LanguageSurface.h"

#include <array>

namespace nativesharp::language
{
namespace
{
constexpr std::array<LanguageSurfaceEntry, 16> kSurface = {{
    {LanguageFeature::RecordStructs, "record-structs", LanguageDisposition::Implemented,
     "productive parser, layout, metadata and code-generation vertical"},
    {LanguageFeature::PrimaryConstructors, "primary-constructors", LanguageDisposition::Implemented,
     "lowered to ordinary fields and constructors before name binding"},
    {LanguageFeature::RawStringLiterals, "raw-string-literals", LanguageDisposition::Implemented,
     "delimiter-aware UTF-8 literal with exact multiline contents"},
    // EN: These five entries were published as planned while their
    //     productive lowering already shipped; each disposition below was confirmed by compiling
    //     and running a program that uses the feature.
    // ES: Estas cinco entradas se publicaban como planeadas cuando su
    //     lowering productivo ya existia; cada disposicion se confirmo compilando y ejecutando un
    //     programa que usa la capacidad.
    {LanguageFeature::AsyncAwait, "async-await", LanguageDisposition::Implemented,
     "async callables lower to a future constructor and a poll state machine; await suspends, resumes and propagates errors"},
    {LanguageFeature::AsyncStreams, "async-streams", LanguageDisposition::Implemented,
     "AsyncIterator<T, E> with MoveNextAsync, cooperative cancellation and deterministic cleanup of active finally regions"},
    {LanguageFeature::YieldGenerators, "yield-generators", LanguageDisposition::Implemented,
     "Iterator<T> generator frames with MoveNext, yield break and early-stop destruction of live locals"},
    {LanguageFeature::DefaultInterfaceImplementations, "default-interface-implementations",
     LanguageDisposition::Implemented,
     "interface and trait method bodies dispatched through the interface map, most-derived default wins"},
    {LanguageFeature::ListPatterns, "list-patterns", LanguageDisposition::Implemented,
     "list patterns in match and is with length-domain exhaustiveness and redundancy analysis; not in case labels"},
    {LanguageFeature::StaticConstructors, "static-constructors", LanguageDisposition::Implemented,
     "thread-safe once initialization with reentrancy and terminal failure"},
    {LanguageFeature::Decimal, "decimal", LanguageDisposition::Implemented,
     "frozen 128-bit value ABI (coefficient 96 + scale + sign) with arithmetic, overflow, conversion, formatting and FFI contracts"},
    {LanguageFeature::ExtensionMethods, "extension-methods", LanguageDisposition::Implemented,
     "static namespace extensions with deterministic import lookup and no receiver-type mutation"},
    {LanguageFeature::TraitOperators, "trait-operators", LanguageDisposition::Implemented,
     "stable lang-item witnesses with builtin precedence and intrinsic short-circuit control flow"},
    // EN: Freeze C-06: the published rule said every value type required an explicit Copy;
    //     the implemented rule (ProjectMemberModel::typeIsImplicitlyCopyable) copies a struct
    //     whose fields are all copyable and makes a struct with an owned field, a variant or an
    //     opaque type move-only. The manifest now publishes the implemented rule.
    // ES: Congelamiento C-06: la regla publicada decia que todo tipo valor exigia Copy
    //     explicito; la implementada (ProjectMemberModel::typeIsImplicitlyCopyable) copia un
    //     struct cuyos campos son todos copiables y hace move-only a un struct con un campo owned,
    //     a un variant o a un tipo opaco. El manifiesto publica ahora la regla implementada.
    // EN: Freeze DEC-07/DEC-08: C-11 made a struct with a destructor move-only and C-14 made a
    //     future move-only with `await` consuming the local; both are kept, so the rule names them.
    //     ProjectMemberModel::typeIsImplicitlyCopyable rejects a destructor, an owned field and a
    //     move-only-value marker without `copy`; ProjectMemberModel::typeIsMoveOnly adds `future:`.
    // ES: Congelamiento DEC-07/DEC-08: C-11 hizo move-only a un struct con destructor y C-14 a un
    //     future, con `await` consumiendo el local; ambos se conservan, asi que la regla los nombra.
    //     ProjectMemberModel::typeIsImplicitlyCopyable rechaza un destructor, un campo owned y una
    //     marca move-only-value sin `copy`; ProjectMemberModel::typeIsMoveOnly agrega `future:`.
    {LanguageFeature::MoveOnlyDefaults, "move-only-defaults", LanguageDisposition::Implemented,
     "structs without a destructor whose fields are all copyable copy implicitly; structs with a "
     "destructor or owned fields, variants and opaque types without the copy modifier, and futures "
     "are move-only; named consuming reads require move(value) and await consumes its future"},
    {LanguageFeature::LongDouble, "long-double", LanguageDisposition::Rejected,
     "Windows x64 MSVC ABI aliases long double to double; use double in NativeSharp"},
    {LanguageFeature::ConditionDeclarations, "condition-declarations", LanguageDisposition::Implemented,
     "edge-scoped if, while and switch locals with definite assignment and cleanup"},
    {LanguageFeature::StaticLocals, "static-locals", LanguageDisposition::Implemented,
     "module storage with thread-safe once initialization and unload destruction"}
}};

std::string_view dispositionName(LanguageDisposition disposition) noexcept
{
    switch (disposition)
    {
        case LanguageDisposition::Implemented: return "implemented";
        case LanguageDisposition::Planned: return "planned";
        case LanguageDisposition::Deferred: return "deferred";
        case LanguageDisposition::Rejected: return "rejected";
    }
    return "unknown";
}
}

std::span<const LanguageSurfaceEntry> languageSurface() noexcept
{
    return kSurface;
}

const LanguageSurfaceEntry& languageSurfaceEntry(LanguageFeature feature) noexcept
{
    for (const LanguageSurfaceEntry& entry : kSurface)
    {
        if (entry.feature == feature) return entry;
    }
    return kSurface.front();
}

std::string languageSurfaceManifest()
{
    std::string manifest = "NSLANGSURFACE\n";
    manifest += "target=x86_64-pc-windows-msvc\n";
    manifest += "policy=implemented-planned-or-explicitly-deferred-no-placeholder-syntax\n";
    for (const LanguageSurfaceEntry& entry : kSurface)
    {
        manifest += std::string(entry.name) + "=" + std::string(dispositionName(entry.disposition)) +
            ":" + std::string(entry.rationale) + "\n";
    }
    return manifest;
}

std::string languageSurfaceDiagnostic(LanguageFeature feature)
{
    const LanguageSurfaceEntry& entry = languageSurfaceEntry(feature);
    std::string result(entry.name);
    if (entry.disposition == LanguageDisposition::Planned)
        result += " is planned but is not implemented because ";
    else if (entry.disposition == LanguageDisposition::Deferred)
        result += " is deferred because ";
    else if (entry.disposition == LanguageDisposition::Rejected)
        result += " is not supported by NativeSharp because ";
    else
        result += " is implemented in NativeSharp: ";
    result += entry.rationale;
    return result;
}
}
