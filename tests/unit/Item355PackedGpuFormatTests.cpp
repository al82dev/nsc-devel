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
using nativesharp::ExitCode; using nativesharp::driver::Driver; using nativesharp::tests::TestContext;
void writeFile(const std::filesystem::path& path, std::string_view text)
{ std::filesystem::create_directories(path.parent_path()); std::ofstream stream(path); stream << text; }
bool execute(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    std::ostringstream out; std::ostringstream err; const std::string p = project.string(); const std::string e = executable.string();
    const std::array<std::string_view, 5> args = {"--emit-project-oop-exe", p, optimization, "-o", e};
    if (Driver{}.run(args, out, err) != ExitCode::Success) { diagnostic = err.str() + out.str(); return false; }
    const int result = std::system(("\"" + e + "\"").c_str());
    diagnostic = result == 42 ? std::string{} : "process result " + std::to_string(result); return result == 42;
}
}

int main()
{
    TestContext tests; const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "item355_packed_gpu";
    std::error_code error; std::filesystem::remove_all(root, error); std::filesystem::create_directories(root, error);
    const std::string core = (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
    writeFile(root / "PackedGpu.nsproj", "<Project><PropertyGroup><Name>PackedGpu</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\"" + core + "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Numerics;
public class Program
{
    public static int Main()
    {
        static_assert(sizeof(Rgba8Unorm) == 4u, "RGBA8 ABI");
        static_assert(sizeof(Rgb10A2Unorm) == 4u, "RGB10A2 ABI");
        Rgba8Unorm rgba = Rgba8Unorm.Pack((byte)1, (byte)2, (byte)3, (byte)4);
        Rgb10A2Unorm rgb10 = Rgb10A2Unorm.Pack((ushort)10, (ushort)20, (ushort)8, (byte)1);
        return (int)Rgba8Unorm.Alpha(rgba) + (int)Rgb10A2Unorm.Red(rgb10) +
            (int)Rgb10A2Unorm.Alpha(rgb10) + 27;
    }
}
)NS");
    std::string diagnostic;
    tests.expect(execute(root / "PackedGpu.nsproj", root / "PackedGpuO0.exe", "-O0", diagnostic),
        "packed GPU formats execute at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(execute(root / "PackedGpu.nsproj", root / "PackedGpuO2.exe", "-O2", diagnostic),
        "packed GPU formats execute at O2 (" + diagnostic + ")");
    const int result = tests.result(); if (result == 0) std::filesystem::remove_all(root, error); return result;
}
