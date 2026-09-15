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

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    std::string& diagnostic)
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
        std::filesystem::path(NSC_BINARY_DIR) / "item347_integer_vectors_128";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj")
            .generic_string();
    writeFile(root / "IntegerVectors.nsproj",
        "<Project><PropertyGroup><Name>IntegerVectors</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/>"
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;

public class Program
{
    public static int Main()
    {
        Int32x4 left;
        left.X = 10; left.Y = 2; left.Z = 3; left.W = 4;
        Int32x4 right;
        right.X = 20; right.Y = 5; right.Z = 6; right.W = 7;
        Int32x4 sum = Int32x4.Add(left, right);
        Int64x2 wideLeft;
        wideLeft.X = (long)8; wideLeft.Y = (long)3;
        Int64x2 wideRight;
        wideRight.X = (long)9; wideRight.Y = (long)4;
        Int64x2 product = Int64x2.Multiply(wideLeft, wideRight);
        return sum.X + (int)product.Y;
    }
}
)NS");

    std::string diagnostic;
    const bool o0Passed = compileAndRun(
        root / "IntegerVectors.nsproj", root / "IntegerVectorsO0.exe", "-O0", diagnostic);
    tests.expect(o0Passed,
        "128-bit integer vectors execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2Passed = compileAndRun(
        root / "IntegerVectors.nsproj", root / "IntegerVectorsO2.exe", "-O2", diagnostic);
    tests.expect(o2Passed,
        "128-bit integer vectors execute at O2 (" + diagnostic + ")");

    const std::string projectText = (root / "IntegerVectors.nsproj").string();
    const std::array<std::string_view, 2> irArguments = {
        "--dump-project-oop-llvm", projectText};
    std::string out;
    std::string err;
    const bool dumped = run(irArguments, out, err) == ExitCode::Success;
    tests.expect(dumped && out.find("add <4 x i32>") != std::string::npos,
        "Int32x4.Add lowers to one four-lane LLVM instruction (" + err + ")");
    tests.expect(dumped && out.find("mul <2 x i64>") != std::string::npos,
        "Int64x2.Multiply lowers to one two-lane LLVM instruction");
    tests.expect(dumped && out.find("simd.int.result") != std::string::npos,
        "integer SIMD preserves the public aggregate carrier through an explicit bridge");

    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
