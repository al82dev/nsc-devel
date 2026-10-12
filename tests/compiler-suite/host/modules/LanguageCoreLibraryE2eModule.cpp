#include "ExternalProgramTestSupport.h"
#include "nsc_suite/TestRegistry.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NSC_SUITE_REPOSITORY_ROOT
#error "NSC_SUITE_REPOSITORY_ROOT is required for Language Core e2e cases"
#endif
#ifndef NSC_SUITE_NSC_EXECUTABLE
#error "NSC_SUITE_NSC_EXECUTABLE is required for Language Core e2e cases"
#endif
#ifndef NSC_SUITE_NSLINK_EXECUTABLE
#error "NSC_SUITE_NSLINK_EXECUTABLE is required for Language Core e2e cases"
#endif
#ifndef NSC_SUITE_WINDOWS_RUNTIME_LIBRARY
#error "NSC_SUITE_WINDOWS_RUNTIME_LIBRARY is required for Language Core e2e cases"
#endif

namespace nsc::suite
{
namespace
{
namespace fs = std::filesystem;
using external_program_support::Result;

class TemporaryProject final
{
public:
    explicit TemporaryProject(std::string_view caseId)
    {
        path_ = fs::temp_directory_path() / "nsc-component-language-core" /
            (std::string(caseId) + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(GetTickCount64()));
        std::error_code error;
        fs::create_directories(path_, error);
        valid_ = !error;
    }

    TemporaryProject(const TemporaryProject&) = delete;
    TemporaryProject& operator=(const TemporaryProject&) = delete;
    TemporaryProject(TemporaryProject&& other) noexcept
        : path_(std::move(other.path_)), valid_(other.valid_)
    {
        other.valid_ = false;
    }

    ~TemporaryProject()
    {
        if (valid_)
        {
            std::error_code ignored;
            fs::remove_all(path_, ignored);
        }
    }

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }

private:
    fs::path path_;
    bool valid_ = false;
};

void writeFile(const fs::path& path, std::string_view contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

std::string processFailure(std::string_view phase, const Result& result)
{
    return std::string(phase) + " started=" + (result.started ? "true" : "false") +
        " exit=" + std::to_string(result.exitCode) + " error=" + result.error +
        " output=" + result.output;
}

struct PreparedProgram
{
    TemporaryProject project;
    fs::path executable;
    Result compilation;

    explicit PreparedProgram(std::string_view caseId) : project(caseId) {}
};

PreparedProgram prepareProgram(std::string_view caseId, std::string_view fixtureName)
{
    PreparedProgram prepared(caseId);
    if (!prepared.project.valid())
    {
        prepared.compilation.error = "could not create isolated project directory";
        return prepared;
    }

    const fs::path repository(NSC_SUITE_REPOSITORY_ROOT);
    const fs::path fixture = repository / "tests/compiler-suite/fixtures/language-core" /
        fixtureName / "Main.ns";
    const fs::path source = prepared.project.path() / "Main.ns";
    std::error_code copyError;
    fs::copy_file(fixture, source, fs::copy_options::overwrite_existing, copyError);
    if (copyError)
    {
        prepared.compilation.error = "fixture copy failed: " + copyError.message();
        return prepared;
    }

    const std::string projectName = "LanguageCoreProbe";
    const fs::path projectFile = prepared.project.path() / (projectName + ".nsproj");
    const std::string coreProject =
        (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
    writeFile(projectFile,
        "<Project><PropertyGroup><Name>" + projectName +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<ProjectReference Include=\"" + coreProject +
        "\"/></ItemGroup></Project>");

    const fs::path compiler(NSC_SUITE_NSC_EXECUTABLE);
    const std::vector<std::wstring> arguments = {
        L"build", projectFile.wstring(), L"-O0", L"-g0", L"--linker", L"NSLink"};
    const std::vector<std::pair<std::wstring, std::wstring>> environment = {
        {L"NSC_LINKER", fs::path(NSC_SUITE_NSLINK_EXECUTABLE).wstring()},
        {L"NSC_WINDOWS_RUNTIME_LIBRARY",
            fs::path(NSC_SUITE_WINDOWS_RUNTIME_LIBRARY).wstring()}};
    prepared.compilation = external_program_support::run(
        compiler, arguments, {}, prepared.project.path(), environment);
    prepared.executable = prepared.project.path() / "bin/Debug" / (projectName + ".exe");
    return prepared;
}

void expectProgram(TestContext& context, std::string_view caseId, std::string_view fixtureName,
    std::string input, DWORD expectedExit)
{
    PreparedProgram prepared = prepareProgram(caseId, fixtureName);
    std::error_code sizeError;
    const bool executableReady = fs::is_regular_file(prepared.executable, sizeError) &&
        !sizeError && fs::file_size(prepared.executable, sizeError) != 0 && !sizeError;
    context.expect(prepared.compilation.started && prepared.compilation.exitCode == 0 &&
            prepared.compilation.error.empty() && executableReady,
        std::string(caseId) + " fixture must compile and publish a non-empty executable (" +
            processFailure("compile", prepared.compilation) + ")");
    if (!executableReady || prepared.compilation.exitCode != 0) return;

    const Result execution = external_program_support::run(
        prepared.executable, {}, input, prepared.project.path());
    context.expect(execution.started && execution.exitCode == expectedExit &&
            execution.error.empty(),
        std::string(caseId) + " executable must reach its exact terminal result (" +
            processFailure("execute", execution) + ")");
}

void addCase(TestRegistry& registry, std::string id, std::string contract,
    std::vector<std::string> tags, TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Language", "integration",
        "PR", std::move(tags), std::move(function)});
}
}

void registerLanguageCoreLibraryE2eModule(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-287", "CL-LANG-016",
        {"language", "numeric", "core", "e2e"}, [](TestContext& context) {
            expectProgram(context, "TL-LANG-287", "numeric", {}, 42);
        });
    addCase(registry, "TL-LANG-288", "CL-LANG-016",
        {"language", "numeric", "panic", "e2e"}, [](TestContext& context) {
            expectProgram(context, "TL-LANG-288", "numeric-invalid", {}, 142);
        });
    addCase(registry, "TL-LANG-289", "CL-LANG-010",
        {"language", "console", "readline", "e2e"}, [](TestContext& context) {
            std::string input = "alpha\r\n\r\n";
            input.append(8192, 'x');
            input += "\r\n";
            expectProgram(context, "TL-LANG-289", "console-readline", std::move(input), 42);
        });
    addCase(registry, "TL-LANG-290", "CL-LANG-010",
        {"language", "console", "numeric", "e2e"}, [](TestContext& context) {
            expectProgram(context, "TL-LANG-290", "console-parse",
                "255\r\n-128\r\n-32768\r\n65535\r\n-2147483648\r\n4294967295\r\n"
                "-9223372036854775808\r\n18446744073709551615\r\n1.5\r\n2.5\r\nbad\r\n",
                42);
        });
}
}
