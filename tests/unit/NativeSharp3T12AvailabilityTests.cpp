#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const std::filesystem::path directory = root / name;
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Exe</OutputType><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    using nativesharp::ExitCode;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f05_availability";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path available = writeProject(root, "Available", R"NS(
#[target("windows")]
#[architecture("x64")]
#[since(3)]
public class PlatformValue { public static int Read() { return 42; } }
public class Program
{
    public static int Main() { PlatformValue value; return PlatformValue.Read(); }
}
)NS");
    const std::string availableProject = available.string();
    const std::string availableExe = (available.parent_path() / "Available.exe").string();
    const std::array<std::string_view, 5> emit = {
        "--emit-project-oop-exe", availableProject, "-O0", "-o", availableExe};
    std::string output;
    std::string error;
    const bool compiled = runDriver(emit, output, error) == ExitCode::Success;
    tests.expect(compiled, "matching edition/target/architecture compiles at O0 (" + error + output + ")");
    if (compiled)
        tests.expect(std::system(("\"" + availableExe + "\"").c_str()) == 42,
            "availability-qualified executable links with NSLink and returns 42");

    const std::filesystem::path future = writeProject(root, "Future", R"NS(
#[since(4, "upgrade the project edition")]
public class FutureValue { }
public class Program { public static int Main() { FutureValue value; return 0; } }
)NS");
    const std::string futureProject = future.string();
    const std::array<std::string_view, 3> futureCheck = {
        "--check-project-oop-hir", futureProject, "-O0"};
    output.clear(); error.clear();
    tests.expect(runDriver(futureCheck, output, error) != ExitCode::Success &&
            (error + output).find("available since language edition 4") != std::string::npos,
        "since rejects a use before its normative edition (" + error + output + ")");

    const std::filesystem::path removed = writeProject(root, "Removed", R"NS(
#[removed(3, "the old API has been retired")]
public class OldValue { }
public class Program { public static int Main() { OldValue value; return 0; } }
)NS");
    const std::string removedProject = removed.string();
    const std::array<std::string_view, 3> removedCheck = {
        "--check-project-oop-hir", removedProject, "-O0"};
    output.clear(); error.clear();
    tests.expect(runDriver(removedCheck, output, error) != ExitCode::Success &&
            (error + output).find("was removed in language edition 3") != std::string::npos,
        "removed rejects a use at and after its normative edition (" + error + output + ")");

    const std::filesystem::path wrongArch = writeProject(root, "WrongArch", R"NS(
#[architecture("arm64")]
public class ArmValue { }
public class Program { public static int Main() { ArmValue value; return 0; } }
)NS");
    const std::string wrongArchProject = wrongArch.string();
    const std::array<std::string_view, 3> archCheck = {
        "--check-project-oop-hir", wrongArchProject, "-O0"};
    output.clear(); error.clear();
    tests.expect(runDriver(archCheck, output, error) != ExitCode::Success &&
            (error + output).find("unavailable for architecture x64") != std::string::npos,
        "architecture guard rejects a non-x64 declaration use (" + error + output + ")");

    const std::filesystem::path deprecated = writeProject(root, "Deprecated", R"NS(
public class Legacy
{
    #[deprecated("legacy call", "Modern.Read")]
    public static int Read() { return 42; }
}
public class Program { public static int Main() { return Legacy.Read(); } }
)NS");
    const std::string deprecatedProject = deprecated.string();
    nativesharp::frontend::ProjectCompilation deprecatedCompilation;
    error.clear();
    const bool deprecatedOk = deprecatedCompilation.compileProjectObjectLayouts(
        deprecatedProject, error);
    const auto& deprecatedDiagnostics = deprecatedCompilation.diagnostics().diagnostics();
    tests.expect(deprecatedOk && std::ranges::any_of(deprecatedDiagnostics, [](const auto& diagnostic)
        {
            return diagnostic.message().find(
                "is deprecated: legacy call; use 'Modern.Read'") != std::string::npos;
        }), "deprecated emits a structured warning with replacement (" + error + ")");

    const std::filesystem::path suppressed = writeProject(root, "Suppressed", R"NS(
public class Legacy
{
    #[deprecated("legacy call", "Modern.Read")]
    public static int Read() { return 42; }
}
#[allow("deprecated")]
public class Program { public static int Main() { return Legacy.Read(); } }
)NS");
    const std::string suppressedProject = suppressed.string();
    nativesharp::frontend::ProjectCompilation suppressedCompilation;
    error.clear();
    const bool suppressedOk = suppressedCompilation.compileProjectObjectLayouts(
        suppressedProject, error);
    const auto& suppressedDiagnostics = suppressedCompilation.diagnostics().diagnostics();
    tests.expect(suppressedOk && std::ranges::none_of(suppressedDiagnostics, [](const auto& diagnostic)
        {
            return diagnostic.message().find("is deprecated") != std::string::npos;
        }), "allow(deprecated) suppresses the warning for the enclosing declaration (" + error + ")");

    return tests.resultKeeping(root);
}
