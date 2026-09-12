#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::driver::Driver;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage68_simd";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    writeFile(root / "Simd.nsproj",
        "<Project><PropertyGroup><Name>Simd</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Simd.ns", R"NS(
namespace NativeSharp.Core.Numerics
{
    public struct Float32x4
    {
        public float X; public float Y; public float Z; public float W;
        public static Float32x4 Add(Float32x4 a, Float32x4 b) { return default(Float32x4); }
        public static Float32x4 Subtract(Float32x4 a, Float32x4 b) { return default(Float32x4); }
        public static Float32x4 Multiply(Float32x4 a, Float32x4 b) { return default(Float32x4); }
    }
}
namespace Simd
{
    using NativeSharp.Core.Numerics;
    public class Program
    {
        public static Float32x4 Apply(Float32x4 a, Float32x4 b)
        {
            return Float32x4.Multiply(Float32x4.Add(a, b), Float32x4.Subtract(a, b));
        }
    }
}
)NS");

    std::ostringstream output;
    std::ostringstream error;
    const std::string project = (root / "Simd.nsproj").string();
    const std::vector<std::string_view> args = {"--dump-project-oop-llvm", project, "-O0"};
    const ExitCode code = Driver{}.run(args, output, error);
    tests.expect(code == ExitCode::Success, "Float32x4 program reaches verified LLVM IR at O0");
    tests.expect(error.str().empty(), "valid Float32x4 program keeps stderr empty");
    tests.expect(output.str().find("<4 x float>") != std::string::npos,
        "Float32x4 lowering uses a 128-bit LLVM vector");
    tests.expect(output.str().find("fadd <4 x float>") != std::string::npos,
        "Float32x4.Add emits vector fadd");
    tests.expect(output.str().find("fsub <4 x float>") != std::string::npos,
        "Float32x4.Subtract emits vector fsub");
    tests.expect(output.str().find("fmul <4 x float>") != std::string::npos,
        "Float32x4.Multiply emits vector fmul");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
