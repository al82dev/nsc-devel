#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ProjectCommands.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
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
        writeFile(projectRoot / "Content/title.txt", "NativeSharp");
        writeFile(projectRoot / "Content/shaders/default.vert", "shader");
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string> owned{"build", project.string(), "--no-restore", "--output-format=json"};
        std::vector<std::string_view> arguments;
        for (const std::string& value : owned) arguments.push_back(value);
        const auto compiler = [](std::span<const std::string_view> compilerArguments,
                                 std::ostream&, std::ostream&) {
            for (std::size_t index = 0; index + 1 < compilerArguments.size(); ++index)
            {
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
