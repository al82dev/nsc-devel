#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Language/CallableSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;

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
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
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

bool compileAndRun(
    const std::filesystem::path& project,
    int expected,
    std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() /
        (project.stem().string() + ".exe");
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        diagnostic += "driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t08-f04-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path execution = writeProject(
        root / "execution", "GenericLambdas", R"NS(
namespace GenericLambdas
{
    public class Program
    {
        public static int Main()
        {
            var identity = <T>(T value) => value;
            var automatic = (auto value) => value + 1;
            var addConstant = <const int N>(int value) => value + N;
            var constrained = <T>(T value) where T : struct, copy => value;
            var packed = <pack const int Ns>() => packsum(Ns);
            int first = identity(10);
            long second = identity<long>(20);
            int third = automatic(5);
            return first + (int)second + third + addConstant<4>(1) + constrained(3) +
                packed<1, 2, 3>();
        }
    }
}
)NS");
    std::string executionDiagnostic;
    const bool executionPassed = compileAndRun(execution, 50, executionDiagnostic);
    tests.expect(executionPassed,
        "O0/NSLink executes inferred type, explicit type/const, and auto generic lambdas (" +
            executionDiagnostic + ")");

    const std::filesystem::path invalid = writeProject(
        root / "invalid", "GenericLambdaInferenceDiagnostic", R"NS(
namespace GenericLambdaInferenceDiagnostic
{
    public class Program
    {
        public static int Main()
        {
            var impossible = <T>(int value) => value;
            return impossible(1);
        }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation compilation;
    std::string invalidError;
    const bool built = compilation.compileProjectOopMir(invalid, invalidError);
    std::string diagnostics;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        diagnostics += diagnostic.message() + " | ";
    tests.expect(!built &&
            diagnostics.find("generic-lambda parameter 'T' could not be inferred") !=
                std::string::npos,
        "failed generic-lambda inference names the unresolved parameter (" +
            invalidError + diagnostics + ")");

    const std::filesystem::path constraintInvalid = writeProject(
        root / "constraint-invalid", "GenericLambdaConstraintDiagnostic", R"NS(
namespace GenericLambdaConstraintDiagnostic
{
    public class Program
    {
        public static int Main()
        {
            var referenceOnly = <T>(T value) where T : class => value;
            return referenceOnly(1);
        }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation constraintCompilation;
    std::string constraintError;
    const bool constraintBuilt =
        constraintCompilation.compileProjectOopMir(constraintInvalid, constraintError);
    std::string constraintDiagnostics;
    for (const auto& diagnostic : constraintCompilation.diagnostics().diagnostics())
        constraintDiagnostics += diagnostic.message() + " | ";
    tests.expect(!constraintBuilt &&
            constraintDiagnostics.find("requires a reference type") != std::string::npos,
        "generic-lambda constraints reject an invalid concrete instance (" +
            constraintError + constraintDiagnostics + ")");

    const std::filesystem::path recursive = writeProject(
        root / "recursive", "GenericLambdaRecursion", R"NS(
namespace GenericLambdaRecursion
{
    public class Program
    {
        public static int Main()
        {
            var count = <T>(T value) => value == 0 ? 0 : 1 + count(value - 1);
            return count(42);
        }
    }
}
)NS");
    std::string recursiveDiagnostic;
    const bool recursivePassed = compileAndRun(recursive, 42, recursiveDiagnostic);
    tests.expect(recursivePassed,
        "a closed generic-lambda instance recurses through its direct self binding (" +
            recursiveDiagnostic + ")");

    tests.expect(!nativesharp::language::genericLambdaUsesErasedDispatch(),
        "generic lambda instances remain monomorphized native callables");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
