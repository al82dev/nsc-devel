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
    output = out.str();
    error = err.str();
    return code;
}

bool execute(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    std::string out;
    std::string err;
    if (run(args, out, err) != ExitCode::Success)
    {
        diagnostic = err + out;
        return false;
    }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    diagnostic = result == 42 ? std::string{} : "process result " + std::to_string(result);
    return result == 42;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item349_integer_vectors_256";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string coreProject =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj")
            .generic_string();
    writeFile(root / "IntegerVectors256.nsproj",
        "<Project><PropertyGroup><Name>IntegerVectors256</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject + "\"/>"
        "</ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;

public class Program
{
    public static int Main()
    {
        Int32x8 left;
        left.X = 10; left.Y = 2; left.Z = 3; left.W = 4;
        left.A = 5; left.B = 6; left.C = 7; left.D = 8;
        Int32x8 right;
        right.X = 20; right.Y = 5; right.Z = 6; right.W = 7;
        right.A = 8; right.B = 9; right.C = 10; right.D = 11;
        Int32x8 sum = Int32x8.Add(left, right);
        Int64x4 wideLeft;
        wideLeft.X = (long)8; wideLeft.Y = (long)3;
        wideLeft.Z = (long)2; wideLeft.W = (long)1;
        Int64x4 wideRight;
        wideRight.X = (long)9; wideRight.Y = (long)4;
        wideRight.Z = (long)3; wideRight.W = (long)2;
        Int64x4 product = Int64x4.Multiply(wideLeft, wideRight);
        return sum.X + (int)product.Y;
    }
}
)NS");

    std::string diagnostic;
    const bool o0 = execute(root / "IntegerVectors256.nsproj",
        root / "IntegerVectors256O0.exe", "-O0", diagnostic);
    tests.expect(o0, "256-bit integer vectors execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2 = execute(root / "IntegerVectors256.nsproj",
        root / "IntegerVectors256O2.exe", "-O2", diagnostic);
    tests.expect(o2, "256-bit integer vectors execute at O2 (" + diagnostic + ")");

    const std::string projectText = (root / "IntegerVectors256.nsproj").string();
    const std::array<std::string_view, 2> irArgs = {"--dump-project-oop-llvm", projectText};
    std::string out;
    std::string err;
    const bool dumped = run(irArgs, out, err) == ExitCode::Success;
    tests.expect(dumped && out.find("add <8 x i32>") != std::string::npos,
        "Int32x8.Add emits an eight-lane LLVM add (" + err + ")");
    tests.expect(dumped && out.find("mul <4 x i64>") != std::string::npos,
        "Int64x4.Multiply emits a four-lane LLVM multiply");
    tests.expect(dumped && out.find("simd.int256.result") != std::string::npos,
        "256-bit integer SIMD bridges back to its public carrier");

    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
