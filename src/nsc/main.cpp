#include "nativesharp/Driver/CrashBoundary.h"
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
class TeeBuffer final : public std::streambuf
{
public:
    TeeBuffer(std::streambuf* first, std::streambuf* second) : first_(first), second_(second) {}

protected:
    int overflow(int value) override
    {
        if (value == traits_type::eof()) return traits_type::not_eof(value);
        const char character = static_cast<char>(value);
        const bool firstOk = first_ == nullptr || first_->sputc(character) != traits_type::eof();
        const bool secondOk = second_ == nullptr || second_->sputc(character) != traits_type::eof();
        return firstOk && secondOk ? value : traits_type::eof();
    }

    int sync() override
    {
        const bool firstOk = first_ == nullptr || first_->pubsync() == 0;
        const bool secondOk = second_ == nullptr || second_->pubsync() == 0;
        return firstOk && secondOk ? 0 : -1;
    }

private:
    std::streambuf* first_;
    std::streambuf* second_;
};

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

int runCompiler(int argc, char** argv)
{
    const auto isTraceWorkerArgument = [](const char* argument) {
        return std::string_view(argument) ==
            nativesharp::llvm_backend::kLlvmTraceWorkerArgument;
    };
    const bool traceWorker = std::any_of(argv + 1, argv + argc, isTraceWorkerArgument);

    std::vector<std::string_view> rawArguments;
    rawArguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index)
        if (!isTraceWorkerArgument(argv[index])) rawArguments.emplace_back(argv[index]);
    nativesharp::driver::CompilerConsoleArguments consoleArguments =
        nativesharp::driver::parseCompilerConsoleArguments(rawArguments);
    nativesharp::driver::CompilerLanguageScope languageScope(consoleArguments.language);
    if (!consoleArguments.error.empty())
    {
        std::cerr << "nsc: error: " << consoleArguments.error << '\n';
        return static_cast<int>(nativesharp::ExitCode::CommandLineError);
    }

    std::vector<std::string_view> arguments;
    arguments.reserve(consoleArguments.arguments.size());
    std::filesystem::path logPath;
    std::filesystem::path errorLogPath;
    std::filesystem::path timeTracePath;
    std::filesystem::path failureBundlePath;
    std::string logFormat = "text";
    bool quiet = false;
    bool verbose = false;
    bool banner = true;
    bool automaticErrorLog = true;
    for (const std::string_view argument : consoleArguments.arguments)
    {
        if (argument.starts_with("--log=")) { logPath = argument.substr(6); continue; }
        if (argument.starts_with("--log-on-error=")) { errorLogPath = argument.substr(15); continue; }
        if (argument.starts_with("--log-format=")) { logFormat = argument.substr(13); continue; }
        if (argument.starts_with("--time-trace=")) { timeTracePath = argument.substr(13); continue; }
        if (argument.starts_with("--save-failure-bundle="))
        {
            failureBundlePath = argument.substr(22);
            continue;
        }
        if (argument == "--time-report") { arguments.emplace_back("--time"); continue; }
        if (argument == "--quiet") { quiet = true; continue; }
        if (argument == "--no-banner") { banner = false; continue; }
        if (argument == "--verbose") { verbose = true; continue; }
        if (argument == "--no-error-log") { automaticErrorLog = false; continue; }
        arguments.emplace_back(argument);
    }
    if (verbose)
    {
        arguments.emplace_back("--progress");
        arguments.emplace_back("--time");
    }
    const bool explicitProgress = std::ranges::any_of(
        arguments, [](std::string_view argument) {
            return argument == "--progress" || argument.starts_with("--progress=");
        });
    const bool buildCommand = !arguments.empty() &&
        (arguments.front() == "--emit-project-oop-obj" ||
         arguments.front() == "--emit-project-oop-exe" ||
         arguments.front() == "--emit-project-oop-lib" ||
         arguments.front() == "--emit-project-oop-dll" ||
         arguments.front() == "--emit-project-c-header" ||
         arguments.front() == "--emit-project-library-metadata");
    if (buildCommand && !quiet && !explicitProgress)
        arguments.emplace_back("--progress");
    if (!timeTracePath.empty()) arguments.emplace_back("--time");
    if (logFormat != "text" && logFormat != "json")
    {
        std::cerr << "nsc: error: " << nativesharp::driver::compilerText(
            "unsupported log format '", "formato de registro no compatible '") << logFormat
                  << nativesharp::driver::compilerText("'; expected text or json\n",
                                                       "'; se esperaba text o json\n");
        return static_cast<int>(nativesharp::ExitCode::CommandLineError);
    }

