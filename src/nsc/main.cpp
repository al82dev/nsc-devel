#include "nativesharp/Driver/CrashBoundary.h"
#include "Internal/CompilerRunConfiguration.h"
#include "Internal/TeeBuffer.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/Driver/DiagnosticBundle.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <streambuf>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
using nsc_internal::TeeBuffer;
using nsc_internal::CompilerRunConfiguration;

bool automaticErrorLogEnabled()
{
    char value[2]{};
    const DWORD length = GetEnvironmentVariableA("NSC_NO_ERROR_LOG", value, sizeof(value));
    return length != 1U || value[0] != '1';
}

std::string jsonEscape(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character >= 0x20U) escaped.push_back(static_cast<char>(character));
            break;
        }
    }
    return escaped;
}

bool publishPayload(const std::filesystem::path& path, std::string_view payload)
{
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream destination(temporary, std::ios::binary | std::ios::trunc);
    if (!destination) return false;
    destination << payload;
    destination.close();
    if (!destination) return false;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) std::filesystem::remove(temporary, error);
    return !error;
}

bool publishLog(const std::filesystem::path& path, std::string_view format,
    const std::vector<std::string_view>& arguments, int exitCode,
    std::string_view standardOutput, std::string_view standardError)
{
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream destination(temporary, std::ios::binary | std::ios::trunc);
    if (!destination) return false;
    if (format == "json")
    {
        destination << "{\n  \"schema\": \"nativesharp-compilation-log\",\n"
                    << "  \"version\": 1,\n  \"exit_code\": " << exitCode << ",\n"
                    << "  \"arguments\": [";
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (index != 0) destination << ", ";
            destination << '"' << jsonEscape(arguments[index]) << '"';
        }
        destination << "],\n  \"stdout\": \"" << jsonEscape(standardOutput)
                    << "\",\n  \"stderr\": \"" << jsonEscape(standardError) << "\"\n}\n";
    }
    else
    {
        destination << "NativeSharp compilation log v1\nexit_code=" << exitCode << "\narguments=";
        for (const std::string_view argument : arguments) destination << ' ' << argument;
        destination << "\n--- stdout ---\n" << standardOutput
                    << "--- stderr ---\n" << standardError;
    }
    destination.close();
    if (!destination) return false;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) std::filesystem::remove(temporary, error);
    return !error;
}

bool publishAutomaticErrorLog(std::string_view format,
    const std::vector<std::string_view>& arguments, int exitCode,
    std::string_view standardOutput, std::string_view standardError,
    std::filesystem::path& publishedPath)
{
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::current_path(error);
    if (error) return false;

    constexpr std::size_t kMaximumLogOrdinal = 1000000U;
    for (std::size_t ordinal = 0; ordinal < kMaximumLogOrdinal; ++ordinal)
    {
        const std::string fileName = ordinal == 0U
            ? "error.log"
            : "error" + std::to_string(ordinal) + ".log";
        const std::filesystem::path candidate = directory / fileName;
        const std::filesystem::path claim = candidate.string() + ".claim";
        error.clear();
        if (!std::filesystem::create_directory(claim, error))
        {
            if (error) return false;
            continue;
        }

        const bool available = !std::filesystem::exists(candidate, error) && !error;
        const bool published = available && publishLog(candidate, format, arguments, exitCode,
            standardOutput, standardError);
        std::error_code removeError;
        std::filesystem::remove(claim, removeError);
        if (published)
        {
            publishedPath = candidate;
            return true;
        }
        if (!available)
        {
            continue;
        }
        return false;
    }
    return false;
}

bool isCompilationInvocation(std::span<const std::string_view> arguments)
{
    if (arguments.empty()) return false;
    if (arguments.front() == "build" || arguments.front() == "run") return true;
    if (arguments.front() == "new" || arguments.front() == "restore" ||
        arguments.front() == "clean" || arguments.front() == "add" ||
        arguments.front() == "remove" || arguments.front() == "list")
        return false;
    for (const std::string_view argument : arguments)
    {
        if (argument.starts_with("--emit-")) return true;
        if (argument.starts_with("--check") || argument.starts_with("--dump-") ||
            argument.starts_with("--verify-") || argument.starts_with("--print-") ||
            argument.starts_with("--write-") || argument.starts_with("--compare-") ||
            argument.starts_with("--apply-"))
            return false;
    }
    for (const std::string_view argument : arguments)
    {
        if (!argument.empty() && argument.front() != '-' &&
            (argument.ends_with(".ns") || argument.ends_with(".nsproj")))
            return true;
    }
    return false;
}

