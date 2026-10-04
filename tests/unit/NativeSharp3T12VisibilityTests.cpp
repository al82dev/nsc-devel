#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/ModuleSemantics.h"

#include "../support/TestContext.h"

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

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view package,
    std::string_view output,
    std::string_view source,
    std::string_view items = {})
{
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name><Package>" +
        std::string(package) + "</Package><OutputType>" + std::string(output) +
        "</OutputType><Language>3</Language></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />" + std::string(items) + "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
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
    using namespace nativesharp::language;
    nativesharp::tests::TestContext tests;

    tests.expect(!canAccess(Visibility::Private, true, true, false) &&
            canAccess(Visibility::Private, false, false, false, true) &&
            canAccess(Visibility::Module, true, false, false) &&
            canAccess(Visibility::Package, false, true, false) &&
            canAccess(Visibility::Module, false, false, true) &&
            canAccess(Visibility::Public, false, false, false),
        "private, module, package, friend, and public access are distinct");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f03_visibility";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path producer = root / "Producer";
    writeProject(producer, "Producer", "Suite", "Library", R"NS(
namespace Contracts
{
    package class PackageType { public static int Value() { return 40; } }
    module class ModuleType { public static int Value() { return 2; } }
    public class PublicType { public static int Value() { return 42; } }
}
)NS", "<Friend Include=\"FriendApp\" />");

    const std::string reference = "<ProjectReference Include=\"../Producer/Producer.nsproj\" />";
    const std::filesystem::path sibling = root / "Sibling";
    writeProject(sibling, "Sibling", "Suite", "Exe", R"NS(
using Contracts;
public class Program { public static int Main() { return PackageType.Value() + 2; } }
)NS", reference);
    const std::filesystem::path friendApp = root / "Friend";
    writeProject(friendApp, "FriendApp", "External", "Exe", R"NS(
using Contracts;
public class Program { public static int Main() { return ModuleType.Value() + 40; } }
)NS", reference);
    const std::filesystem::path outsider = root / "Outsider";
    writeProject(outsider, "Outsider", "External", "Exe", R"NS(
using Contracts;
public class Program { public static int Main() { PackageType value; return 0; } }
)NS", reference);

    const auto emit = [&](const std::filesystem::path& projectDirectory, std::string_view name)
    {
        const std::string project = (projectDirectory / (std::string(name) + ".nsproj")).string();
        const std::string executable = (projectDirectory / (std::string(name) + ".exe")).string();
        const std::array<std::string_view, 5> arguments = {
            "--emit-project-oop-exe", project, "-O0", "-o", executable};
        std::string outputText;
        std::string errorText;
        const bool compiled = runDriver(arguments, outputText, errorText) == ExitCode::Success;
        tests.expect(compiled, std::string(name) + " compiles at O0 (" + errorText + outputText + ")");
        if (compiled)
            tests.expect(std::system(("\"" + executable + "\"").c_str()) == 42,
                std::string(name) + " executes the expected visible API");
    };
    emit(sibling, "Sibling");
    emit(friendApp, "FriendApp");

    const std::string outsiderProject = (outsider / "Outsider.nsproj").string();
    const std::array<std::string_view, 3> check = {
        "--check-project-oop-hir", outsiderProject, "-O0"};
    std::string output;
    std::string error;
    const bool rejected = runDriver(check, output, error) != ExitCode::Success;
    tests.expect(rejected && (error + output).find("different Package identity") != std::string::npos,
        "package visibility rejects an external package with an accessibility diagnostic (" +
            error + output + ")");

    return tests.resultKeeping(root);
}
