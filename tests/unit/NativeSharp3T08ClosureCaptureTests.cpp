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
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopMirModelDumper;
using nativesharp::tests::TestContext;

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
        diagnostic += "compiler exit code " +
            std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

bool diagnosticsContain(const ProjectCompilation& compilation, std::string_view needle)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        if (diagnostic.message().find(needle) != std::string::npos) return true;
    return false;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "directed-ns3-t08-explicit-captures";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    const std::filesystem::path positive = writeProject(root / "positive", "ExplicitCapture", R"NS(
delegate int Transform(int value);
class Program
{
    public static int Main()
    {
        int copied = 2;
        int shared = 3;
        raii Transform snapshot = [copy copied] (int value) => value + copied;
        raii Transform mutate = [ref mut shared] (int value) =>
        {
            shared = shared + value;
            return shared;
        };
        copied = 20;
        return snapshot(40) + mutate(1) - shared;
    }
}
)NS");
    ProjectCompilation compilation;
    std::string modelError;
    const bool modelBuilt = compilation.compileProjectOopMir(positive, modelError);
    tests.expect(modelBuilt && modelError.empty(),
        "explicit copy/ref-mut captures reach MIR (" + modelError + ")");
    if (modelBuilt && compilation.oopMirModel() != nullptr)
    {
        const std::string dump = ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
        tests.expect(dump.find("schema=1.29") != std::string::npos &&
                dump.find("mode=copy storage=owned-value-cell") != std::string::npos &&
                dump.find("mode=ref-mut storage=shared-reference-cell") != std::string::npos &&
                dump.find("callable-trait=FnMut") != std::string::npos,
            "MIR 1.29 publishes capture modes, storage and callable trait");
    }
    std::string executionDiagnostic;
    const bool positiveExecuted = compileAndRun(positive, 42, executionDiagnostic);
    tests.expect(positiveExecuted,
        "copy snapshots and ref-mut aliases execute through NSLink at O0 (" +
            executionDiagnostic + ")");

    const std::filesystem::path moveProject = writeProject(root / "move", "MoveCapture", R"NS(
delegate int Read();
class Box
{
    public int Value;
    public Box(int value) { Value = value; }
}
class Program
{
    public static int Main()
    {
        raii Box box = new Box(42);
        raii Read read = [move box] () => box.Value;
        return read();
    }
}
)NS");
    std::string moveDiagnostic;
    const bool moveExecuted = compileAndRun(moveProject, 42, moveDiagnostic);
    tests.expect(moveExecuted,
        "move capture transfers a move-only owner into the closure and destroys it once (" +
            moveDiagnostic + ")");

    const std::filesystem::path fnOnceProject = writeProject(
        root / "fn-once", "FnOnceRejected", R"NS(
delegate int Read();
class Box
{
    public int Value;
    public Box(int value) { Value = value; }
}
class Program
{
    public static int Main()
    {
        raii Box box = new Box(21);
        raii Read once = [move box] () => box.Value;
        int first = once();
        return first + once();
    }
}
)NS");
    ProjectCompilation fnOnceCompilation;
    std::string fnOnceError;
    const bool fnOnceBuilt = fnOnceCompilation.compileProjectMethodBodies(
        fnOnceProject, fnOnceError);
    tests.expect(!fnOnceBuilt && diagnosticsContain(
            fnOnceCompilation, "FnOnce closure local 'once' was already invoked"),
        "a move-capturing FnOnce closure cannot be invoked twice");

    const std::filesystem::path implicitProject = writeProject(
        root / "implicit", "ImplicitCaptureRejected", R"NS(
delegate int Read();
class Program
{
    public static int Main()
    {
        int allowed = 1;
        int hidden = 2;
        Read read = [copy allowed] () => allowed + hidden;
        return read();
    }
}
)NS");
    ProjectCompilation implicitCompilation;
    std::string implicitError;
    const bool implicitBuilt = implicitCompilation.compileProjectMethodBodies(
        implicitProject, implicitError);
    tests.expect(!implicitBuilt && diagnosticsContain(
            implicitCompilation, "is not present in the explicit capture list"),
        "an explicit list rejects an undeclared implicit capture");

    const std::filesystem::path noheapProject = writeProject(
        root / "noheap", "NoHeapCaptureRejected", R"NS(
delegate int Read();
class Program
{
    public static noheap int Main()
    {
        int value = 42;
        Read read = [copy value] () => value;
        return read();
    }
}
)NS");
    ProjectCompilation noheapCompilation;
    std::string noheapError;
    const bool noheapBuilt = noheapCompilation.compileProjectMethodBodies(
        noheapProject, noheapError);
    tests.expect(!noheapBuilt && diagnosticsContain(
            noheapCompilation, "not allowed in a noheap callable"),
        "noheap rejects a closure environment allocation");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
