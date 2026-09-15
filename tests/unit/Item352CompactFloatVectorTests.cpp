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
    std::ostringstream out;
    std::ostringstream err;
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
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item352_compact_float_vectors";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj")
            .generic_string();
    writeFile(root / "CompactFloat.nsproj",
        "<Project><PropertyGroup><Name>CompactFloat</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;
public class Program
{
    public static int Main()
    {
        Float32x2 a; a.X = (float)10; a.Y = (float)2;
        Float32x2 b; b.X = (float)20; b.Y = (float)5;
        Float32x2 sum = Float32x2.Add(a, b);
        Float32x3 c; c.X = (float)2; c.Y = (float)3; c.Z = (float)3;
        Float32x3 d; d.X = (float)4; d.Y = (float)5; d.Z = (float)4;
        Float32x3 product = Float32x3.Multiply(c, d);
        return (int)(sum.X + product.Z);
    }
}
)NS");
    std::string diagnostic;
    const bool o0 = execute(root / "CompactFloat.nsproj", root / "CompactFloatO0.exe", "-O0", diagnostic);
    tests.expect(o0, "float2/float3 execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2 = execute(root / "CompactFloat.nsproj", root / "CompactFloatO2.exe", "-O2", diagnostic);
    tests.expect(o2, "float2/float3 execute at O2 (" + diagnostic + ")");
    const std::string projectText = (root / "CompactFloat.nsproj").string();
    const std::array<std::string_view, 2> irArgs = {"--dump-project-oop-llvm", projectText};
    std::string out; std::string err;
    const bool dumped = run(irArgs, out, err) == ExitCode::Success;
    tests.expect(dumped && out.find("fadd <2 x float>") != std::string::npos,
        "float2 uses an exact two-lane LLVM vector (" + err + ")");
    tests.expect(dumped && out.find("fmul <3 x float>") != std::string::npos,
        "float3 uses an exact three-lane LLVM vector without public padding");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
