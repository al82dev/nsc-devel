#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/NumericSemantics.h"
#include "nativesharp/LLVM/FloatingPointMode.h"

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
    std::string& diagnostic)
{
    std::ostringstream out;
    std::ostringstream error;
    const ExitCode code = Driver{}.run(arguments, out, error);
    output = out.str();
    diagnostic = error.str();
    return code;
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
    if (runDriver(arguments, output, diagnostic) != ExitCode::Success) return false;
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += output + "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    _putenv_s("NSC_LINKER", "A:\\nslink-devel\\build-debug\\bin\\nslink.exe");
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    using nativesharp::language::FloatingPointMode;
    const auto strictContract =
        nativesharp::language::floatingPointContract(FloatingPointMode::Strict);
    const auto preciseContract =
        nativesharp::language::floatingPointContract(FloatingPointMode::Precise);
    const auto fastContract =
        nativesharp::language::floatingPointContract(FloatingPointMode::Fast);
    tests.expect(strictContract.preservesNanSemantics && strictContract.preservesSignedZero &&
            !strictContract.allowsContraction && !strictContract.allowsReassociation &&
            preciseContract.preservesNanSemantics && preciseContract.allowsContraction &&
            !preciseContract.allowsReassociation && !fastContract.preservesNanSemantics &&
            !fastContract.preservesSignedZero && fastContract.allowsReassociation &&
            strictContract.preservesNanPayload && strictContract.preservesDenormals &&
            strictContract.constevalMatchesRuntime && !preciseContract.constevalMatchesRuntime &&
            !fastContract.preservesNanPayload,
        "strict, precise, and fast publish distinct NaN/zero/contraction/reassociation contracts");

    tests.expect(
        nativesharp::llvm_backend::parseFloatingPointMode("strict") ==
            nativesharp::llvm_backend::FloatingPointMode::Strict &&
        nativesharp::llvm_backend::floatingPointModeName(
            nativesharp::llvm_backend::FloatingPointMode::Strict) == "strict",
        "strict is a first-class backend mode while deterministic remains compatible");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t14-f04-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    const std::filesystem::path project = writeProject(root / "modes", "FloatingPointScopes", R"NS(
namespace NativeSharp.Core.Numerics
{
    public copy struct Float32x4
    {
        public float X; public float Y; public float Z; public float W;
        public static Float32x4 Add(Float32x4 a, Float32x4 b) { return default(Float32x4); }
        public static Float32x4 Multiply(Float32x4 a, Float32x4 b) { return default(Float32x4); }
    }
}
namespace FloatingPointScopes
{
    using NativeSharp.Core.Numerics;
    public class Program
    {
        public static double StrictAdd(double a, double b) fp(strict) { return a + b; }
        public static double FastAdd(double a, double b) fp(fast) { return a + b; }
        public static double Mixed(double a, double b) fp(strict)
        {
            fp(fast) { a = a * b; }
            fp(precise) { return a + b; }
        }
        public static Float32x4 FastVector(Float32x4 a, Float32x4 b) fp(fast)
        {
            return Float32x4.Multiply(Float32x4.Add(a, b), b);
        }
        public static int Main() fp(strict)
        {
            const double folded = 20.0 + 22.0;
            double negativeZero = -0.0;
            double one = 1.0;
            if ((one / negativeZero) >= 0.0) return 1;
            double zero = 0.0;
            double nan = zero / zero;
            if (nan == nan) return 2;
            if ((int)folded != (int)StrictAdd(40.0, 2.0)) return 3;
            return (int)folded;
        }
    }
}
)NS");

    std::string executionDiagnostic;
    const bool executed = compileAndRun(project, 42, executionDiagnostic);
    tests.expect(executed,
        "strict NaN and signed-zero semantics execute at O0 through NSLink (" +
            executionDiagnostic + ")");

    const std::string projectText = project.string();
    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", projectText, "-O0"};
    std::string llvm;
    std::string llvmDiagnostic;
    const bool llvmDumped = runDriver(llvmArguments, llvm, llvmDiagnostic) == ExitCode::Success;
    tests.expect(llvmDumped &&
            llvm.find("llvm.experimental.constrained.fadd") != std::string::npos &&
            llvm.find("round.tonearest") != std::string::npos,
        "strict functions emit constrained nearest-even FP operations (" +
            llvmDiagnostic + ")");
    tests.expect(llvmDumped &&
            (llvm.find("fadd fast double") != std::string::npos ||
             llvm.find("fmul fast double") != std::string::npos),
        "fast functions/scopes emit reassociation and relaxed NaN/signed-zero flags");
    tests.expect(llvmDumped &&
            (llvm.find("fadd contract double") != std::string::npos ||
             llvm.find("fmul contract double") != std::string::npos),
        "precise scopes preserve NaN/signed zero while permitting contraction");
    tests.expect(llvmDumped && llvm.find("fmul fast <4 x float>") != std::string::npos,
        "the same source mode reaches SIMD floating-point operations");

    const std::array<std::string_view, 4> strictCliArguments = {
        "--dump-project-oop-llvm", projectText, "-O0", "--fp-mode=strict"};
    std::string strictCli;
    std::string strictCliDiagnostic;
    tests.expect(runDriver(strictCliArguments, strictCli, strictCliDiagnostic) ==
                ExitCode::Success &&
            strictCli.find("floating-point-mode: strict") != std::string::npos,
        "the strict project/CLI mode is accepted and reported (" + strictCliDiagnostic + ")");

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", projectText, "-O0"};
    std::string mir;
    std::string mirDiagnostic;
    const bool mirDumped = runDriver(mirArguments, mir, mirDiagnostic) == ExitCode::Success;
    tests.expect(mirDumped && mir.find("fp=strict") != std::string::npos &&
            mir.find("fp=precise") != std::string::npos &&
            mir.find("fp=fast") != std::string::npos,
        "MIR exposes effective function and lexical-scope FP policies (" +
            mirDiagnostic + ")");

    const std::filesystem::path invalid = writeProject(root / "invalid", "InvalidFpMode", R"NS(
namespace InvalidFpMode
{
    public class Program
    {
        public static int Main() fp(chaos) { return 0; }
    }
}
)NS");
    const std::string invalidText = invalid.string();
    const std::array<std::string_view, 2> invalidArguments = {
        "--dump-project-oop-mir", invalidText};
    std::string invalidOutput;
    std::string invalidDiagnostic;
    tests.expect(runDriver(invalidArguments, invalidOutput, invalidDiagnostic) != ExitCode::Success &&
            invalidDiagnostic.find("floating-point mode must be strict, precise, or fast") !=
                std::string::npos,
        "unknown source FP modes receive a causal diagnostic");

    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, cleanupError);
    return result;
}
