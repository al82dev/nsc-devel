#include "CompilerInvocation.h"
#include "Internal/MillisecondsParseResult.h"
#include "Internal/OptionContext.h"
#include "Internal/OptionResult.h"

#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/CompilerConsole.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <utility>

namespace nativesharp::driver
{
namespace
{
using OptionHandler = OptionResult (*)(OptionContext&);

OptionResult noMatch() noexcept { return {}; }
OptionResult matched(ExitCode code = ExitCode::Success) noexcept
{
    return {true, code};
}

bool parseBoundedDecimal(
    std::string_view text, std::size_t maximum, std::size_t& value) noexcept
{
    if (text.empty()) return false;
    std::size_t parsed = 0;
    for (const unsigned char character : text)
    {
        if (std::isdigit(character) == 0) return false;
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (parsed > (maximum - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    value = parsed;
    return true;
}

MillisecondsParseResult parseMilliseconds(
    std::string_view value, std::uint64_t& milliseconds) noexcept
{
    if (value.empty() || value.size() > 8U ||
        !std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        }))
    {
        return MillisecondsParseResult::Malformed;
    }
    for (const char character : value)
        milliseconds = milliseconds * 10U + static_cast<unsigned>(character - '0');
    return milliseconds == 0 || milliseconds > 86400000U
        ? MillisecondsParseResult::OutOfRange
        : MillisecondsParseResult::Valid;
}

OptionResult handleTiming(OptionContext& context)
{
    if (context.argument != "--time") return noMatch();
    context.invocation.reportTiming = true;
    return matched();
}

OptionResult handleProgress(OptionContext& context)
{
    if (context.argument == "--progress")
    {
        context.invocation.reportProgress = true;
        return matched();
    }
    const bool enablesProgress = context.argument.starts_with("--progress=");
    if (!enablesProgress && !context.argument.starts_with("--progress-format="))
        return noMatch();
    const std::string_view prefix = enablesProgress
        ? std::string_view("--progress=")
        : std::string_view("--progress-format=");
    if (enablesProgress) context.invocation.reportProgress = true;
    const std::string_view value = context.argument.substr(prefix.size());
    if (parseProgressFormat(value, context.invocation.progressFormat)) return matched();
    context.error << "nsc: error: " << compilerText(
        "unsupported progress format '", "formato de progreso no compatible '")
                  << value << compilerText(
                       "'; expected files, text or json\n", "'; se esperaba files, text o json\n");
    return matched(ExitCode::CommandLineError);
}

#if 0 // INSTRUMENTATION CODE: optional --generics-trace parser.
OptionResult handleGenericsTrace(OptionContext& context)
{
    if (context.argument != "--generics-trace" &&
        !context.argument.starts_with("--generics-trace="))
        return noMatch();
    if (context.invocation.genericsTraceSeen)
    {
        context.error << "nsc: error: duplicate --generics-trace option\n";
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.genericsTraceSeen = true;
    std::string_view value;
    if (context.argument == "--generics-trace")
    {
        if (context.index + 1U >= context.arguments.size())
        {
            context.error << "nsc: error: --generics-trace requires a file path\n";
            return matched(ExitCode::CommandLineError);
        }
        value = context.arguments[++context.index];
    }
    else
        value = context.argument.substr(std::string_view("--generics-trace=").size());
    if (value.empty())
    {
        context.error << "nsc: error: --generics-trace requires a non-empty file path\n";
        return matched(ExitCode::CommandLineError);
    }
    return matched(openCompilerGenericsTrace(
        std::filesystem::path(value), value, false, context.error));
}
#endif // INSTRUMENTATION CODE

OptionResult handleTraceAndIncrementalLink(OptionContext& context)
{
    if (context.argument == "--llvm-pass-trace") return matched();
    if (context.argument != "--incremental-link") return noMatch();
    context.invocation.incrementalLink = true;
    return matched();
}

OptionResult handleLinkTimeOptimization(OptionContext& context)
{
    if (context.argument != "-flto" && !context.argument.starts_with("-flto="))
        return noMatch();
    if (context.invocation.linkTimeOptimizationSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate link-time optimization mode\n",
            "el modo de optimización en tiempo de enlace está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.linkTimeOptimizationSeen = true;
    if (context.argument == "-flto" || context.argument == "-flto=full")
        context.invocation.linkTimeOptimization =
            llvm_backend::LinkTimeOptimizationMode::Full;
    else if (context.argument == "-flto=thin")
        context.invocation.linkTimeOptimization =
            llvm_backend::LinkTimeOptimizationMode::Thin;
    else
    {
        context.error << "nsc: error: " << compilerText(
            "unsupported link-time optimization mode '",
            "modo de optimización en tiempo de enlace no compatible '")
                      << context.argument << compilerText(
                           "'; expected -flto, -flto=full, or -flto=thin\n",
                           "'; se esperaba -flto, -flto=full o -flto=thin\n");
        return matched(ExitCode::CommandLineError);
    }
    return matched();
}

OptionResult handleFloatingPointMode(OptionContext& context)
{
    constexpr std::string_view prefix = "--fp-mode=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.floatingPointModeSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate floating-point mode\n",
            "el modo de punto flotante está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.floatingPointModeSeen = true;
    const auto parsed = llvm_backend::parseFloatingPointMode(
        context.argument.substr(prefix.size()));
    if (parsed.has_value())
    {
        context.invocation.floatingPointMode = *parsed;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "unsupported floating-point mode '", "modo de punto flotante no compatible '")
                  << context.argument << compilerText(
                       "'; expected --fp-mode=strict, precise, fast, or deterministic\n",
                       "'; se esperaba --fp-mode=strict, precise, fast o deterministic\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleFmaContract(OptionContext& context)
{
    constexpr std::string_view prefix = "--fp-contract=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.fmaContractPolicySeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate FMA contraction policy\n",
            "la política de contracción FMA está repetida\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.fmaContractPolicySeen = true;
    const auto parsed = llvm_backend::parseFmaContractPolicy(
        context.argument.substr(prefix.size()));
    if (parsed.has_value())
    {
        context.invocation.fmaContractPolicy = *parsed;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "unsupported FMA contraction policy '",
        "política de contracción FMA no compatible '")
                  << context.argument << compilerText(
                       "'; expected --fp-contract=off, on, or fast\n",
                       "'; se esperaba --fp-contract=off, on o fast\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleFloatingPointRounding(OptionContext& context)
{
    constexpr std::string_view prefix = "--fp-rounding=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.floatingPointRoundingModeSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate floating-point rounding mode\n",
            "el modo de redondeo de punto flotante está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.floatingPointRoundingModeSeen = true;
    const auto parsed = llvm_backend::parseFloatingPointRoundingMode(
        context.argument.substr(prefix.size()));
    if (parsed.has_value())
    {
        context.invocation.floatingPointRoundingMode = *parsed;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "unsupported floating-point rounding mode '",
        "modo de redondeo de punto flotante no compatible '")
                  << context.argument << compilerText(
                       "'; expected --fp-rounding=dynamic, nearest-even, toward-zero, upward, or downward\n",
                       "'; se esperaba --fp-rounding=dynamic, nearest-even, toward-zero, upward o downward\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleFloatingPointDenormal(OptionContext& context)
{
    constexpr std::string_view prefix = "--fp-denormal=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.floatingPointDenormalModeSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate floating-point denormal mode\n",
            "el modo de desnormalizados de punto flotante está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.floatingPointDenormalModeSeen = true;
    const auto parsed = llvm_backend::parseFloatingPointDenormalMode(
        context.argument.substr(prefix.size()));
    if (parsed.has_value())
    {
        context.invocation.floatingPointDenormalMode = *parsed;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "unsupported floating-point denormal mode '",
        "modo de desnormalizados de punto flotante no compatible '")
                  << context.argument << compilerText(
                       "'; expected --fp-denormal=preserve, ftz, daz, or ftz-daz\n",
                       "'; se esperaba --fp-denormal=preserve, ftz, daz o ftz-daz\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleCompilationProfile(OptionContext& context)
{
    constexpr std::string_view prefix = "--profile=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.compilationProfileSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate compilation profile\n", "el perfil de compilación está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.compilationProfileSeen = true;
    context.invocation.compilationProfile =
        parseCompilationProfile(context.argument.substr(prefix.size()));
    if (context.invocation.compilationProfile.has_value()) return matched();
    context.error << "nsc: error: " << compilerText(
        "unsupported compilation profile '", "perfil de compilación no compatible '")
                  << context.argument << compilerText(
                       "'; expected --profile=debug or --profile=release\n",
                       "'; se esperaba --profile=debug o --profile=release\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleJobCount(OptionContext& context)
{
    if (context.argument != "-j" &&
        !(context.argument.starts_with("-j") && context.argument.size() > 2U))
        return noMatch();
    if (context.invocation.jobCountSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate compilation job count\n",
            "el número de trabajos de compilación está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.jobCountSeen = true;
    std::string_view value;
    if (context.argument == "-j")
    {
        if (context.index + 1U >= context.arguments.size())
        {
            context.error << "nsc: error: " << compilerText(
                "-j requires a job count from 1 through 1024\n",
                "-j requiere un número de trabajos entre 1 y 1024\n");
            return matched(ExitCode::CommandLineError);
        }
        value = context.arguments[++context.index];
    }
    else
        value = context.argument.substr(2);
    std::size_t jobs = 0;
    if (parseBoundedDecimal(value, kMaximumCompilationJobs, jobs) && jobs != 0)
    {
        context.invocation.requestedJobs = jobs;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "compilation job count must be between 1 and 1024\n",
        "el número de trabajos de compilación debe estar entre 1 y 1024\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleReservedCores(OptionContext& context)
{
    constexpr std::string_view prefix = "--reserve-cores=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.reservedCoresSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate reserved-core count\n",
            "el número de núcleos reservados está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.reservedCoresSeen = true;
    std::size_t reserved = 0;
    if (parseBoundedDecimal(
            context.argument.substr(prefix.size()),
            kMaximumCompilationJobs - 1U,
            reserved))
    {
        context.invocation.reservedCores = reserved;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "reserved-core count must be between 0 and 1023\n",
        "el número de núcleos reservados debe estar entre 0 y 1023\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleArtifactCachePolicy(OptionContext& context)
{
    if (context.argument == "--no-artifact-cache")
    {
        if (context.invocation.artifactCacheDisabled ||
            context.invocation.artifactCacheSeen)
        {
            context.error << "nsc: error: " << compilerText(
                "conflicting artifact-cache policy\n",
                "la política de caché de artefactos tiene opciones incompatibles\n");
            return matched(ExitCode::CommandLineError);
        }
        context.invocation.artifactCacheDisabled = true;
        return matched();
    }
    constexpr std::string_view prefix = "--artifact-cache=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.artifactCacheSeen || context.invocation.artifactCacheDisabled)
    {
        context.error << "nsc: error: " << compilerText(
            "conflicting artifact-cache policy\n",
            "la política de caché de artefactos tiene opciones incompatibles\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.artifactCacheSeen = true;
    const std::string_view value = context.argument.substr(prefix.size());
    const std::filesystem::path cachePath{std::string(value)};
    if (!value.empty() && cachePath.is_absolute())
    {
        context.invocation.artifactCacheRoot = cachePath;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "artifact cache requires an absolute directory: '",
        "la caché de artefactos requiere un directorio absoluto: '")
                  << value << "'\n";
    return matched(ExitCode::CommandLineError);
}

OptionResult handleArtifactCacheMaximum(OptionContext& context)
{
    constexpr std::string_view prefix = "--artifact-cache-max-mb=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.artifactCacheMaximumSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate artifact-cache size limit\n",
            "el límite de tamaño de la caché de artefactos está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.artifactCacheMaximumSeen = true;
    std::size_t maximumMiB = 0;
    if (parseBoundedDecimal(
            context.argument.substr(prefix.size()), 1024U * 1024U, maximumMiB))
    {
        context.invocation.artifactCacheMaximumBytes =
            static_cast<std::uintmax_t>(maximumMiB) * 1024U * 1024U;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "artifact-cache size limit must be between 0 and 1048576 MiB\n",
        "el límite de la caché debe estar entre 0 y 1048576 MiB\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleLibraryReferenceRoot(OptionContext& context)
{
    constexpr std::string_view option = "--library-reference-root";
    constexpr std::string_view prefix = "--library-reference-root=";
    if (context.argument != option && !context.argument.starts_with(prefix))
        return noMatch();
    if (context.invocation.libraryReferenceRootSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate library-reference root\n",
            "la raíz de referencias de bibliotecas está repetida\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.libraryReferenceRootSeen = true;
    std::string_view value;
    if (context.argument == option)
    {
        if (context.index + 1U >= context.arguments.size())
        {
            context.error << "nsc: error: " << compilerText(
                "--library-reference-root requires an absolute directory\n",
                "--library-reference-root requiere un directorio absoluto\n");
            return matched(ExitCode::CommandLineError);
        }
        value = context.arguments[++context.index];
    }
    else
        value = context.argument.substr(prefix.size());
    const std::filesystem::path root{std::string(value)};
    if (!value.empty() && root.is_absolute())
    {
        context.invocation.projectLoadOptions.libraryReferenceRoot =
            root.lexically_normal();
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "library-reference root requires an absolute directory: '",
        "la raíz de referencias de bibliotecas requiere un directorio absoluto: '")
                  << value << "'\n";
    return matched(ExitCode::CommandLineError);
}

OptionResult handleGenericDepth(OptionContext& context)
{
    constexpr std::string_view prefix = "--generic-depth-limit=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.genericInstantiationDepthSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate generic instantiation depth limit\n",
            "el límite de profundidad de instanciación genérica está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.genericInstantiationDepthSeen = true;
    std::size_t depth = 0;
    if (parseBoundedDecimal(
            context.argument.substr(prefix.size()),
            compilation::MaximumGenericInstantiationDepth,
            depth) && depth != 0)
    {
        context.invocation.projectLoadOptions.genericInstantiationDepth = depth;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "generic instantiation depth limit must be between 1 and 4096\n",
        "el límite de profundidad de instanciación genérica debe estar entre 1 y 4096\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleGenericCount(OptionContext& context)
{
    constexpr std::string_view prefix = "--generic-instantiation-limit=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.genericInstantiationCountSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate generic instantiation count limit\n",
            "el límite de cantidad de instanciaciones genéricas está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.genericInstantiationCountSeen = true;
    std::size_t count = 0;
    if (parseBoundedDecimal(
            context.argument.substr(prefix.size()),
            compilation::MaximumGenericInstantiationCount,
            count) && count != 0)
    {
        context.invocation.projectLoadOptions.genericInstantiationCount = count;
        return matched();
    }
    context.error << "nsc: error: " << compilerText(
        "generic instantiation count limit must be between 1 and 1048576\n",
        "el límite de cantidad de instanciaciones genéricas debe estar entre 1 y 1048576\n");
    return matched(ExitCode::CommandLineError);
}

OptionResult handleTimeout(OptionContext& context)
{
    constexpr std::string_view prefix = "--timeout-ms=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    std::uint64_t milliseconds = 0;
    const auto result = parseMilliseconds(
        context.argument.substr(prefix.size()), milliseconds);
    if (result == MillisecondsParseResult::Malformed)
    {
        context.error << "nsc: error: " << compilerText(
            "malformed timeout; expected --timeout-ms=1..86400000\n",
            "tiempo límite mal formado; se esperaba --timeout-ms=1..86400000\n");
        return matched(ExitCode::CommandLineError);
    }
    if (result == MillisecondsParseResult::OutOfRange)
    {
        context.error << "nsc: error: " << compilerText(
            "timeout must be between 1 and 86400000 milliseconds\n",
            "el tiempo límite debe estar entre 1 y 86400000 milisegundos\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.timeoutMilliseconds = milliseconds;
    return matched();
}

OptionResult handleOptimizationBudget(OptionContext& context)
{
    constexpr std::string_view prefix = "--o2-budget-ms=";
    if (!context.argument.starts_with(prefix)) return noMatch();
    if (context.invocation.optimizationBudgetSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate O2 optimization budget\n",
            "el presupuesto de optimización O2 está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.optimizationBudgetSeen = true;
    std::uint64_t milliseconds = 0;
    const auto result = parseMilliseconds(
        context.argument.substr(prefix.size()), milliseconds);
    if (result == MillisecondsParseResult::Malformed)
    {
        context.error << "nsc: error: malformed O2 budget; "
                         "expected --o2-budget-ms=1..86400000\n";
        return matched(ExitCode::CommandLineError);
    }
    if (result == MillisecondsParseResult::OutOfRange)
    {
        context.error << "nsc: error: O2 budget must be between 1 and "
                         "86400000 milliseconds\n";
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.optimizationBudgetMilliseconds = milliseconds;
    return matched();
}

OptionResult handleSubsystem(OptionContext& context)
{
    if (!context.folded.starts_with("/subsystem:")) return noMatch();
    if (context.invocation.subsystemSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate executable subsystem\n",
            "el subsistema del ejecutable está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.subsystemSeen = true;
    if (context.folded == "/subsystem:console")
        context.invocation.subsystem = linker::WindowsExecutableSubsystem::Console;
    else if (context.folded == "/subsystem:windows")
        context.invocation.subsystem = linker::WindowsExecutableSubsystem::Windows;
    else
    {
        context.error << "nsc: error: unsupported executable subsystem '"
                      << context.argument
                      << "'; expected /subsystem:console or /subsystem:windows\n";
        return matched(ExitCode::CommandLineError);
    }
    return matched();
}

OptionResult handleLinkLibrary(OptionContext& context)
{
    if (context.argument != "--link-lib") return noMatch();
    if (context.index + 1U >= context.arguments.size())
    {
        context.error << "nsc: error: " << compilerText(
            "--link-lib requires a library file path\n",
            "--link-lib requiere la ruta de un archivo de biblioteca\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.extraLinkLibraries.emplace_back(
        std::string(context.arguments[++context.index]));
    return matched();
}

OptionResult handleDebugPrefixMap(OptionContext& context)
{
    constexpr std::string_view prefix = "-fdebug-prefix-map=";
    if (!context.argument.starts_with(prefix))
    {
        if (!context.argument.starts_with("-fdebug-prefix-map")) return noMatch();
        context.error << "nsc: error: malformed debug prefix map '" << context.argument
                      << "'; expected -fdebug-prefix-map=<absolute-source-prefix>="
                         "<replacement-prefix>\n";
        return matched(ExitCode::CommandLineError);
    }
    const std::string_view mapping = context.argument.substr(prefix.size());
    const std::size_t separator = mapping.find('=');
    if (separator == std::string_view::npos || separator == 0 ||
        separator + 1U == mapping.size())
    {
        context.error << "nsc: error: malformed debug prefix map '" << context.argument
                      << "'; expected -fdebug-prefix-map=<absolute-source-prefix>="
                         "<replacement-prefix>\n";
        return matched(ExitCode::CommandLineError);
    }
    std::filesystem::path sourcePrefix(std::string(mapping.substr(0, separator)));
    if (!sourcePrefix.is_absolute())
    {
        context.error << "nsc: error: debug prefix map source must be absolute: '"
                      << mapping.substr(0, separator) << "'\n";
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.debugPrefixMaps.push_back(llvm_backend::DebugPrefixMap{
        std::move(sourcePrefix),
        std::filesystem::path(std::string(mapping.substr(separator + 1U)))});
    return matched();
}

OptionResult handleOptimizationLevel(OptionContext& context)
{
    const bool exactShape = context.argument.size() == 3U &&
        context.argument[0] == '-' && context.argument[1] == 'O';
    if (!exactShape) return noMatch();
    if (context.invocation.optimizationLevelSeen)
    {
        context.error << "nsc: error: " << compilerText(
            "duplicate LLVM optimization level\n",
            "el nivel de optimización LLVM está repetido\n");
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.optimizationLevelSeen = true;
    switch (context.argument[2])
    {
        case '0': context.invocation.optimizationLevel =
            llvm_backend::LlvmOptimizationLevel::O0; return matched();
        case '1': context.invocation.optimizationLevel =
            llvm_backend::LlvmOptimizationLevel::O1; return matched();
        case '2': context.invocation.optimizationLevel =
            llvm_backend::LlvmOptimizationLevel::O2; return matched();
        case '3': context.invocation.optimizationLevel =
            llvm_backend::LlvmOptimizationLevel::O3; return matched();
        default:
            context.error << "nsc: error: " << compilerText(
                "unsupported LLVM optimization level '",
                "nivel de optimización LLVM no compatible '")
                          << context.argument << "'\n";
            return matched(ExitCode::CommandLineError);
    }
}

OptionResult handleDebugInformation(OptionContext& context)
{
    if (context.argument == "-g0")
        context.invocation.debugInformation = llvm_backend::DebugInformationKind::None;
    else if (context.argument == "-gline-tables-only")
        context.invocation.debugInformation =
            llvm_backend::DebugInformationKind::LineTablesOnly;
    else if (context.argument == "-g")
        context.invocation.debugInformation = llvm_backend::DebugInformationKind::Full;
    else
    {
        if (!context.argument.starts_with("-g")) return noMatch();
        context.error << "nsc: error: unsupported debug information option '"
                      << context.argument
                      << "'; expected -g0, -gline-tables-only or -g\n";
        return matched(ExitCode::CommandLineError);
    }
    context.invocation.debugInformationSeen = true;
    return matched();
}

OptionResult handleMalformedOptimizationLevel(OptionContext& context)
{
    if (context.argument.size() < 2U || context.argument[0] != '-' ||
        context.argument[1] != 'O')
        return noMatch();
    context.error << "nsc: error: unsupported LLVM optimization level '"
                  << context.argument << "'\n";
    return matched(ExitCode::CommandLineError);
}

constexpr OptionHandler kOptionHandlers[] = {
    handleTiming,
    handleProgress,
#if 0 // INSTRUMENTATION CODE: optional --generics-trace parser.
    handleGenericsTrace,
#endif // INSTRUMENTATION CODE
    handleTraceAndIncrementalLink,
    handleLinkTimeOptimization,
    handleFloatingPointMode,
    handleFmaContract,
    handleFloatingPointRounding,
    handleFloatingPointDenormal,
    handleCompilationProfile,
    handleJobCount,
    handleReservedCores,
    handleArtifactCachePolicy,
    handleArtifactCacheMaximum,
    handleLibraryReferenceRoot,
    handleGenericDepth,
    handleGenericCount,
    handleTimeout,
    handleOptimizationBudget,
    handleSubsystem,
    handleLinkLibrary,
    handleDebugPrefixMap,
    handleOptimizationLevel,
    handleDebugInformation,
    handleMalformedOptimizationLevel};
} // namespace

ExitCode parseCompilerOptions(
    std::span<const std::string_view> arguments,
    CompilerInvocation& invocation,
    std::ostream& error)
{
    invocation.arguments.reserve(arguments.size());
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        std::string folded(argument);
        std::transform(
            folded.begin(), folded.end(), folded.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        OptionContext context{arguments, index, argument, folded, invocation, error};
        bool recognized = false;
        for (const OptionHandler handler : kOptionHandlers)
        {
            const OptionResult result = handler(context);
            if (!result.matched) continue;
            if (result.code != ExitCode::Success) return result.code;
            recognized = true;
            break;
        }
        if (!recognized) invocation.arguments.push_back(argument);
    }
    // ThinLTO runs its summaries, imports and backends at -O1 to -O3 (optimization part 2, item 44).
    // At -O0 it would only add cost, so the build stays partitioned; say so instead of letting the
    // option promise cross-partition optimization. The level is known only after every option.
    // ThinLTO ejecuta resumenes, imports y backends de -O1 a -O3 (parte 2, punto 44). En -O0 solo
    // agregaria costo, asi que el build sigue particionado; se dice en lugar de dejar que la opcion
    // prometa optimizacion entre particiones. El nivel solo se conoce despues de todas las opciones.
    if (invocation.linkTimeOptimization == llvm_backend::LinkTimeOptimizationMode::Thin &&
        invocation.optimizationLevel == llvm_backend::LlvmOptimizationLevel::O0)
    {
        error << "nsc: warning: " << compilerText(
            "-flto=thin at -O0 builds the ordinary partitioned code; ThinLTO runs at -O1 to -O3\n",
            "-flto=thin en -O0 compila el código particionado normal; ThinLTO se ejecuta de -O1 a -O3\n");
    }
    return ExitCode::Success;
}
}
