#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    int expected,
    std::string& diagnostic)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    if (driver.run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err.str() + out.str();
        return false;
    }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    if (result != expected)
    {
        diagnostic = "process result " + std::to_string(result) + ", expected " +
            std::to_string(expected);
        return false;
    }
    return true;
}

std::string dumpIr(const std::filesystem::path& project, std::string& diagnostic)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> arguments = {"--dump-project-oop-llvm", projectText};
    if (driver.run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err.str() + out.str();
        return {};
    }
    return out.str();
}
}

// EN: Rules checked here (NativeSharp 3.0). A method that declares `throws` does not convert to an `extern C delegate`,
//     whose signature declares no error (checked throw C-13 plus the delegate error type match). A callback without
//     `throws` gets the foreign-boundary thunk with its catchswitch, both catchpads and the CallbackExceptionEscape
//     trap 13 kept as a guard. A `panic()` inside the callback follows the abort profile: exit 128 + 14 = 142 before
//     any unwinding reaches the boundary. Source code cannot make an exception reach the boundary, so exit 141 is no
//     longer an observable result.
// ES: Reglas que se comprueban (NativeSharp 3.0). Un metodo que declara `throws` no se convierte en un `extern C
//     delegate`, cuya firma no declara error (throw chequeado C-13 y coincidencia del tipo de error del delegate).
//     Un callback sin `throws` recibe el thunk de frontera con su catchswitch, las dos catchpads y el trap 13
//     CallbackExceptionEscape como guarda. Un `panic()` dentro del callback sigue el perfil abort: salida
//     128 + 14 = 142 antes de que ningun desenrollado llegue a la frontera. El codigo fuente no puede hacer llegar
//     una excepcion a la frontera, asi que la salida 141 ya no es un resultado observable.
int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item341_exception_safe_com_thunk";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string project =
        "<Project><PropertyGroup><Name>ComThunk</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>";

    writeFile(root / "rejected" / "ComThunk.nsproj", project);
    writeFile(root / "rejected" / "Main.ns", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;

public extern C delegate int ComMethod(void* self, int value);
public class Boom : Exception { }

public class Program
{
    public static int Throws(void* self, int value) throws Boom
    {
        throw new Boom();
    }

    public static int Main()
    {
        ComMethod guarded = Throws;
        return guarded(null, 42);
    }
}
)NS");
    std::string diagnostic;
    const std::string rejectedIr = dumpIr(root / "rejected" / "ComThunk.nsproj", diagnostic);
    tests.expect(rejectedIr.empty() &&
            diagnostic.find("no method in the method group exactly matches delegate signature") !=
                std::string::npos,
        "a method that declares throws does not convert to an extern C delegate (" + diagnostic + ")");

    writeFile(root / "panic" / "ComThunk.nsproj", project);
    writeFile(root / "panic" / "Main.ns", R"NS(
public extern C delegate int ComMethod(void* self, int value);
public struct ComObject { public nint* VTable; }

public class Program
{
    public static int Panics(void* self, int value)
    {
        panic("boom");
    }

    public static int Main()
    {
        ComMethod guarded = Panics;
        nint* table = stackalloc nint[1];
        table[0] = (nint)guarded;
        ComObject instance;
        instance.VTable = table;
        ComMethod method = (ComMethod)instance.VTable[0];
        return method(&instance, 42);
    }
}
)NS");
    diagnostic.clear();
    const std::string ir = dumpIr(root / "panic" / "ComThunk.nsproj", diagnostic);
    tests.expect(!ir.empty(), "COM thunk fixture lowers to LLVM (" + diagnostic + ")");
    tests.expect(ir.find("boundary.catchswitch") != std::string::npos &&
            ir.find("boundary.exception.catchpad") != std::string::npos &&
            ir.find("boundary.panic.catchpad") != std::string::npos,
        "COM thunk contains the foreign-boundary catchswitch and both NativeSharp catchpads");
    tests.expect(ir.find("call void @__nativesharp_trap(i32 13)") != std::string::npos,
        "COM thunk keeps the CallbackExceptionEscape trap as the boundary guard");
    diagnostic.clear();
    const bool o0Passed = compileAndRun(
        root / "panic" / "ComThunk.nsproj", root / "ComThunkO0.exe", "-O0", 142, diagnostic);
    tests.expect(o0Passed, "a panic inside the COM callback exits with the abort profile at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2Passed = compileAndRun(
        root / "panic" / "ComThunk.nsproj", root / "ComThunkO2.exe", "-O2", 142, diagnostic);
    tests.expect(o2Passed, "a panic inside the COM callback exits with the abort profile at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
