#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::linker::CoffArchiveVerifier;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::tests::TestContext;

std::optional<std::string> environmentValue(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return std::nullopt;
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) return std::nullopt;
    return std::string(value);
#endif
}

void setEnvironmentValue(const char* name, const std::optional<std::string>& value)
{
#ifdef _WIN32
    _putenv_s(name, value.has_value() ? value->c_str() : "");
#else
    if (value.has_value()) setenv(name, value->c_str(), 1);
    else unsetenv(name);
#endif
}

class ScopedEnvironmentValue final
{
public:
    ScopedEnvironmentValue(const char* name, std::string value)
        : name_(name), previous_(environmentValue(name))
    {
        setEnvironmentValue(name_.c_str(), value);
    }

    ~ScopedEnvironmentValue()
    {
        setEnvironmentValue(name_.c_str(), previous_);
    }

    ScopedEnvironmentValue(const ScopedEnvironmentValue&) = delete;
    ScopedEnvironmentValue& operator=(const ScopedEnvironmentValue&) = delete;

private:
    std::string name_;
    std::optional<std::string> previous_;
};

std::filesystem::path configuredTool(
    const char* specificVariable,
    const std::filesystem::path& defaultPath)
{
    if (const auto configured = environmentValue(specificVariable); configured.has_value())
        return *configured;
    return defaultPath;
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

ExitCode emitProduct(
    std::string_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    bool optimize,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::string_view optimization = optimize ? "-O2" : "-O0";
    const std::array<std::string_view, 5> arguments = {
        optimization, action, projectText, "-o", outputText
    };
    return runDriver(arguments, output, error);
}

ExitCode verifyArtifacts(
    const std::filesystem::path& metadata,
    const std::filesystem::path& library,
    const std::filesystem::path& runtime,
    std::string& output,
    std::string& error)
{
    const std::string metadataText = metadata.string();
    const std::string libraryText = library.string();
    const std::string runtimeText = runtime.string();
    const std::array<std::string_view, 4> arguments = {
        "--verify-library-artifacts", metadataText, libraryText, runtimeText
    };
    return runDriver(arguments, output, error);
}

int runExecutable(const std::filesystem::path& executable)
{
#ifdef _WIN32
    return std::system(("\"" + executable.string() + "\"").c_str());
#else
    static_cast<void>(executable);
    return 42;
#endif
}

struct LinkerChoice final
{
    std::string_view name;
    std::filesystem::path executable;
};
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_stage46_1_dll_cross_linker_matrix");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    const std::filesystem::path lldLink = configuredTool(
        "NSC_P1_LLD_LINK", "C:/clang-llvm/bin/lld-link.exe");
    const std::filesystem::path nsLink = configuredTool(
        "NSC_P1_NSLINK", configuredTool("NSLINK_EXE", "C:/native-sharp/bin/nslink.exe"));
    tests.expect(std::filesystem::is_regular_file(lldLink),
                 "DLL-P1 locates the lld-link executable");
    tests.expect(std::filesystem::is_regular_file(nsLink),
                 "DLL-P1 locates the NSLink executable");
    if (!std::filesystem::is_regular_file(lldLink) || !std::filesystem::is_regular_file(nsLink))
        return tests.resultKeeping(root);

    const std::array<LinkerChoice, 2> linkers = {{
        {"lld", lldLink},
        {"nslink", nsLink}
    }};
    const std::array<bool, 2> optimizations = {false, true};

    for (const LinkerChoice& producerLinker : linkers)
    {
        for (const LinkerChoice& consumerLinker : linkers)
        {
            for (const bool producerOptimize : optimizations)
            {
                for (const bool consumerOptimize : optimizations)
                {
                    const std::string label =
                        std::string(producerLinker.name) + "-" +
                        (producerOptimize ? "O2" : "O0") + "__" +
                        std::string(consumerLinker.name) + "-" +
                        (consumerOptimize ? "O2" : "O0");
                    const std::filesystem::path caseRoot = root / label;
                    const std::filesystem::path producer = caseRoot / "Producer";
                    const std::filesystem::path consumer = caseRoot / "Consumer";
                    const std::filesystem::path products = caseRoot / "Products";
                    std::filesystem::create_directories(products);

                    writeFile(producer / "MatrixProducer.nsproj",
                              "<Project><PropertyGroup><Name>MatrixProducer</Name>"
                              "<OutputType>SharedLibrary</OutputType></PropertyGroup>"
                              "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
                    writeFile(producer / "Library.ns", R"NS(
namespace DllMatrix
{
    public class Api
    {
        public static int Answer() { return 42; }
    }
}
)NS");

                    std::string output;
                    std::string error;
                    const std::filesystem::path dll = products / "MatrixProducer.dll";
                    {
                        ScopedEnvironmentValue selected("NSC_LLD_LINK", producerLinker.executable.string());
                        tests.expect(
                            emitProduct("--emit-project-oop-dll",
                                        producer / "MatrixProducer.nsproj",
                                        dll,
                                        producerOptimize,
                                        output,
                                        error) == ExitCode::Success,
                            label + ": producer emits DLL/import library/NSMETA (" + error + ")");
                    }

                    output.clear();
                    error.clear();
                    tests.expect(
                        verifyArtifacts(products / "MatrixProducer.nsmeta",
                                        products / "MatrixProducer.lib",
                                        dll,
                                        output,
                                        error) == ExitCode::Success,
                        label + ": producer artifact triplet verifies (" + error + ")");
                    CoffArchiveVerifier archiveVerifier;
                    tests.expect(archiveVerifier.verify(products / "MatrixProducer.lib"),
                                 label + ": producer import library is valid COFF");

                    writeFile(consumer / "MatrixConsumer.nsproj",
                              "<Project><PropertyGroup><Name>MatrixConsumer</Name>"
                              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                              "<Compile Include=\"**/*.ns\"/>"
                              "<LibraryReference Include=\"../Products/MatrixProducer.lib\" "
                              "Runtime=\"../Products/MatrixProducer.dll\"/>"
                              "</ItemGroup></Project>");
                    writeFile(consumer / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return DllMatrix.Api.Answer(); }
}
)NS");

                    output.clear();
                    error.clear();
                    const std::filesystem::path executable = products / "MatrixConsumer.exe";
                    ExitCode consumerResult = ExitCode::CommandLineError;
                    {
                        ScopedEnvironmentValue selected("NSC_LLD_LINK", consumerLinker.executable.string());
                        consumerResult = emitProduct("--emit-project-oop-exe",
                                                     consumer / "MatrixConsumer.nsproj",
                                                     executable,
                                                     consumerOptimize,
                                                     output,
                                                     error);
                    }
                    tests.expect(consumerResult == ExitCode::Success,
                                 label + ": consumer links against producer (" + error + ")");
                    PeExecutableVerifier executableVerifier;
                    tests.expect(consumerResult == ExitCode::Success &&
                                     executableVerifier.verify(executable) &&
                                     std::ranges::find(executableVerifier.info().importedLibraries,
                                                       "MatrixProducer.dll") !=
                                         executableVerifier.info().importedLibraries.end(),
                                 label + ": consumer imports the declared producer runtime");
#ifdef _WIN32
                    if (consumerResult == ExitCode::Success)
                        tests.expect(runExecutable(executable) == 42,
                                     label + ": consumer executes to 42");
#endif
                }
            }
        }
    }

    return tests.resultKeeping(root);
}
