#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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

std::string valueAfter(std::string_view output, std::string_view prefix)
{
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return {};
    const std::size_t valueBegin = begin + prefix.size();
    const std::size_t end = output.find('\n', valueBegin);
    return std::string(output.substr(valueBegin, end - valueBegin));
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    tests.expect(nativesharp::llvm_backend::linkTimeOptimizationModeName(
            nativesharp::llvm_backend::LinkTimeOptimizationMode::Thin) == "thin",
        "ThinLTO has a stable diagnostic identity");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item391_thinlto_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Thin.nsproj",
        "<Project><PropertyGroup><Name>Thin</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Program.ns", R"NS(
class Program
{
    static noinline int A(int x) { return x + 1; }
    static noinline int B(int x) { return A(x) + 1; }
    static noinline int C(int x) { return B(x) + 1; }
    static noinline int D(int x) { return C(x) + 1; }
    static noinline int E(int x) { return D(x) + 1; }
    static noinline int F(int x) { return E(x) + 1; }
    static noinline int G(int x) { return F(x) + 1; }
    public static int Main() { return G(35); }
}
)NS");

    const std::string project = (root / "Thin.nsproj").string();
    std::string firstIndex;
    for (int run = 0; run < 2; ++run)
    {
        const std::string executable = (root / ("Thin" + std::to_string(run) + ".exe")).string();
        const std::array<std::string_view, 7> arguments = {
            "-O2", "-j4", "-flto=thin", "--emit-project-oop-exe", project, "-o", executable};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const nativesharp::ExitCode result = driver.run(arguments, output, error);
        tests.expect(result == nativesharp::ExitCode::Success,
            "ThinLTO project emits successfully: " + error.str());
        tests.expect(error.str().find("nsc: warning: -flto=thin") == std::string::npos,
            "ThinLTO at -O2 runs real backends and does not warn");
        const std::string text = output.str();
        const std::string index = valueAfter(text, "thinlto-index: ");
        tests.expect(text.find("link-time-optimization: thin") != std::string::npos &&
                text.find("thinlto-summaries: 1") == std::string::npos &&
                text.find("thinlto-imports: ") != std::string::npos &&
                index.size() == 64,
            "ThinLTO builds multiple indexed summaries");
        if (run == 0) firstIndex = index;
        else tests.expect(index == firstIndex,
            "ThinLTO global index is reproducible across parallel builds");
        if (result == nativesharp::ExitCode::Success)
        {
            const std::string command = "\"" + executable + "\"";
            tests.expect(std::system(command.c_str()) == 42,
                "ThinLTO executable preserves native program behavior");
        }
    }
    {
        // The second build with the same artifact cache restores every ThinLTO backend (item 45).
        // El segundo build con el mismo cache de artefactos restaura cada backend ThinLTO (punto 45).
        const std::string cache = "--artifact-cache=" + (root / "cache").string();
        std::string firstObjectIndex;
        for (int run = 0; run < 2; ++run)
        {
            const std::string executable = (root / ("Cached" + std::to_string(run) + ".exe")).string();
            const std::array<std::string_view, 8> arguments = {
                "-O2", "-j4", "-flto=thin", cache, "--emit-project-oop-exe", project, "-o", executable};
            nativesharp::driver::Driver driver;
            std::ostringstream output;
            std::ostringstream error;
            const nativesharp::ExitCode result = driver.run(arguments, output, error);
            const std::string text = output.str();
            const std::string cached = valueAfter(text, "thinlto-cached-backends: ");
            const std::string summaries = valueAfter(text, "thinlto-summaries: ");
            tests.expect(result == nativesharp::ExitCode::Success &&
                    (run == 0 ? cached == "0" : (!cached.empty() && cached == summaries)),
                run == 0 ? "the first cached ThinLTO build runs every backend: " + error.str()
                         : "the second cached ThinLTO build restores every backend: " + error.str());
            const std::string index = valueAfter(text, "thinlto-index: ");
            if (run == 0) firstObjectIndex = index;
            else tests.expect(index == firstObjectIndex && index.size() == 64,
                "restored ThinLTO backends produce the same objects");
            if (result == nativesharp::ExitCode::Success)
            {
                const std::string command = "\"" + executable + "\"";
                tests.expect(std::system(command.c_str()) == 42,
                    "an executable linked from cached ThinLTO backends preserves program behavior");
            }
        }
    }
    {
        // Two identical -O2 -g ThinLTO builds write the same executable and PDB: the backends record
        // no object path in CodeView. Dos builds ThinLTO -O2 -g identicos escriben el mismo ejecutable
        // y PDB: los backends no guardan ruta de objeto en CodeView.
        std::string executables[2];
        std::string pdbs[2];
        for (int run = 0; run < 2; ++run)
        {
            const std::filesystem::path directory = root / ("debug" + std::to_string(run));
            std::filesystem::create_directories(directory);
            const std::string executable = (directory / "Thin.exe").string();
            const std::array<std::string_view, 9> arguments = {"-O2", "-g", "-j4", "-flto=thin",
                "--no-artifact-cache", "--emit-project-oop-exe", project, "-o", executable};
            nativesharp::driver::Driver driver;
            std::ostringstream output;
            std::ostringstream error;
            tests.expect(driver.run(arguments, output, error) == nativesharp::ExitCode::Success,
                "a -O2 -g ThinLTO build succeeds: " + error.str());
            std::ifstream executableStream(directory / "Thin.exe", std::ios::binary);
            executables[run].assign(std::istreambuf_iterator<char>(executableStream), {});
            std::ifstream pdbStream(directory / "Thin.pdb", std::ios::binary);
            pdbs[run].assign(std::istreambuf_iterator<char>(pdbStream), {});
        }
        tests.expect(!executables[0].empty() && executables[0] == executables[1] &&
                !pdbs[0].empty() && pdbs[0] == pdbs[1],
            "identical -O2 -g ThinLTO builds write the same executable and PDB");
    }
    {
        // At -O0 thin keeps the ordinary partitioned build and says so (optimization part 2, item 44).
        // En -O0 thin conserva el build particionado normal y lo dice (parte 2, punto 44).
        const std::string executable = (root / "ThinO0.exe").string();
        const std::array<std::string_view, 7> arguments = {
            "-O0", "-j4", "-flto=thin", "--emit-project-oop-exe", project, "-o", executable};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const nativesharp::ExitCode result = driver.run(arguments, output, error);
        tests.expect(result == nativesharp::ExitCode::Success &&
                error.str().find("nsc: warning: -flto=thin at -O0") != std::string::npos,
            "ThinLTO at -O0 warns that it builds the ordinary partitioned code: " + error.str());
    }
    return tests.resultKeeping(root);
}
