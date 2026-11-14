#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ProjectCommands.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
namespace fs = std::filesystem;

void writeFile(const fs::path& path, std::string_view contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

nativesharp::ExitCode run(const std::vector<std::string>& owned,
                          std::ostringstream& output, std::ostringstream& error)
{
    std::vector<std::string_view> arguments;
    for (const std::string& value : owned) arguments.push_back(value);
    return nativesharp::driver::Driver{}.run(arguments, output, error);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;
    TestContext tests;

    const fs::path root = fs::temp_directory_path() / "nativesharp_project_commands_tests";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root);
    const fs::path projectRoot = root / "Sample";
    const fs::path project = projectRoot / "Sample.nsproj";

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"new", "console", "-n", "Sample", "-o", projectRoot.string(),
                                   "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::Success && fs::is_regular_file(project),
                     "new creates a strict NativeSharp project");
        tests.expect(output.str().find("\"success\":true") != std::string::npos && error.str().empty(),
                      "new emits one successful JSON protocol document");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"--language", "es", "new", "console", "--bad-option"},
                                  output, error);
        tests.expect(code == ExitCode::CommandLineError &&
                         error.str().find("opción de new desconocida") != std::string::npos,
                     "project commands localize human-readable failures");
        tests.expect(output.str().empty(), "localized project-command failure keeps stdout empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"--language", "es", "new", "console", "--bad-option",
                                   "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::CommandLineError &&
                         error.str().find("unknown new option") != std::string::npos &&
                         error.str().find("opción de new") == std::string::npos,
                     "JSON project-command protocol remains language-neutral");
    }

    {
        writeFile(projectRoot / "Content/title.txt", "NativeSharp");
        writeFile(projectRoot / "Content/shaders/default.vert", "shader");
        std::ostringstream output;
        std::ostringstream error;
        const fs::path referenceRoot = root / "configuration" / "O2";
        const std::vector<std::string> owned{
            "build", project.string(), "--no-restore", "--library-reference-root",
            referenceRoot.string(), "--output-format=json"};
        std::vector<std::string_view> arguments;
        for (const std::string& value : owned) arguments.push_back(value);
        bool sawReferenceRoot = false;
        const auto compiler = [&](std::span<const std::string_view> compilerArguments,
                                  std::ostream&, std::ostream&) {
            for (std::size_t index = 0; index + 1 < compilerArguments.size(); ++index)
            {
                if (compilerArguments[index] == "--library-reference-root" &&
                    compilerArguments[index + 1] == referenceRoot.string())
                {
                    sawReferenceRoot = true;
                }
                if (compilerArguments[index] == "-o")
                {
                    writeFile(fs::path(compilerArguments[index + 1]), "executable");
                    return ExitCode::Success;
                }
            }
            return ExitCode::CommandLineError;
        };
        const ExitCode code = nativesharp::driver::runProjectCommand(arguments, output, error, compiler);
        tests.expect(code == ExitCode::Success &&
                     fs::is_regular_file(projectRoot / "bin/Debug/Content/title.txt") &&
                     fs::is_regular_file(projectRoot / "bin/Debug/Content/shaders/default.vert"),
                     "build recursively publishes the project Content directory");
        tests.expect(output.str().find("\"contentFiles\":2") != std::string::npos && error.str().empty(),
                     "build reports the number of published Content files");
        tests.expect(sawReferenceRoot,
                     "build forwards the library-reference root to the nested compiler invocation");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string> owned{
            "build", project.string(), "--no-restore", "--progress=json",
            "--output-format=json"};
        std::vector<std::string_view> arguments;
        for (const std::string& value : owned) arguments.push_back(value);
        bool sawProgress = false;
        const auto compiler = [&](std::span<const std::string_view> compilerArguments,
                                  std::ostream& compilerOutput, std::ostream&) {
            for (std::size_t index = 0; index < compilerArguments.size(); ++index)
            {
                if (compilerArguments[index] == "--progress=json") sawProgress = true;
                if (compilerArguments[index] == "-o" && index + 1U < compilerArguments.size())
                    writeFile(fs::path(compilerArguments[index + 1U]), "executable");
            }
            compilerOutput <<
                "{\"schema\":\"nativesharp-progress-event\",\"version\":1,"
                "\"event\":\"phase-started\",\"phase\":\"parse\"}\n";
            compilerOutput.flush();
            compilerOutput << "technical output\n";
            return ExitCode::Success;
        };

        const ExitCode code = nativesharp::driver::runProjectCommand(
            arguments, output, error, compiler);
        const std::string text = output.str();
        const std::size_t progress = text.find("\"schema\":\"nativesharp-progress-event\"");
        const std::size_t progressName = text.find("nativesharp-progress-event");
        const std::size_t envelope = text.find("\"schemaVersion\":1");
        tests.expect(code == ExitCode::Success && sawProgress && error.str().empty(),
                     "build accepts and forwards JSON progress selection");
        tests.expect(progress != std::string::npos && envelope > progress,
                     "JSON progress streams before the project result envelope");
        tests.expect(text.find("\"stdout\":\"technical output\\n\"") != std::string::npos &&
                         progressName != std::string::npos &&
                         text.find("nativesharp-progress-event", progressName + 1U) == std::string::npos,
                     "project envelope retains technical output without duplicating progress events: " + text);
    }

    // Optimization part 2, items 41 and 43: nsc build forwards -flto and reports the mode.
    // Puntos 41 y 43 de la parte 2: nsc build reenvia -flto y reporta el modo.
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string> owned{
            "build", project.string(), "--no-restore", "-flto=thin", "--output-format=json"};
        std::vector<std::string_view> arguments;
        for (const std::string& value : owned) arguments.push_back(value);
        bool sawThin = false;
        const auto compiler = [&](std::span<const std::string_view> compilerArguments,
                                  std::ostream&, std::ostream&) {
            for (std::size_t index = 0; index < compilerArguments.size(); ++index)
            {
                if (compilerArguments[index] == "-flto=thin") sawThin = true;
                if (compilerArguments[index] == "-o" && index + 1U < compilerArguments.size())
                    writeFile(fs::path(compilerArguments[index + 1U]), "executable");
            }
            return ExitCode::Success;
        };
        const ExitCode code = nativesharp::driver::runProjectCommand(arguments, output, error, compiler);
        tests.expect(code == ExitCode::Success && sawThin,
                     "build forwards -flto=thin to the nested compiler invocation");
        tests.expect(output.str().find("\"linkTimeOptimization\":\"thin\"") != std::string::npos,
                     "the build result names the link-time optimization mode");

        std::ostringstream rejectedOutput;
        std::ostringstream rejectedError;
        const std::vector<std::string> rejectedOwned{
            "build", project.string(), "--no-restore", "-flto=partial"};
        std::vector<std::string_view> rejectedArguments;
        for (const std::string& value : rejectedOwned) rejectedArguments.push_back(value);
        tests.expect(nativesharp::driver::runProjectCommand(
                         rejectedArguments, rejectedOutput, rejectedError, compiler) != ExitCode::Success &&
                         rejectedError.str().find("none, full, or thin") != std::string::npos,
                     "build rejects an unknown link-time optimization mode");
    }

    {
        const fs::path debugRoot = root / "DebugSettings";
        const fs::path debugProject = debugRoot / "DebugSettings.nsproj";
        writeFile(debugProject, "<Project><PropertyGroup><Name>DebugSettings</Name>"
            "<OutputType>Exe</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
        writeFile(debugRoot / "Main.ns", "public class Program { public static int Main() { return 0; } }\n");

        const auto invoke = [&](std::string_view jsonValue, std::string_view expectedFlag) {
            writeFile(debugRoot / "DebugSettings.nspm.json",
                "{\"schemaVersion\":1,\"build\":{\"debug_information\":" + std::string(jsonValue) + "},\"packages\":[]}");
            std::ostringstream output, error;
            const std::vector<std::string> owned{"build", debugProject.string(), "--no-restore"};
            std::vector<std::string_view> args;
            for (const std::string& value : owned) args.push_back(value);
            bool sawExpected = false;
            const auto compiler = [&](std::span<const std::string_view> compilerArgs,
                                      std::ostream&, std::ostream&) {
                for (std::size_t i = 0; i < compilerArgs.size(); ++i)
                {
                    if (compilerArgs[i] == expectedFlag) sawExpected = true;
                    if (compilerArgs[i] == "-o" && i + 1 < compilerArgs.size())
                        writeFile(fs::path(compilerArgs[i + 1]), "executable");
                }
                return ExitCode::Success;
            };
            const ExitCode code = nativesharp::driver::runProjectCommand(args, output, error, compiler);
            return std::tuple(code, error.str(), sawExpected);
        };

        for (const std::string_view invalid : {"\"typo\"", "123", "null"})
        {
            const auto result = invoke(invalid, "-g");
            tests.expect(std::get<0>(result) != ExitCode::Success &&
                    std::get<1>(result).find("debug_information") != std::string::npos,
                "build rejects invalid debug_information: " + std::string(invalid));
        }
        for (const auto& [setting, flag] :
             {std::pair<std::string_view, std::string_view>{"\"none\"", "-g0"},
              {"\"line-tables-only\"", "-gline-tables-only"}, {"\"full\"", "-g"}})
        {
            const auto result = invoke(setting, flag);
            tests.expect(std::get<0>(result) == ExitCode::Success && std::get<2>(result),
                "build forwards valid debug_information: " + std::string(setting) + ": " + std::get<1>(result));
        }
    }

    {
        const fs::path numberRoot = root / "JsonNumbers";
        const fs::path numberProject = numberRoot / "Probe.nsproj";
        writeFile(numberProject, "<Project/>\n");
        for (const std::string_view number : {"1e", "1-2", "01", "1.", "-"})
        {
            writeFile(numberRoot / "Probe.nspm.json",
                "{\"schemaVersion\":" + std::string(number) + ",\"packages\":[]}");
            std::ostringstream output, error;
            tests.expect(run({"list", "package", numberProject.string(), "--output-format=json"}, output, error) !=
                    ExitCode::Success,
                "malformed JSON number is rejected: " + std::string(number));
        }
        for (const std::string_view number : {"1", "1e+0", "1.0"})
        {
            writeFile(numberRoot / "Probe.nspm.json",
                "{\"schemaVersion\":" + std::string(number) + ",\"packages\":[]}");
            std::ostringstream output, error;
            tests.expect(run({"list", "package", numberProject.string(), "--output-format=json"}, output, error) ==
                    ExitCode::Success,
                "valid JSON number remains accepted: " + std::string(number));
        }
    }

    const fs::path package = root / "packages" / "NativeSharp.Test" / "2.0.0";
    writeFile(package / "NativeSharp.Test.lib", "archive");
    writeFile(package / "NativeSharp.Test.nsmeta", "metadata");
    writeFile(package / "NativeSharp.Test.dll", "runtime");

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"add", "package", "NativeSharp.Test", "--version", "2.0.0",
                                   "--source", package.string(), project.string()}, output, error);
        tests.expect(code == ExitCode::Success, "add package records dependency intent");
        tests.expect(error.str().empty(), "add package keeps stderr empty");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"restore", project.string(), "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::Success && fs::is_regular_file(projectRoot / "NativeSharp.lock.json"),
                     "restore writes a deterministic lock file");
        tests.expect(fs::is_regular_file(projectRoot / "obj/nativesharp/packages/NativeSharp.Test/2.0.0/NativeSharp.Test.lib"),
                     "restore materializes package artifacts under obj");
        tests.expect(output.str().find("\"verified\":3") != std::string::npos,
                     "restore reports copied artifacts through JSON");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"restore", project.string(), "--verify", "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::Success && output.str().find("Restore verified") != std::string::npos,
                     "restore verify accepts matching hashes without rewriting packages");
    }