#if defined(_WIN32)
    if (!traceWorker && nativesharp::llvm_backend::llvmProcessTraceRequired(arguments))
    {
        nativesharp::llvm_backend::LlvmTraceChildResult child;
        if (nativesharp::llvm_backend::runCurrentExecutableWithCapturedStandardError(
                nativesharp::llvm_backend::kLlvmTraceWorkerArgument, child))
        {
            const nativesharp::llvm_backend::LlvmProcessTraceLog trace =
                nativesharp::llvm_backend::splitLlvmProcessTrace(child.standardError);
            std::cerr << trace.diagnostics;
            if (!child.launchError.empty())
            {
                std::cerr << nativesharp::driver::compilerText(
                                 "nsc: warning: LLVM trace supervisor could not read its child: ",
                                 "nsc: advertencia: el supervisor de trazas LLVM no pudo leer su proceso hijo: ")
                          << child.launchError << '\n';
            }

            if (child.exitCode != 0 && !trace.passLog.empty())
            {
                nativesharp::llvm_backend::LlvmOptimizationObservation observation;
                observation.captureVerified = trace.framingObserved && trace.framingComplete;
                observation.llvmStandardErrorCaptured = trace.llvmStandardErrorCaptured;
                observation.capturedBytes = trace.capturedBytes;
                nativesharp::llvm_backend::observeLlvmPassLog(trace.passLog, observation);
                if (!observation.lastObservedPass.empty())
                {
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
                              << nativesharp::driver::compilerText(
                                     " passes observed\n", " pases observados\n");
                }
            }
            return static_cast<int>(child.exitCode);
        }

        // Observar no puede crear una forma nueva de impedir la compilacion. Si ni siquiera se
        // pudo crear el hijo, se ejecuta el driver localmente y se conserva la causa en stderr.
        std::cerr << nativesharp::driver::compilerText(
                         "nsc: warning: LLVM trace supervisor unavailable: ",
                         "nsc: advertencia: el supervisor de trazas LLVM no está disponible: ")
                  << child.launchError << '\n';
    }
    else if (traceWorker)
    {
        nativesharp::llvm_backend::setLlvmProcessTraceActive(true);
    }
#else
    nativesharp::llvm_backend::setLlvmProcessTraceActive(
        nativesharp::llvm_backend::llvmProcessTraceRequired(arguments));
