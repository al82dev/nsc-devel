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

bool rejectedWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(project, error);
    diagnostic = error;
    for (const auto& item : compilation.diagnostics().diagnostics())
        diagnostic += item.message() + " | ";
    return !built && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t08-f05-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const auto layout = nativesharp::language::windowsX64BoundMethodLayout();
    tests.expect(layout.size == 32 && layout.alignment == 8,
        "bound method uses the native ABI 1.1 four-pointer cell");

    const std::filesystem::path dynamic = writeProject(
        root / "dynamic", "DynamicMethodValues", R"NS(
namespace DynamicMethodValues
{
    public delegate int Transform(int value);
    public interface ITransform { int Apply(int value); }
    public class Base
    {
        public virtual int Apply(int value) { return value + 1; }
        public long Apply(long value) { return value + 100; }
    }
    public class Derived : Base
    {
        public override int Apply(int value) { return value + 19; }
    }
    public class InterfaceTransform : ITransform
    {
        public int Apply(int value) { return value + 21; }
    }
    public class Program
    {
        public static int Main()
        {
            Base polymorphic = new Derived();
            ITransform contract = new InterfaceTransform();
            raii Transform virtualMethod = polymorphic.Apply;
            raii Transform interfaceMethod = contract.Apply;
            int answer = virtualMethod(1) + interfaceMethod(1);
            delete polymorphic;
            delete contract;
            return answer;
        }
    }
}
)NS");
    std::string dynamicDiagnostic;
    tests.expect(compileAndRun(dynamic, 42, dynamicDiagnostic),
        "O0/NSLink bound methods preserve virtual and interface dispatch (" +
            dynamicDiagnostic + ")");

    const std::filesystem::path moved = writeProject(
        root / "moved", "MovedReceiverMethodValue", R"NS(
namespace MovedReceiverMethodValue
{
    public delegate int Transform(int value);
    public class Box
    {
        public static int Drops;
        public int Bias;
        ~Box() { Drops = Drops + 1; }
        public int Add(int value) { return value + Bias; }
    }
    public class Program
    {
        public static int Main()
        {
            int answer = 0;
            {
                raii Box box = new Box();
                box.Bias = 1;
                raii Transform bound = move(box.Add);
                answer = bound(40);
            }
            return answer + Box.Drops;
        }
    }
}
)NS");
    std::string movedDiagnostic;
    tests.expect(compileAndRun(moved, 42, movedDiagnostic),
        "move(receiver.Method) transfers receiver ownership and destroys it exactly once (" +
            movedDiagnostic + ")");

    const std::filesystem::path explicitThis = writeProject(
        root / "this", "ExplicitThisCapture", R"NS(
namespace ExplicitThisCapture
{
    public delegate int Transform(int value);
    public class Box
    {
        public int Bias;
        public int Run()
        {
            raii Transform add = [ref this](int value) => value + Bias;
            return add(40);
        }
    }
    public class Program
    {
        public static int Main()
        {
            raii Box box = new Box();
            box.Bias = 2;
            return box.Run();
        }
    }
}
)NS");
    std::string thisDiagnostic;
    tests.expect(compileAndRun(explicitThis, 42, thisDiagnostic),
        "[ref this] is an explicit borrowed receiver capture (" + thisDiagnostic + ")");

    const std::filesystem::path escaping = writeProject(
        root / "escape", "BorrowedMethodEscape", R"NS(
namespace BorrowedMethodEscape
{
    public delegate int Transform(int value);
    public class Box { public int Add(int value) { return value + 1; } }
    public class Program
    {
        public static Transform Invalid()
        {
            raii Box box = new Box();
            return box.Add;
        }
        public static int Main() { return 0; }
    }
}
)NS");
    std::string escapeDiagnostic;
    tests.expect(rejectedWith(escaping, "bound method borrows its receiver", escapeDiagnostic),
        "a borrowed receiver method value cannot escape by return (" + escapeDiagnostic + ")");

    const std::filesystem::path invalidThis = writeProject(
        root / "invalid-this", "InvalidThisCapture", R"NS(
namespace InvalidThisCapture
{
    public delegate int Transform(int value);
    public class Box
    {
        public int Bias;
        public Transform Invalid() { return [move this](int value) => value + Bias; }
    }
    public class Program { public static int Main() { return 0; } }
}
)NS");
    std::string invalidThisDiagnostic;
    tests.expect(rejectedWith(invalidThis, "'this' is a borrowed receiver", invalidThisDiagnostic),
        "copy/move cannot invent ownership for this (" + invalidThisDiagnostic + ")");

    const std::filesystem::path temporary = writeProject(
        root / "temporary", "TemporaryReceiverMethodValue", R"NS(
namespace TemporaryReceiverMethodValue
{
    public delegate int Transform(int value);
    public class Box { public int Add(int value) { return value + 1; } }
    public class Program
    {
        public static int Main()
        {
            raii Transform invalid = (new Box()).Add;
            return invalid(1);
        }
    }
}
)NS");
    std::string temporaryDiagnostic;
    tests.expect(rejectedWith(temporary, "temporary receiver cannot be borrowed", temporaryDiagnostic),
        "a temporary receiver requires explicit move ownership (" + temporaryDiagnostic + ")");

    const std::filesystem::path readonlyThis = writeProject(
        root / "readonly-this", "ReadonlyThisCapture", R"NS(
namespace ReadonlyThisCapture
{
    public delegate int Transform(int value);
    public struct Box
    {
        public int Bias;
        public readonly Transform Invalid()
        {
            return [ref mut this](int value) => value + Bias;
        }
    }
    public class Program { public static int Main() { return 0; } }
}
)NS");
    std::string readonlyThisDiagnostic;
    tests.expect(rejectedWith(
            readonlyThis, "readonly method cannot capture 'this' as 'ref mut this'",
            readonlyThisDiagnostic),
        "readonly receivers reject mutable this captures (" + readonlyThisDiagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