bool requestsJsonOutput(std::span<const std::string_view> arguments)
{
    return std::ranges::any_of(arguments, [](std::string_view argument) {
        return argument == "--output-format=json";
    });
}

std::filesystem::path projectFileFromArguments(
    std::span<const std::string_view> arguments)
{
    for (const std::string_view argument : arguments)
        if (!argument.empty() && argument.front() != '-' && argument.ends_with(".nsproj"))
            return std::filesystem::path(std::string(argument));
    return {};
}

nativesharp::compilation::ProjectLoadOptions projectLoadOptionsFromArguments(
    std::span<const std::string_view> arguments)
{
    nativesharp::compilation::ProjectLoadOptions options;
    constexpr std::string_view option = "--library-reference-root";
    constexpr std::string_view prefix = "--library-reference-root=";
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument.starts_with(prefix))
            options.libraryReferenceRoot =
                std::filesystem::path(std::string(argument.substr(prefix.size())));
        else if (argument == option && index + 1U < arguments.size())
            options.libraryReferenceRoot =
                std::filesystem::path(std::string(arguments[index + 1U]));
    }
    return options;
}

bool progressFormatFromArguments(
    std::span<const std::string_view> arguments,
    nativesharp::driver::ProgressFormat& format)
{
    bool requested = false;
    for (const std::string_view argument : arguments)
    {
        if (argument == "--progress")
        {
            requested = true;
            continue;
        }
        constexpr std::string_view progressPrefix = "--progress=";
        constexpr std::string_view formatPrefix = "--progress-format=";
        if (argument.starts_with(progressPrefix))
        {
            requested = nativesharp::driver::parseProgressFormat(
                argument.substr(progressPrefix.size()), format);
        }
        else if (argument.starts_with(formatPrefix) &&
                 !nativesharp::driver::parseProgressFormat(
                     argument.substr(formatPrefix.size()), format))
        {
            requested = false;
        }
    }
    return requested;
}

bool isTraceWorkerArgument(const char* argument)
{
    return std::string_view(argument) == nativesharp::llvm_backend::kLlvmTraceWorkerArgument;
}

void configureCompilerRun(
    const nativesharp::driver::CompilerConsoleArguments& consoleArguments,
    CompilerRunConfiguration& configuration)
{
    configuration.arguments.reserve(consoleArguments.arguments.size());
    configuration.automaticErrorLog = automaticErrorLogEnabled();
    for (const std::string_view argument : consoleArguments.arguments)
    {
        if (argument.starts_with("--log=")) { configuration.logPath = argument.substr(6); continue; }
        if (argument.starts_with("--log-on-error=")) { configuration.errorLogPath = argument.substr(15); continue; }
        if (argument.starts_with("--log-format=")) { configuration.logFormat = argument.substr(13); continue; }
        if (argument.starts_with("--time-trace=")) { configuration.timeTracePath = argument.substr(13); continue; }
        if (argument.starts_with("--save-failure-bundle="))
        {
            configuration.failureBundlePath = argument.substr(22);
            continue;
        }
        if (argument == "--time-report") { configuration.arguments.emplace_back("--time"); continue; }
        if (argument == "--quiet") { configuration.quiet = true; continue; }
        if (argument == "--no-banner") { configuration.banner = false; continue; }
        if (argument == "--verbose") { configuration.verbose = true; continue; }
        if (argument == "--no-error-log") { configuration.automaticErrorLog = false; continue; }
        configuration.arguments.emplace_back(argument);
    }
}

