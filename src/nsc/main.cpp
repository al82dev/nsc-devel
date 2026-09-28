#include "nativesharp/Driver/CrashBoundary.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

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
        arguments, [](std::string_view argument) { return argument == "--progress"; });
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
    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode exitCode = driver.run(arguments, observedOutput, observedError);
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
        logOk = publishLog(timeTracePath, "json", arguments, numericExitCode,
            capturedOutput.str(), capturedError.str()) && logOk;
    if (!logOk)
    {
        std::cerr << nativesharp::driver::compilerText(
            "nsc: warning: compilation log could not be published atomically\n",
            "nsc: advertencia: el registro de compilación no pudo publicarse de forma atómica\n");
    }
    return numericExitCode;
}
}

int main(int argc, char** argv)
{
    return nativesharp::driver::runWithCrashBoundary(
        [&]() { return runCompiler(argc, argv); }, std::cerr);
}
