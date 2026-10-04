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
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode result = driver.run(arguments, output, error);
    diagnostic = error.str() + output.str();
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

std::string diagnosticsFor(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        text += diagnostic.message() + " | ";
    return text;
}

}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    _putenv_s("NSC_LINKER", "A:\\nslink-devel\\build-debug\\bin\\nslink.exe");
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    FunctionType cdeclType;
    cdeclType.parameters = {"builtin:int"};
    cdeclType.result = "builtin:int";
    cdeclType.callingConvention = CallingConvention::C;
    FunctionType win64 = cdeclType;
    win64.callingConvention = CallingConvention::Win64;
    FunctionType vectorcall = cdeclType;
    vectorcall.callingConvention = CallingConvention::VectorCall;
    tests.expect(
        functionTypeIdentity(cdeclType) != functionTypeIdentity(win64) &&
        functionTypeIdentity(win64) != functionTypeIdentity(vectorcall) &&
        !functionTypesAssignable(cdeclType, win64) &&
        parseFunctionTypeIdentity(functionTypeIdentity(win64))->callingConvention ==
            CallingConvention::Win64,
        "cdecl, win64, and vectorcall are distinct, round-trippable function types");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f04-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::filesystem::path positive = writeProject(root / "positive", "Conventions", R"NS(
namespace Conventions
{
    public class Program
    {
        public static int Main()
        {
            fn* cdecl(int) -> int c;
            fn* win64(int) -> int w;
            fn* stdcall(int) -> int s;
            fn* vectorcall(int) -> int v;
            return 42;
        }
    }
}
)NS");
    std::string diagnostic;
    const bool positivePassed = compileAndRun(positive, 42, diagnostic);
    tests.expect(positivePassed,
        "all Windows x64 calling-convention type spellings compile and execute at O0 through NSLink (" +
            diagnostic + ")");

    tests.expect(!functionTypesAssignable(cdeclType, win64) &&
            !functionTypesAssignable(win64, cdeclType) &&
            !functionTypesAssignable(vectorcall, win64),
        "calling-convention mismatches require an explicit adapter in both directions");

    const std::filesystem::path variadic = writeProject(root / "variadic", "Variadic", R"NS(
namespace Variadic
{
    public class Program
    {
        public static int Main()
        {
            fn* vectorcall(int, ...) -> int callback;
            return 0;
        }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation variadicCompilation;
    std::string variadicError;
    const bool variadicBuilt = variadicCompilation.compileProjectOopMir(variadic, variadicError);
    const std::string variadicDiagnostics = diagnosticsFor(variadicCompilation);
    tests.expect(!variadicBuilt &&
            variadicDiagnostics.find("variadic function pointer requires") != std::string::npos,
        "vectorcall variadics are rejected while cdecl/win64 own the Win64 vararg contract (" +
            variadicError + variadicDiagnostics + ")");

    return tests.result();
}