void addImplicitCompilerOptions(CompilerRunConfiguration& configuration)
{
    if (configuration.verbose)
    {
        configuration.arguments.emplace_back("--progress");
        configuration.arguments.emplace_back("--time");
    }
    const bool explicitProgress = std::ranges::any_of(
        configuration.arguments, [](std::string_view argument) {
            return argument == "--progress" || argument.starts_with("--progress=");
        });
    const bool buildCommand = !configuration.arguments.empty() &&
        (configuration.arguments.front() == "--emit-project-oop-obj" ||
         configuration.arguments.front() == "--emit-project-oop-exe" ||
         configuration.arguments.front() == "--emit-project-oop-lib" ||
         configuration.arguments.front() == "--emit-project-oop-dll" ||
         configuration.arguments.front() == "--emit-project-c-header" ||
         configuration.arguments.front() == "--emit-project-library-metadata");
    if (buildCommand && !configuration.quiet && !explicitProgress)
        configuration.arguments.emplace_back("--progress");
    if (!configuration.timeTracePath.empty()) configuration.arguments.emplace_back("--time");
}

bool validateCompilerRunConfiguration(const CompilerRunConfiguration& configuration)
{
    if (configuration.logFormat == "text" || configuration.logFormat == "json") return true;
    std::cerr << "nsc: error: " << nativesharp::driver::compilerText(
        "unsupported log format '", "formato de registro no compatible '") << configuration.logFormat
              << nativesharp::driver::compilerText("'; expected text or json\n",
                                                   "'; se esperaba text o json\n");
    return false;
}

#if defined(_WIN32)
void reportLlvmTraceFailure(
    const nativesharp::llvm_backend::LlvmTraceChildResult& child,
    const nativesharp::llvm_backend::LlvmProcessTraceLog& trace)
{
    if (child.exitCode == 0 || trace.passLog.empty()) return;
    nativesharp::llvm_backend::LlvmOptimizationObservation observation;
    observation.captureVerified = trace.framingObserved && trace.framingComplete;
    observation.llvmStandardErrorCaptured = trace.llvmStandardErrorCaptured;
    observation.capturedBytes = trace.capturedBytes;
    nativesharp::llvm_backend::observeLlvmPassLog(trace.passLog, observation);
    if (observation.lastObservedPass.empty()) return;
    std::cerr << nativesharp::driver::compilerText(
                     "nsc: LLVM optimization trace: last observed pass '",
                     "nsc: traza de optimización LLVM: último pase observado '")
              << observation.lastObservedPass << "'";
    if (!observation.lastObservedUnit.empty())
    {
        std::cerr << nativesharp::driver::compilerText(
                         "; last observed function/unit '",
                         "; última función/unidad observada '")
                  << observation.lastObservedUnit << "'";
    }
    std::cerr << "; " << observation.observedPassCount
              << nativesharp::driver::compilerText(" passes observed\n", " pases observados\n");
}

std::optional<int> superviseLlvmTrace(
    const std::span<const std::string_view> arguments,
    const bool traceWorker)
{
    if (traceWorker)
    {
        nativesharp::llvm_backend::setLlvmProcessTraceActive(true);
        return std::nullopt;
    }
    if (!nativesharp::llvm_backend::llvmProcessTraceRequired(arguments)) return std::nullopt;
    nativesharp::llvm_backend::LlvmTraceChildResult child;
    if (nativesharp::llvm_backend::runCurrentExecutableWithCapturedStandardError(
            nativesharp::llvm_backend::kLlvmTraceWorkerArgument, child))
    {
        const auto trace = nativesharp::llvm_backend::splitLlvmProcessTrace(child.standardError);
        std::cerr << trace.diagnostics;
        if (!child.launchError.empty())
        {
            std::cerr << nativesharp::driver::compilerText(
                             "nsc: warning: LLVM trace supervisor could not read its child: ",
                             "nsc: advertencia: el supervisor de trazas LLVM no pudo leer su proceso hijo: ")
                      << child.launchError << '\n';
        }
        reportLlvmTraceFailure(child, trace);
        return static_cast<int>(child.exitCode);
    }
    // Observar no puede crear una forma nueva de impedir la compilacion. Si ni siquiera se
    // pudo crear el hijo, se ejecuta el driver localmente y se conserva la causa en stderr.
    std::cerr << nativesharp::driver::compilerText(
                     "nsc: warning: LLVM trace supervisor unavailable: ",
                     "nsc: advertencia: el supervisor de trazas LLVM no está disponible: ")
              << child.launchError << '\n';
    return std::nullopt;
}
#endif