#endif

    std::ostringstream capturedOutput;
    std::ostringstream capturedError;
    TeeBuffer outputBuffer(quiet ? nullptr : std::cout.rdbuf(), capturedOutput.rdbuf());
    TeeBuffer errorBuffer(std::cerr.rdbuf(), capturedError.rdbuf());
    std::ostream observedOutput(&outputBuffer);
    std::ostream observedError(&errorBuffer);
    if (banner && !quiet && !traceWorker && !requestsJsonOutput(arguments) &&
        isCompilationInvocation(arguments))
    {
        nativesharp::driver::printCompilerBanner(observedOutput);
        observedOutput << '\n';
        observedOutput.flush();
    }
    nativesharp::driver::ProgressFormat topLevelProgressFormat =
        nativesharp::driver::ProgressFormat::Text;
    const bool topLevelProgress =
        !quiet && progressFormatFromArguments(arguments, topLevelProgressFormat);
    const std::filesystem::path projectFile = projectFileFromArguments(arguments);
    const std::string projectDetail = projectFile.generic_string();
    const std::string targetDetail = arguments.empty() ? std::string{} : std::string(arguments.front());
    const auto compilationStarted = std::chrono::steady_clock::now();
    if (topLevelProgress && !projectDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {nativesharp::basic::CompilationEventKind::WorkStarted,
                "project", projectDetail, 0, 1, {}},
            topLevelProgressFormat) << '\n';
    if (topLevelProgress && !targetDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {nativesharp::basic::CompilationEventKind::WorkStarted,
                "target", targetDetail, 0, 1, {}},
            topLevelProgressFormat) << '\n';
    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode exitCode = driver.run(arguments, observedOutput, observedError);
    const auto compilationElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - compilationStarted);
    const auto completionKind = exitCode == nativesharp::ExitCode::Success
        ? nativesharp::basic::CompilationEventKind::WorkFinished
        : nativesharp::basic::CompilationEventKind::Failed;
    if (topLevelProgress && !targetDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {completionKind, "target", targetDetail,
                exitCode == nativesharp::ExitCode::Success ? 1U : 0U, 1, compilationElapsed},
            topLevelProgressFormat) << '\n';
    if (topLevelProgress && !projectDetail.empty())
        observedOutput << nativesharp::driver::renderCompilationEvent(
            {completionKind, "project", projectDetail,
                exitCode == nativesharp::ExitCode::Success ? 1U : 0U, 1, compilationElapsed},
            topLevelProgressFormat) << '\n';
    observedOutput.flush();
    observedError.flush();
    const int numericExitCode = static_cast<int>(exitCode);
    bool logOk = true;
    if (!logPath.empty())
        logOk = publishLog(logPath, logFormat, arguments, numericExitCode,
            capturedOutput.str(), capturedError.str());
    if (numericExitCode != 0 && !errorLogPath.empty())
        logOk = publishLog(errorLogPath, logFormat, arguments, numericExitCode,
            capturedOutput.str(), capturedError.str()) && logOk;
    else if (numericExitCode != 0 && automaticErrorLog && logPath.empty())
    {
        std::filesystem::path publishedPath;
        const bool automaticLogOk = publishAutomaticErrorLog(logFormat, arguments,
            numericExitCode, capturedOutput.str(), capturedError.str(), publishedPath);
        logOk = automaticLogOk && logOk;
    }
    if (!timeTracePath.empty())
        logOk = publishPayload(timeTracePath,
            nativesharp::driver::renderCompilationTimeTraceJson(
                capturedOutput.str(), numericExitCode)) && logOk;
    if (numericExitCode != 0 && !failureBundlePath.empty())
    {
        const std::filesystem::path projectFile = projectFileFromArguments(arguments);
        std::string bundleError;
        const bool bundleOk = !projectFile.empty() &&
            nativesharp::driver::writeDiagnosticBundle(
                projectFile, failureBundlePath, exitCode,
                capturedOutput.str(), capturedError.str(),
                projectLoadOptionsFromArguments(arguments), arguments, bundleError);
        if (!bundleOk)
        {
            if (bundleError.empty())
                bundleError = "a failure bundle requires a .nsproj argument";
            std::cerr << nativesharp::driver::compilerText(
                "nsc: warning: failure bundle could not be written: ",
                "nsc: advertencia: no se pudo escribir el paquete de fallo: ")
                      << bundleError << '\n';
        }
        logOk = bundleOk && logOk;
    }
    if (!logOk)
    {
        std::cerr << nativesharp::driver::compilerText(
            "nsc: warning: compilation log could not be published atomically\n",
            "nsc: advertencia: el registro de compilación no pudo publicarse de forma atómica\n");
    }
    return numericExitCode;
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
