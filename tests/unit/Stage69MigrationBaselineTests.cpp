#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/MigrationBaseline.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}
} // namespace

int main()
{
    using namespace nativesharp;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::string baseline = driver::currentMigrationBaseline();
    tests.expect(baseline.starts_with("NSMIGRATION 1\n") && baseline.ends_with('\n'),
        "migration baseline has canonical versioned framing");
    tests.expect(baseline.find("source_compiler=1.0.1\n") != std::string::npos &&
                     baseline.find("source_language=1.0.1\n") != std::string::npos &&
                     baseline.find("target_major=2\n") != std::string::npos,
        "migration baseline identifies its source and prospective target");
    tests.expect(baseline.find("nsmeta=1.14\n") != std::string::npos &&
                     baseline.find("native_abi=1.0\n") != std::string::npos &&
                     baseline.find("runtime_abi=1.0\n") != std::string::npos &&
                     baseline.find("runtime_symbol_set=1\n") != std::string::npos &&
                     baseline.find("mir_schema=1.2\n") != std::string::npos,
        "migration baseline freezes every cross-component contract");
    tests.expect(baseline.find("breaking_change_ledger=required\n") != std::string::npos &&
                     baseline.find("migration_notes=required\n") != std::string::npos &&
                     baseline.find("n_minus_one_matrix=required\n") != std::string::npos,
        "2.0 promotion requirements are executable fields rather than prose only");

    const std::filesystem::path evidence =
        nativesharp::tests::evidenceRoot("nativesharp_stage69_migration_baseline");
    std::error_code filesystemError;
    std::filesystem::remove_all(evidence, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(evidence, filesystemError);
    tests.expect(!filesystemError, "migration baseline evidence directory is available");

    const std::filesystem::path path = evidence / "NativeSharp2Migration.baseline";
    std::string error;
    tests.expect(driver::writeMigrationBaseline(path, error), "migration baseline writer succeeds");
    tests.expect(readFile(path) == baseline, "writer preserves canonical bytes");
    tests.expect(driver::verifyMigrationBaseline(path, error) && error.empty(),
        "current migration baseline verifies");

    std::string incompatible = baseline;
    const std::size_t runtime = incompatible.find("runtime_abi=1.0");
    if (runtime != std::string::npos)
    {
        incompatible.replace(runtime, std::string("runtime_abi=1.0").size(), "runtime_abi=2.0");
    }
    tests.expect(writeFile(path, incompatible) && !driver::verifyMigrationBaseline(path, error) &&
                     error.find("incompatible migration baseline field") != std::string::npos,
        "an unclassified runtime ABI break is rejected precisely");
    tests.expect(writeFile(path, baseline + "silent_break=accepted\n") &&
                     !driver::verifyMigrationBaseline(path, error) &&
                     error.find("unexpected migration baseline field") != std::string::npos,
        "unknown migration claims are not silently accepted");

    driver::Driver compiler;
    std::ostringstream output;
    std::ostringstream driverError;
    const std::vector<std::string_view> printArguments = {"--print-migration-baseline"};
    tests.expect(compiler.run(printArguments, output, driverError) == ExitCode::Success &&
                     output.str() == baseline && driverError.str().empty(),
        "public print command emits only canonical migration bytes");

    const std::filesystem::path driverPath = evidence / "driver.baseline";
    const std::string driverPathText = driverPath.string();
    const std::vector<std::string_view> writeArguments = {"--write-migration-baseline", driverPathText};
    output.str({});
    output.clear();
    tests.expect(compiler.run(writeArguments, output, driverError) == ExitCode::Success &&
                     readFile(driverPath) == baseline,
        "public write command produces the current baseline");
    const std::vector<std::string_view> verifyArguments = {"--verify-migration-baseline", driverPathText};
    output.str({});
    output.clear();
    tests.expect(compiler.run(verifyArguments, output, driverError) == ExitCode::Success &&
                     output.str().find("migration baseline verified") != std::string::npos,
        "public verifier accepts the installed migration baseline");

    return tests.result();
}
