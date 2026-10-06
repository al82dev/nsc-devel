#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::string waveSource()
{
    std::string source =
        "class Wave { public static int Identity<T>(T value) { return 1; } ";
    for (std::size_t index = 0; index < 24; ++index)
    {
        source += "public static int M" + std::to_string(index) +
            "() { return Identity<int>(" + std::to_string(index) + "); } ";
    }
    source += "public static int Run() { int total = 0; ";
    for (std::size_t index = 0; index < 24; ++index)
        source += "total = total + M" + std::to_string(index) + "(); ";
    return source + "return total; } }\n";
}

std::pair<ExitCode, std::string> runDump(
    const std::filesystem::path& project,
    std::string_view jobs,
    bool timing,
    std::string& error)
{
    const std::string projectText = project.string();
    std::vector<std::string_view> arguments{
        "--dump-project-method-bodies", projectText, "-O0", "-j", jobs};
    if (timing) arguments.push_back("--time");
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const nativesharp::driver::Driver driver;
    const ExitCode code = driver.run(arguments, outputStream, errorStream);
    error = errorStream.str();
    return {code, outputStream.str()};
}

std::string digest(std::string_view text)
{
    const auto* data = reinterpret_cast<const std::uint8_t*>(text.data());
    return nativesharp::basic::sha256Hex(
        nativesharp::basic::sha256(std::span(data, text.size())));
}

void verifyO0Equivalence(TestContext& tests, const std::filesystem::path& project)
{
    std::string error1;
    std::string error2;
    std::string error12;
    std::string repeatedError;
    auto [code1, j1] = runDump(project, "1", false, error1);
    auto [code2, j2] = runDump(project, "2", false, error2);
    auto [code12, j12] = runDump(project, "12", false, error12);
    auto [repeatedCode, repeated] = runDump(project, "12", false, repeatedError);
    tests.expect(code1 == ExitCode::Success && code2 == ExitCode::Success &&
            code12 == ExitCode::Success && repeatedCode == ExitCode::Success,
        "O0 Driver dumps complete for j1, j2, j12 and a repeated j12 run");
    tests.expect(error1.empty() && error2.empty() && error12.empty() &&
            repeatedError.empty(),
        "O0 callable-wave Driver dumps keep stderr empty");
    tests.expect(j1 == j2 && j2 == j12 && j12 == repeated,
        "O0 Driver dumps are byte-identical across job counts and repetition");
    tests.expect(digest(j1) == digest(j2) && digest(j2) == digest(j12) &&
            digest(j12) == digest(repeated) && digest(j1).size() == 64,
        "O0 Driver dump artifacts preserve the same SHA-256");
}

void verifyWaveTelemetry(TestContext& tests, const std::filesystem::path& project)
{
    std::string error;
    const auto [code, output] = runDump(project, "2", true, error);
    tests.expect(code == ExitCode::Success && error.empty(),
        "O0 timing-mode callable-wave dump completes");
    tests.expect(output.find("timing phase=method-bodies-wave-0-analysis") !=
            std::string::npos &&
            output.find("timing phase=method-bodies-wave-0-wait") !=
                std::string::npos &&
            output.find("timing phase=method-bodies-wave-0-merge") !=
                std::string::npos &&
            output.find("peak_working_set_bytes=") != std::string::npos,
        "timing mode exposes wave analysis, wait, merge and memory evidence");
}
}

int runTests()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nsc_callable_wave_driver_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    const std::filesystem::path project = root / "CallableWaves.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>CallableWaves</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(root / "Main.ns", waveSource());
    verifyO0Equivalence(tests, project);
    verifyWaveTelemetry(tests, project);
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}

// NOLINTNEXTLINE(bugprone-exception-escape): both standard and unknown exceptions are reported.
int main()
{
    try
    {
        return runTests();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: unhandled callable-wave Driver exception: " <<
            exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "FAIL: unknown callable-wave Driver exception\n";
    }
    return 1;
}
