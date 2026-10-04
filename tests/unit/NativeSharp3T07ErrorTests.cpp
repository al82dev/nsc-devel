#include "nativesharp/Language/ErrorSemantics.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"

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
        diagnostic += "driver exit code " +
            std::to_string(static_cast<int>(result));
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
    using namespace nativesharp::language;
    std::string error;
    const ResultProtocol result{"type:Packet", "type:IoError"};
    tests.expect(supportsQuestionPropagation(result, error),
        "? accepts a complete static Result protocol without runtime identity");
    ResultProtocol incomplete = result;
    incomplete.hasTakeError = false;
    tests.expect(!supportsQuestionPropagation(incomplete, error),
        "? rejects incomplete protocols before lowering");
    tests.expect(errorConversionIsExplicit("E", "E", false) &&
            errorConversionIsExplicit("E1", "E2", true) &&
            !errorConversionIsExplicit("E1", "E2", false),
        "error propagation never invents an implicit cross-error conversion");

    tests.expect(validateThrowContract({"type:IoError", ThrowAbi::DualReturn, false, false}, error) &&
            !validateThrowContract({"type:IoError", ThrowAbi::DualReturn, true, false}, error),
        "typed throws has a native ABI and requires adapters at C boundaries");
    tests.expect(catchesAllDeclaredErrors(
            std::array<std::string, 2>{"Io", "Parse"},
            std::array<std::string, 2>{"Parse", "Io"}, false) &&
            !catchesAllDeclaredErrors(
                std::array<std::string, 2>{"Io", "Parse"},
                std::array<std::string, 1>{"Io"}, false),
        "typed catch coverage is checked without managed RTTI");
    tests.expect(neverCoercesTo("builtin:int") && neverCoercesTo("type:Packet"),
        "never coerces to every inhabited result type without producing a value");
    tests.expect(unifyLoopBreakTypes(std::array<std::string, 3>{"never", "builtin:int", "builtin:int"}) ==
            "builtin:int" && !unifyLoopBreakTypes(
                std::array<std::string, 2>{"builtin:int", "builtin:long"}).has_value(),
        "loop expressions unify break values while ignoring diverging paths");
    const PanicContract abort = panicContract(PanicProfile::Abort);
    const PanicContract unwind = panicContract(PanicProfile::NativeUnwind);
    tests.expect(!abort.runsCleanup && unwind.runsCleanup && !abort.mayCrossFfi && !unwind.mayCrossFfi,
        "panic profiles define cleanup and forbid implicit unwind across FFI");
    tests.expect(assertionRequiresUnsafe(AssertionKind::Assume) &&
            !assertionRequiresUnsafe(AssertionKind::Assert),
        "assume is unsafe because a false premise may introduce undefined behavior");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t07_result_propagation";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const std::filesystem::path project = writeProject(
        root, "ResultPropagation", R"NS(
public variant Result<T, E> { Ok(T), Error(E) }

public class State
{
    public static int Cleanups = 0;
    public static int Observed = 0;
    public static int Conversions = 0;
}

public struct SourceError
{
    public int Code;
    public SourceError(int code) { Code = code; }
}

public struct PublicError
{
    public int Code;
    public PublicError(int code) { Code = code; }
    public static explicit operator PublicError(SourceError source)
    {
        State.Conversions = State.Conversions + 1;
        return new PublicError(source.Code);
    }
}

public class Program
{
    private static Result<int, SourceError> Source(bool fail, int value)
    {
        if (fail) { return Result<int, SourceError>.Error(new SourceError(value)); }
        return Result<int, SourceError>.Ok(value);
    }

    private static Result<int, PublicError> Forward(bool fail, int value)
    {
        defer [] { State.Cleanups = State.Cleanups + 1; }
        int extracted = Source(fail, value)?;
        State.Observed = extracted + 2;
        return Result<int, PublicError>.Ok(extracted + 2);
    }

    public static int Main()
    {
        Result<int, PublicError> successful = Forward(false, 40);
        if (!successful.IsOk()) { return 10; }
        Result<int, PublicError> failed = Forward(true, 7);
        if (!failed.IsError()) { return 20; }
        int conditional = State.Observed == 42 ? 1 : 0;
        if (conditional != 1 || State.Cleanups != 2 || State.Conversions != 1) { return 31; }
        return State.Observed;
    }
}
)NS");

    nativesharp::frontend::ProjectCompilation compilation;
    std::string compilationError;
    const bool mirBuilt = compilation.compileProjectOopMir(project, compilationError);
    std::string semanticDiagnostics;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!semanticDiagnostics.empty()) semanticDiagnostics += " | ";
        semanticDiagnostics += "@" + std::to_string(diagnostic.location().offset()) + ": ";
        semanticDiagnostics += diagnostic.message();
    }
    const std::string mir = mirBuilt && compilation.oopMirModel() != nullptr
        ? nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel())
        : std::string();
    tests.expect(
        mirBuilt && compilationError.empty() &&
            mir.find("schema=1.23") != std::string::npos &&
            mir.find("result-propagation") != std::string::npos &&
            mir.find("conversion=__nsc_method_") != std::string::npos &&
            mir.find("continuation=return") != std::string::npos,
        "postfix '?' publishes its branch, explicit error conversion, and cleanup return in MIR (" +
            compilationError + semanticDiagnostics + mir + ")");

    std::string executionDiagnostic;
    const bool executed = compileAndRun(project, 42, executionDiagnostic);
    tests.expect(executed,
        "O0/NSLink executes Ok continuation and converted Error propagation with defer cleanup (" +
            executionDiagnostic + ")");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
