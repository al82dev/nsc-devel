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

ExitCode run(std::span<const std::string_view> arguments, std::string& outText, std::string& errText)
{
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = Driver{}.run(arguments, out, err);
    outText = out.str();
    errText = err.str();
    return result;
}

bool compileAndRun(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    std::string out;
    std::string err;
    if (run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err + out;
        return false;
    }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    if (result != 42)
    {
        diagnostic = "process result " + std::to_string(result) + ", expected 42";
        return false;
    }
    return true;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item348_floating_vectors_256";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj")
            .generic_string();
    writeFile(root / "FloatingVectors.nsproj",
        "<Project><PropertyGroup><Name>FloatingVectors</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/>"
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;

public class Program
{
    public static int Main()
    {
        Float32x8 left;
        left.X = (float)10; left.Y = (float)2; left.Z = (float)3; left.W = (float)4;
        left.A = (float)5; left.B = (float)6; left.C = (float)7; left.D = (float)8;
        Float32x8 right;
        right.X = (float)20; right.Y = (float)5; right.Z = (float)6; right.W = (float)7;
        right.A = (float)8; right.B = (float)9; right.C = (float)10; right.D = (float)11;
        Float32x8 sum = Float32x8.Add(left, right);
        Float64x4 wideLeft;
        wideLeft.X = (double)8; wideLeft.Y = (double)3;
        wideLeft.Z = (double)2; wideLeft.W = (double)1;
        Float64x4 wideRight;
        wideRight.X = (double)9; wideRight.Y = (double)4;
        wideRight.Z = (double)3; wideRight.W = (double)2;
        Float64x4 product = Float64x4.Multiply(wideLeft, wideRight);
        return (int)(sum.X + product.Y);
    }
}
)NS");

    std::string diagnostic;
    const bool o0Passed = compileAndRun(
        root / "FloatingVectors.nsproj", root / "FloatingVectorsO0.exe", "-O0", diagnostic);
    tests.expect(o0Passed, "256-bit floating vectors execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2Passed = compileAndRun(
        root / "FloatingVectors.nsproj", root / "FloatingVectorsO2.exe", "-O2", diagnostic);
    tests.expect(o2Passed, "256-bit floating vectors execute at O2 (" + diagnostic + ")");

    const std::string projectText = (root / "FloatingVectors.nsproj").string();
    const std::array<std::string_view, 2> irArguments = {
        "--dump-project-oop-llvm", projectText};
    std::string out;
    std::string err;
    const bool dumped = run(irArguments, out, err) == ExitCode::Success;
    tests.expect(dumped && out.find("fadd <8 x float>") != std::string::npos,
        "Float32x8.Add lowers to one eight-lane LLVM operation (" + err + ")");
    tests.expect(dumped && out.find("fmul <4 x double>") != std::string::npos,
        "Float64x4.Multiply lowers to one four-lane LLVM operation");
    tests.expect(dumped && out.find("simd.fp256.result") != std::string::npos,
        "256-bit floating SIMD explicitly bridges back to the public carrier");

    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
