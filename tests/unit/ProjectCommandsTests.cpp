#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ProjectCommands.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

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
        const std::size_t envelope = text.find("\"schemaVersion\":1");
        tests.expect(code == ExitCode::Success && sawProgress && error.str().empty(),
                     "build accepts and forwards JSON progress selection");
        tests.expect(progress != std::string::npos && envelope > progress,
                     "JSON progress streams before the project result envelope");
        tests.expect(text.find("\"stdout\":\"technical output\\n\"") != std::string::npos &&
                         text.find("nativesharp-progress-event", progress + 1U) == std::string::npos,
                     "project envelope retains technical output without duplicating progress events");
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
