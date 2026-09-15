#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"

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
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using nativesharp::ExitCode;
    using nativesharp::driver::Driver;
    using nativesharp::llvm_backend::LinkTimeOptimizationMode;
    using nativesharp::llvm_backend::linkTimeOptimizationModeName;

    tests.expect(linkTimeOptimizationModeName(LinkTimeOptimizationMode::None) == "none" &&
            linkTimeOptimizationModeName(LinkTimeOptimizationMode::Full) == "full",
        "LTO modes have stable diagnostic identities");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item390_lto_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Lto.nsproj",
        "<Project><PropertyGroup><Name>Lto</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Program.ns", R"NS(
class Program
{
    static int A(int x) { return x + 1; }
    static int B(int x) { return A(x) + 1; }
    static int C(int x) { return B(x) + 1; }
    static int D(int x) { return C(x) + 1; }
    static int E(int x) { return D(x) + 1; }
    static int F(int x) { return E(x) + 1; }
    static int G(int x) { return F(x) + 1; }
    public static int Main() { return G(35); }
}
)NS");

    const std::string project = (root / "Lto.nsproj").string();
    const std::string executable = (root / "Lto.exe").string();
    const std::array<std::string_view, 7> arguments = {
        "-O2", "-j4", "-flto", "--emit-project-oop-exe", project, "-o", executable};
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode result = driver.run(arguments, output, error);
    tests.expect(result == ExitCode::Success,
        "full LTO project emits successfully: " + error.str());
    tests.expect(output.str().find("link-time-optimization: full") != std::string::npos &&
            output.str().find("code-units: 1") != std::string::npos,
        "full LTO keeps the complete program in one optimized LLVM module");
    if (result == ExitCode::Success)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 42,
            "full-LTO executable preserves native program behavior");
    }

    return tests.resultKeeping(root);
}