#if defined(_WIN32)
    {
        const fs::path installed = projectRoot / "obj/nativesharp/packages";
        const HANDLE held = CreateFileW(installed.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        tests.expect(held != INVALID_HANDLE_VALUE, "restore failure fixture locks the package directory");
        if (held != INVALID_HANDLE_VALUE)
        {
            std::error_code expected;
            fs::rename(installed, installed.parent_path() / "rename-probe", expected);
            tests.expect(bool(expected), "the held directory prevents renaming the package root");
            std::ostringstream output, error;
            const ExitCode code = run({"restore", project.string(), "--force"}, output, error);
            CloseHandle(held);
            tests.expect(code != ExitCode::Success && expected &&
                    error.str().find(expected.message()) != std::string::npos,
                "restore reports the original rename error after cleaning staging");
            tests.expect(fs::is_regular_file(installed / "NativeSharp.Test/2.0.0/NativeSharp.Test.lib"),
                "failed restore keeps the previously published package");
        }
    }
#endif

    // Optimization part 2, item 52: restore brings the ThinLTO sidecar of a static library with the
    // library, and never the one beside a DLL's import library.
    // Parte 2, punto 52: restore trae el sidecar ThinLTO de una biblioteca estatica con la biblioteca,
    // y nunca el que esta junto a la biblioteca de importacion de una DLL.
    {
        const fs::path thinRoot = root / "ThinRestore";
        const fs::path thinProject = thinRoot / "ThinRestore.nsproj";
        std::ostringstream newOutput;
        std::ostringstream newError;
        const ExitCode created =
            run({"new", "console", "-n", "ThinRestore", "-o", thinRoot.string()}, newOutput, newError);
        tests.expect(created == ExitCode::Success, "new creates the ThinLTO restore project");
        const fs::path staticPackage = root / "packages" / "NativeSharp.Thin" / "1.0.0";
        writeFile(staticPackage / "NativeSharp.Thin.lib", "archive");
        writeFile(staticPackage / "NativeSharp.Thin.nsmeta", "metadata");
        writeFile(staticPackage / "NativeSharp.Thin.nsthinlto", "sidecar");
        const fs::path dllPackage = root / "packages" / "NativeSharp.ThinDll" / "1.0.0";
        writeFile(dllPackage / "NativeSharp.ThinDll.lib", "import");
        writeFile(dllPackage / "NativeSharp.ThinDll.nsmeta", "metadata");
        writeFile(dllPackage / "NativeSharp.ThinDll.dll", "runtime");
        writeFile(dllPackage / "NativeSharp.ThinDll.nsthinlto", "sidecar");
        for (const auto& [id, source] : {std::pair{std::string("NativeSharp.Thin"), staticPackage},
                                         std::pair{std::string("NativeSharp.ThinDll"), dllPackage}})
        {
            std::ostringstream output;
            std::ostringstream error;
            tests.expect(run({"add", "package", id, "--version", "1.0.0", "--source", source.string(),
                              thinProject.string()}, output, error) == ExitCode::Success,
                         "add package records the ThinLTO restore dependency " + id);
        }
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"restore", thinProject.string(), "--output-format=json"}, output, error);
        const fs::path restored = thinRoot / "obj/nativesharp/packages";
        std::ifstream lockStream(thinRoot / "NativeSharp.lock.json", std::ios::binary);
        const std::string lock{std::istreambuf_iterator<char>(lockStream), {}};
        tests.expect(code == ExitCode::Success &&
                         fs::is_regular_file(restored / "NativeSharp.Thin/1.0.0/NativeSharp.Thin.nsthinlto") &&
                         lock.find("thinlto-sidecar") != std::string::npos,
                     "restore materializes and locks the ThinLTO sidecar of a static library: " + error.str());
        tests.expect(!fs::is_regular_file(restored / "NativeSharp.ThinDll/1.0.0/NativeSharp.ThinDll.nsthinlto"),
                     "restore leaves out a ThinLTO sidecar beside a DLL import library");
    }

    {
        const fs::path lock = projectRoot / "NativeSharp.lock.json";
        const fs::path cached = projectRoot /
            "obj/nativesharp/packages/NativeSharp.Test/2.0.0/NativeSharp.Test.lib";
        const auto lockTime = fs::last_write_time(lock);
        const auto cacheTime = fs::last_write_time(cached);
        const auto compile = [](std::span<const std::string_view> arguments,
                                std::ostream&, std::ostream&) {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (arguments[index] != "-o") continue;
                writeFile(fs::path(arguments[index + 1]), "executable");
                return ExitCode::Success;
            }
            return ExitCode::CommandLineError;
        };
        const auto buildOnce = [&]() {
            std::ostringstream output;
            std::ostringstream error;
            const std::vector<std::string> owned{
                "build", project.string(), "--precompiled-only", "--no-restore",
                "--dependency-report", "--output-format=json"};
            std::vector<std::string_view> arguments;
            for (const std::string& value : owned) arguments.push_back(value);
            const ExitCode code = nativesharp::driver::runProjectCommand(
                arguments, output, error, compile);
            return std::tuple(code, output.str(), error.str());
        };
        const auto first = buildOnce();
        const auto second = buildOnce();
        tests.expect(std::get<0>(first) == ExitCode::Success &&
                std::get<1>(first).find("\"kind\":\"source\"") != std::string::npos &&
                std::get<1>(first).find("\"kind\":\"library-reference\"") != std::string::npos &&
                std::get<1>(first).find("\"kind\":\"store-cache\"") != std::string::npos,
            "precompiled-only reports source, LibraryReference and store/cache provenance");
        tests.expect(std::get<1>(first) == std::get<1>(second) &&
                std::get<2>(first).empty() && std::get<2>(second).empty(),
            "dependency JSON and fingerprints are reproducible");
        tests.expect(fs::last_write_time(lock) == lockTime &&
                fs::last_write_time(cached) == cacheTime,
            "no-restore/precompiled-only does not mutate lock or restored artifacts");
        std::ostringstream textOutput;
        std::ostringstream textError;
        const std::vector<std::string> textOwned{
            "build", project.string(), "--precompiled-only", "--no-restore",
            "--dependency-report"};
        std::vector<std::string_view> textArguments;
        for (const std::string& value : textOwned) textArguments.push_back(value);
        tests.expect(nativesharp::driver::runProjectCommand(
                textArguments, textOutput, textError, compile) == ExitCode::Success &&
                textOutput.str().find("Dependency provenance:\n") != std::string::npos &&
                textOutput.str().find("store-cache NativeSharp.Test@2.0.0") != std::string::npos,
            "dependency provenance has a deterministic human-readable report");
    }

    {
        const fs::path lock = projectRoot / "NativeSharp.lock.json";
        std::ifstream originalStream(lock, std::ios::binary);
        const std::string original{std::istreambuf_iterator<char>(originalStream), {}};
        originalStream.close();
        const std::string sizeField = "\"size\": 7";
        const auto position = original.find(sizeField);
        tests.expect(position != std::string::npos, "lock size fixture exists");
        if (position != std::string::npos)
        {
            for (const std::string_view invalid : {"7.5", "18446744073709551616", "1e100"})
            {
                std::string altered = original;
                altered.replace(position, sizeField.size(), "\"size\": " + std::string(invalid));
                writeFile(lock, altered);
                std::ostringstream output, error;
                const ExitCode code = run({"build", project.string(), "--no-restore"}, output, error);
                tests.expect(code == ExitCode::CompilationError &&
                        error.str().find("lock file contains an invalid artifact") != std::string::npos,
                    "lock rejects invalid artifact size: " + std::string(invalid));
            }
        }
        writeFile(lock, original);
    }

    {
        const fs::path dependencyRoot = root / "SourceDependency";
        const fs::path dependency = dependencyRoot / "SourceDependency.nsproj";
        writeFile(dependency, "<Project><PropertyGroup><Name>SourceDependency</Name>"
            "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
        writeFile(dependencyRoot / "Api.ns", "public class SourceApi { }\n");
        const fs::path consumerRoot = root / "SourceConsumer";
        const fs::path consumer = consumerRoot / "SourceConsumer.nsproj";
        writeFile(consumer, "<Project><PropertyGroup><Name>SourceConsumer</Name></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\"" +
            dependency.generic_string() + "\"/></ItemGroup></Project>");
        writeFile(consumerRoot / "Main.ns", "public class Program { public static int Main() { return 0; } }\n");
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"build", consumer.string(), "--precompiled-only"}, output, error);
        tests.expect(code == ExitCode::CompilationError &&
                error.str().find("rejected source ProjectReference module 'SourceDependency'") !=
                    std::string::npos,
            "precompiled-only fails closed before compiling a source dependency");
    }

    {
        const fs::path missingRoot = root / "MissingLock";
        const fs::path missing = missingRoot / "MissingLock.nsproj";
        writeFile(missing, "<Project><PropertyGroup><Name>MissingLock</Name></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
        writeFile(missingRoot / "Main.ns", "public class Program { public static int Main() { return 0; } }\n");
        std::ostringstream addOutput;
        std::ostringstream addError;
        tests.expect(run({"add", "package", "NativeSharp.Test", "--version", "2.0.0",
                "--source", package.string(), missing.string()}, addOutput, addError) == ExitCode::Success,
            "missing-lock fixture records a package without restoring it");
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"build", missing.string(), "--no-restore"}, output, error);
        tests.expect(code == ExitCode::CompilationError &&
                error.str().find("--no-restore requires an existing verified lock/store closure") !=
                    std::string::npos && !fs::exists(missingRoot / "NativeSharp.lock.json") &&
                !fs::exists(missingRoot / "obj"),
            "no-restore reports the missing causal artifact and performs no restore mutation");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"list", "package", project.string(), "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::Success && output.str().find("NativeSharp.Test") != std::string::npos,
                     "list package exposes sidecar dependencies through JSON");
    }

    {
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = run({"clean", project.string(), "--output-format=json"}, output, error);
        tests.expect(code == ExitCode::Success && !fs::exists(projectRoot / "obj"),
                     "clean removes only generated project directories");
        tests.expect(fs::is_regular_file(project) && fs::is_regular_file(projectRoot / "Main.ns"),
                     "clean preserves project and source files");
    }

    fs::remove_all(root, cleanupError);
    return tests.result();
}
