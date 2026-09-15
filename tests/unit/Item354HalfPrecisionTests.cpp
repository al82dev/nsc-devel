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
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc); stream << text;
}
ExitCode run(std::span<const std::string_view> args, std::string& output, std::string& error)
{
    std::ostringstream out; std::ostringstream err;
    const ExitCode code = Driver{}.run(args, out, err);
    output = out.str(); error = err.str(); return code;
}
bool execute(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    const std::string projectText = project.string(); const std::string executableText = executable.string();
    const std::array<std::string_view, 5> args = {"--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    std::string out; std::string err;
    if (run(args, out, err) != ExitCode::Success) { diagnostic = err + out; return false; }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    diagnostic = result == 42 ? std::string{} : "process result " + std::to_string(result);
    return result == 42;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "item354_half";
    std::error_code filesystemError; std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
    writeFile(root / "Half.nsproj", "<Project><PropertyGroup><Name>HalfProbe</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\"" + coreProject + "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;
public class Program
{
    public static int Main()
    {
        static_assert(sizeof(Half) == 2u, "binary16 ABI");
        Half h = Half.FromFloat((float)13.5);
        float value = Half.ToFloat(h);
        return (int)(value * (float)3) + 2;
    }
}
)NS");
    std::string diagnostic;
    const bool o0 = execute(root / "Half.nsproj", root / "HalfO0.exe", "-O0", diagnostic);
    tests.expect(o0, "half conversion executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2 = execute(root / "Half.nsproj", root / "HalfO2.exe", "-O2", diagnostic);
    tests.expect(o2, "half conversion executes at O2 (" + diagnostic + ")");
    const std::string projectText = (root / "Half.nsproj").string();
    const std::array<std::string_view, 2> args = {"--dump-project-oop-llvm", projectText};
    std::string out; std::string err; const bool dumped = run(args, out, err) == ExitCode::Success;
    // LLVM is allowed to fold the literal's fptrunc to a half constant before printing the module;
    // the half-typed store still proves the 16-bit operation, while the dynamic load keeps fpext.
    tests.expect(dumped && out.find("store half") != std::string::npos &&
        out.find("fpext half") != std::string::npos, "half uses native LLVM binary16 values (" + err + ")");
    const int result = tests.result(); if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