nativesharp::ExitCode executeObservedCompiler(
    const CompilerRunConfiguration& configuration,
    const bool traceWorker,
    std::ostream& observedOutput,
    std::ostream& observedError)
{
    if (configuration.banner && !configuration.quiet && !traceWorker &&
        !requestsJsonOutput(configuration.arguments) && isCompilationInvocation(configuration.arguments))
    {
        nativesharp::driver::printCompilerBanner(observedOutput);
        observedOutput << '\n';
        observedOutput.flush();
    }
    nativesharp::driver::ProgressFormat progressFormat = nativesharp::driver::ProgressFormat::Text;
    const bool progress = !configuration.quiet &&
        progressFormatFromArguments(configuration.arguments, progressFormat);
    const std::filesystem::path projectFile = projectFileFromArguments(configuration.arguments);
    const std::string projectDetail = projectFile.generic_string();
    const std::string targetDetail = configuration.arguments.empty()
        ? std::string{} : std::string(configuration.arguments.front());
    const auto started = std::chrono::steady_clock::now();
    if (progress && !projectDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {nativesharp::basic::CompilationEventKind::WorkStarted, "project", projectDetail, 0, 1, {}},
            progressFormat) << '\n';
    if (progress && !targetDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {nativesharp::basic::CompilationEventKind::WorkStarted, "target", targetDetail, 0, 1, {}},
            progressFormat) << '\n';
    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode exitCode = driver.run(configuration.arguments, observedOutput, observedError);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started);
    const auto completion = exitCode == nativesharp::ExitCode::Success
        ? nativesharp::basic::CompilationEventKind::WorkFinished
        : nativesharp::basic::CompilationEventKind::Failed;
    if (progress && !targetDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {completion, "target", targetDetail, exitCode == nativesharp::ExitCode::Success ? 1U : 0U, 1, elapsed},
            progressFormat) << '\n';
    if (progress && !projectDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {completion, "project", projectDetail, exitCode == nativesharp::ExitCode::Success ? 1U : 0U, 1, elapsed},
            progressFormat) << '\n';
    return exitCode;
}

bool publishFailureBundle(
    const CompilerRunConfiguration& configuration,
    const nativesharp::ExitCode exitCode,
    const std::string& capturedOutput,
    const std::string& capturedError)
{
    const std::filesystem::path projectFile = projectFileFromArguments(configuration.arguments);
    std::string bundleError;
    const bool bundleOk = !projectFile.empty() &&
        nativesharp::driver::writeDiagnosticBundle(
            projectFile, configuration.failureBundlePath, exitCode,
            capturedOutput, capturedError,
            projectLoadOptionsFromArguments(configuration.arguments), configuration.arguments, bundleError);
    if (!bundleOk)
    {
        if (bundleError.empty()) bundleError = "a failure bundle requires a .nsproj argument";
        std::cerr << nativesharp::driver::compilerText(
            "nsc: warning: failure bundle could not be written: ",
            "nsc: advertencia: no se pudo escribir el paquete de fallo: ") << bundleError << '\n';
    }
    return bundleOk;
}

bool publishCompilerRun(
    const CompilerRunConfiguration& configuration,
    const nativesharp::ExitCode exitCode,
    const std::string& capturedOutput,
    const std::string& capturedError)
{
    const int numericExitCode = static_cast<int>(exitCode);
    bool logOk = true;
    if (!configuration.logPath.empty())
        logOk = publishLog(configuration.logPath, configuration.logFormat, configuration.arguments,
            numericExitCode, capturedOutput, capturedError);
    if (numericExitCode != 0 && !configuration.errorLogPath.empty())
        logOk = publishLog(configuration.errorLogPath, configuration.logFormat, configuration.arguments,
            numericExitCode, capturedOutput, capturedError) && logOk;
    else if (numericExitCode != 0 && configuration.automaticErrorLog && configuration.logPath.empty())
    {
        std::filesystem::path publishedPath;
        const bool automaticLogOk = publishAutomaticErrorLog(configuration.logFormat, configuration.arguments,
            numericExitCode, capturedOutput, capturedError, publishedPath);
        logOk = automaticLogOk && logOk;
    }
    if (!configuration.timeTracePath.empty())
        logOk = publishPayload(configuration.timeTracePath,
            nativesharp::driver::renderCompilationTimeTraceJson(capturedOutput, numericExitCode)) && logOk;
    if (numericExitCode != 0 && !configuration.failureBundlePath.empty())
        logOk = publishFailureBundle(configuration, exitCode, capturedOutput, capturedError) && logOk;
    return logOk;
}

