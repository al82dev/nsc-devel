#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

std::string compile(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    nativesharp::ExitCode& code,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 6> arguments = {
        "-O2", "--incremental-link", "--emit-project-oop-exe",
        projectText, "-o", executableText};
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    code = driver.run(arguments, output, error);
    diagnostic = error.str();
    return output.str();
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item392_incremental_link_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path project = root / "Incremental.nsproj";
    const std::filesystem::path source = root / "Program.ns";
    const std::filesystem::path executable = root / "Incremental.exe";
    writeFile(project,
        "<Project><PropertyGroup><Name>Incremental</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(source, "class Program { public static int Main() { return 41; } }");

    nativesharp::ExitCode code{};
    std::string error;
    const std::string cold = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            cold.find("incremental-link: miss") != std::string::npos,
        "first incremental link produces and records the PE: " + error);
    tests.expect(std::filesystem::is_regular_file(executable.string() + ".nslinc"),
        "incremental link publishes its versioned sidecar state");

    const std::string warm = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            warm.find("incremental-link: hit") != std::string::npos,
        "unchanged inputs reuse the verified linked PE: " + error);

    writeFile(source, "class Program { public static int Main() { return 42; } }");
    const std::string changed = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            changed.find("incremental-link: miss") != std::string::npos,
        "source/object change invalidates incremental link state: " + error);
    if (code == nativesharp::ExitCode::Success)
    {
        const std::string command = "\"" + executable.string() + "\"";
        tests.expect(std::system(command.c_str()) == 42,
            "relinked executable contains the changed program");
    }
    return tests.resultKeeping(root);
}
