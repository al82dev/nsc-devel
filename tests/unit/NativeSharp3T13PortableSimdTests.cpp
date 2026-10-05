#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Language/AbiSemantics.h"

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
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    const auto project = directory / (std::string(name) + ".nsproj");
    writeFile(project, "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

bool compileAndRun(const std::filesystem::path& project, std::string& diagnostic)
{
    const auto executable = project.parent_path() / "Simd.exe";
    const std::string p = project.string();
    const std::string e = executable.string();
    const std::array<std::string_view, 5> args = {"--emit-project-oop-exe", p, "-O0", "-o", e};
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const auto result = driver.run(args, out, err);
    diagnostic = err.str() + out.str();
    if (result != nativesharp::ExitCode::Success)
    {
        diagnostic += " driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + e + "\"").c_str());
    if (observed == 42) return true;
    diagnostic += " process exit code " + std::to_string(observed);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    _putenv_s("NSC_LINKER", "A:\\nslink-devel\\build-debug\\bin\\nslink.exe");
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    std::string error;
    const auto f32x8 = nativesharp::language::portableSimdType("builtin:float", 8, false, error);
    tests.expect(f32x8.has_value() &&
            nativesharp::language::portableSimdTypeIdentity(*f32x8) == "simd<builtin:float,8>",
        "generic lane/count identity is stable");
    tests.expect(!nativesharp::language::portableSimdType("builtin:bool", 4, false, error).has_value(),
        "unsupported lane types are rejected");
    tests.expect(nativesharp::language::validateSimdShuffle(4, "3,2,1,0", error) &&
            !nativesharp::language::validateSimdShuffle(4, "0,1,2,4", error),
        "shuffle indices are constant, complete, and in range");
    tests.expect(nativesharp::language::simdMaskLane(true, 32) == 0xffffffffULL &&
            nativesharp::language::simdMaskLane(false, 32) == 0,
        "mask lanes use defined all-bits/zero representation");

    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f07-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const auto project = writeProject(root, "PortableSimd", R"NS(
namespace PortableSimd
{
    public class Program
    {
        public static int Main()
        {
            simd<int, 4> left = default(simd<int, 4>);
            simd<int, 4> right = default(simd<int, 4>);
            simd<int, 4> sum = left + right;
            mask<int, 4> equal = left == right;
            mask<int, 4> selected = equal & equal;
            return 42;
        }
    }
}
)NS");
    std::string diagnostic;
    const bool ran = compileAndRun(project, diagnostic);
    tests.expect(ran,
        "portable SIMD arithmetic and masks execute at O0 through NSLink (" + diagnostic + ")");
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
