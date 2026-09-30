#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_comptime_integration";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    writeFile(root / "A.ns", R"NS(
namespace Demo
{
    public int First()
    {
        int left = #run(40 + 2);
        int right = #run(40 + 2);
        return left + right;
    }
}
)NS");
    writeFile(root / "B.ns", "namespace Demo { public int Plain() { return 1; } }\n");
    writeFile(root / "Demo.nsproj", R"XML(<Project>
  <PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup>
  <ItemGroup><Compile Include="*.ns" /></ItemGroup>
</Project>
)XML");

    nativesharp::frontend::ProjectCompilation compilation;
    compilation.setCompilationJobs(1);
    std::string error;
    tests.expect(compilation.compileProjectSyntax(root / "Demo.nsproj", error),
        "productive project syntax path evaluates #run directives");
    tests.expect(error.empty(), "successful comptime integration has no load error");

    const auto records = compilation.comptimeRecords();
    tests.expect(records.size() == 2, "every source has one deterministic comptime record");
    tests.expect(records.size() == 2 && records[0].relativeSourcePath == "A.ns" &&
            records[1].relativeSourcePath == "B.ns",
        "comptime records use root-independent project-relative paths");
    tests.expect(records.size() == 2 && records[0].evaluationCount == 2 &&
            records[0].cacheHits == 1 && records[0].cacheMisses == 1 &&
            records[1].evaluationCount == 0,
        "repeated expressions expose cache behavior without inventing external inputs");
    tests.expect(records.size() == 2 && records[0].dependencies.size() == 2 &&
            records[0].dependencies[0].digest == records[0].dependencies[1].digest,
        "each #run dependency retains its source identity and content digest");
    tests.expect(records.size() == 2 && records[0].reproducibilityDigest.size() == 64,
        "source comptime result exposes a stable SHA-256 reproducibility digest");

    std::ostringstream report;
    nativesharp::frontend::ProjectCompilationDumper{}.dumpComptime(compilation, report);
    tests.expect(report.str().find("NSCOMPTIME 1.0") != std::string::npos &&
            report.str().find("evaluations: 2") != std::string::npos &&
            report.str().find("path=A.ns") != std::string::npos &&
            report.str().find(root.generic_string()) == std::string::npos,
        "project comptime manifest is versioned, explanatory, and root-independent");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
