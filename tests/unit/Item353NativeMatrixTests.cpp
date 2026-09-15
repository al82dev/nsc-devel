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
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
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
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
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
    const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "item353_native_matrices";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
    writeFile(root / "NativeMatrices.nsproj",
        "<Project><PropertyGroup><Name>NativeMatrices</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;
public class Program
{
    public static int Main()
    {
        static_assert(sizeof(Float32x3x3) == 36u, "float3x3 ABI");
        static_assert(sizeof(Float32x4x4) == 64u, "float4x4 ABI");
        Float32x4x4 m;
        m.C0.X = (float)1; m.C0.Y = (float)0; m.C0.Z = (float)0; m.C0.W = (float)0;
        m.C1.X = (float)0; m.C1.Y = (float)2; m.C1.Z = (float)0; m.C1.W = (float)0;
        m.C2.X = (float)0; m.C2.Y = (float)0; m.C2.Z = (float)3; m.C2.W = (float)0;
        m.C3.X = (float)10; m.C3.Y = (float)20; m.C3.Z = (float)30; m.C3.W = (float)1;
        Float32x4 v; v.X = (float)2; v.Y = (float)3; v.Z = (float)4; v.W = (float)1;
        Float32x4 r = Float32x4x4.Transform(m, v);
        return (int)r.X + (int)r.Y + (int)r.W + (int)sizeof(Float32x3x3) - 33;
    }
}
)NS");
    std::string diagnostic;
    const bool o0 = execute(root / "NativeMatrices.nsproj", root / "NativeMatricesO0.exe", "-O0", diagnostic);
    tests.expect(o0, "native matrices preserve layout and execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2 = execute(root / "NativeMatrices.nsproj", root / "NativeMatricesO2.exe", "-O2", diagnostic);
    tests.expect(o2, "native matrices preserve layout and execute at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