int runCompiler(int argc, char** argv)
{
    const bool traceWorker = std::any_of(argv + 1, argv + argc, isTraceWorkerArgument);
    std::vector<std::string_view> rawArguments;
    rawArguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index)
        if (!isTraceWorkerArgument(argv[index])) rawArguments.emplace_back(argv[index]);
    const nativesharp::driver::CompilerConsoleArguments consoleArguments =
        nativesharp::driver::parseCompilerConsoleArguments(rawArguments);
    nativesharp::driver::CompilerLanguageScope languageScope(consoleArguments.language);
    if (!consoleArguments.error.empty())
    {
        std::cerr << "nsc: error: " << consoleArguments.error << '\n';
        return static_cast<int>(nativesharp::ExitCode::CommandLineError);
    }
    CompilerRunConfiguration configuration;
    configureCompilerRun(consoleArguments, configuration);
    addImplicitCompilerOptions(configuration);
    if (!validateCompilerRunConfiguration(configuration))
        return static_cast<int>(nativesharp::ExitCode::CommandLineError);
#if defined(_WIN32)
    if (const std::optional<int> supervised = superviseLlvmTrace(configuration.arguments, traceWorker))
        return *supervised;
#else
    nativesharp::llvm_backend::setLlvmProcessTraceActive(
        nativesharp::llvm_backend::llvmProcessTraceRequired(configuration.arguments));
#endif
    std::ostringstream capturedOutput;
    std::ostringstream capturedError;
    TeeBuffer outputBuffer(configuration.quiet ? nullptr : std::cout.rdbuf(), capturedOutput.rdbuf());
    TeeBuffer errorBuffer(std::cerr.rdbuf(), capturedError.rdbuf());
    std::ostream observedOutput(&outputBuffer);
    std::ostream observedError(&errorBuffer);
    const nativesharp::ExitCode exitCode = executeObservedCompiler(
        configuration, traceWorker, observedOutput, observedError);
    observedOutput.flush();
    observedError.flush();
    const bool logOk = publishCompilerRun(
        configuration, exitCode, capturedOutput.str(), capturedError.str());
    if (!logOk)
    {
        std::cerr << nativesharp::driver::compilerText(
            "nsc: warning: compilation log could not be published atomically\n",
            "nsc: advertencia: el registro de compilación no pudo publicarse de forma atómica\n");
    }
    return static_cast<int>(exitCode);
}
}

#ifdef _WIN32
std::string utf8Argument(std::wstring_view argument)
{
    if (argument.empty()) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, argument.data(), static_cast<int>(argument.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) throw std::runtime_error("could not convert a command-line argument to UTF-8");
    std::string converted(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, argument.data(), static_cast<int>(argument.size()),
            converted.data(), required, nullptr, nullptr) != required)
        throw std::runtime_error("could not convert a command-line argument to UTF-8");
    return converted;
}

int wmain(int argc, wchar_t** argv)
{
    return nativesharp::driver::runWithCrashBoundary(
        [&]()
        {
            std::vector<std::string> utf8Arguments;
            utf8Arguments.reserve(static_cast<std::size_t>(argc));
            for (int index = 0; index < argc; ++index)
                utf8Arguments.push_back(utf8Argument(argv[index]));

            std::vector<char*> argumentPointers;
            argumentPointers.reserve(utf8Arguments.size());
            for (std::string& argument : utf8Arguments)
                argumentPointers.push_back(argument.data());
            return runCompiler(argc, argumentPointers.data());
        },
        std::cerr);
}
#else
int main(int argc, char** argv)
{
    return nativesharp::driver::runWithCrashBoundary(
        [&]() { return runCompiler(argc, argv); }, std::cerr);
}
#endif
