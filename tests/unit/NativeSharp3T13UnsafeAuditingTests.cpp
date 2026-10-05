#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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
    std::string_view source,
    int languageEdition = 3)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::to_string(languageEdition) +
        "</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsFor(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        text += diagnostic.message() + " | ";
    return text;
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

bool rejectsWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(project, error);
    diagnostic = error + diagnosticsFor(compilation);
    return !built && diagnostic.find(expected) != std::string::npos;
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

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f05-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    const std::filesystem::path positive = writeProject(root / "positive", "UnsafePositive", R"NS(
namespace UnsafePositive
{
    public class Program
    {
        public static unsafe int Read(int* pointer)
        {
            unsafe("pointer comes from a live local")
            {
                unsafe { return *pointer; }
            }
        }

        public static int Main()
        {
            int value = 42;
            unsafe("address is valid for this call") { return Read(&value); }
        }
    }
}
)NS");
    std::string positiveDiagnostic;
    const bool positivePassed = compileAndRun(positive, 42, positiveDiagnostic);
    tests.expect(positivePassed,
        "nested reasoned unsafe blocks and unsafe fn execute at O0 through NSLink (" +
            positiveDiagnostic + ")");

    const std::filesystem::path unsafeCall = writeProject(root / "unsafe-call", "UnsafeCall", R"NS(
namespace UnsafeCall
{
    public class Program
    {
        public static unsafe int Dangerous() { return 42; }
        public static int Main() { return Dangerous(); }
    }
}
)NS");
    std::string unsafeCallDiagnostic;
    const bool unsafeCallRejected = rejectsWith(
        unsafeCall, "call to unsafe function 'Dangerous' requires", unsafeCallDiagnostic);
    tests.expect(unsafeCallRejected,
        "unsafe fn places a caller-side requirement (" + unsafeCallDiagnostic + ")");

    const std::filesystem::path rawDereference = writeProject(root / "raw-deref", "RawDeref", R"NS(
namespace RawDeref
{
    public class Program
    {
        private static int Read(int* pointer) { return *pointer; }
        public static int Main() { return 0; }
    }
}
)NS");
    std::string rawDiagnostic;
    const bool rawRejected = rejectsWith(
        rawDereference, "raw pointer dereference requires", rawDiagnostic);
    tests.expect(rawRejected, "raw dereference is catalogued as unsafe (" + rawDiagnostic + ")");

    const std::filesystem::path pointerCast = writeProject(root / "pointer-cast", "PointerCast", R"NS(
namespace PointerCast
{
    public class Program
    {
        public static int Main()
        {
            int* pointer = (int*)1;
            return 0;
        }
    }
}
)NS");
    std::string castDiagnostic;
    const bool castRejected = rejectsWith(
        pointerCast, "integer/pointer casts require", castDiagnostic);
    tests.expect(castRejected, "integer/pointer casts are catalogued as unsafe (" +
        castDiagnostic + ")");

    const std::filesystem::path ffi = writeProject(root / "ffi", "UnsafeFfi", R"NS(
namespace UnsafeFfi
{
    public class Program
    {
        extern C from "kernel32.dll"
        {
            public uint GetCurrentProcessId();
        }
        public static int Main() { return (int)GetCurrentProcessId(); }
    }
}
)NS");
    std::string ffiDiagnostic;
    const bool ffiRejected = rejectsWith(
        ffi, "call to unsafe function 'GetCurrentProcessId' requires", ffiDiagnostic);
    tests.expect(ffiRejected, "FFI invocation is catalogued as unsafe (" + ffiDiagnostic + ")");

    const std::filesystem::path unsafeTrait = writeProject(
        root / "unsafe-trait", "UnsafeTrait", R"NS(
namespace UnsafeTrait
{
    unsafe trait RawContract
    {
        public abstract int Read();
    }
    public class Holder {}
    impl RawContract for Holder
    {
        public int Read() { return 42; }
    }
    public class Program { public static int Main() { return 0; } }
}
)NS");
    std::string traitDiagnostic;
    const bool traitRejected = rejectsWith(
        unsafeTrait, "implementing an unsafe trait requires 'unsafe impl'", traitDiagnostic);
    tests.expect(traitRejected,
        "unsafe traits require an auditable unsafe impl declaration (" + traitDiagnostic + ")");

    const std::filesystem::path assembly = writeProject(root / "asm", "UnsafeAsm", R"NS(
namespace UnsafeAsm
{
    public class Program
    {
        public static int Main()
        {
            asm { nop }
            return 0;
        }
    }
}
)NS");
    std::string asmDiagnostic;
    const bool asmRejected = rejectsWith(
        assembly, "inline assembly requires", asmDiagnostic);
    tests.expect(asmRejected, "inline assembly is catalogued as unsafe (" + asmDiagnostic + ")");

    const std::filesystem::path broad = writeProject(root / "broad", "BroadUnsafe", R"NS(
namespace BroadUnsafe
{
    public class Program
    {
        public static int Main()
        {
            unsafe
            {
                int a = 1; int b = 2; int c = 3; int d = 4; int e = 5;
                int f = 6; int g = 7; int h = 8; int i = 9;
            }
            return 0;
        }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation broadCompilation;
    std::string broadError;
    broadCompilation.compileProjectOopMir(broad, broadError);
    const std::string broadDiagnostics = broadError + diagnosticsFor(broadCompilation);
    tests.expect(broadDiagnostics.find("unsafe block covers more than eight statements") !=
            std::string::npos,
        "broad unexplained unsafe blocks produce the audit lint (" + broadDiagnostics + ")");

    const std::filesystem::path edition2 = writeProject(root / "edition2", "UnsafeEdition2", R"NS(
namespace UnsafeEdition2
{
    public class Program
    {
        extern C from "kernel32.dll" { public uint GetCurrentProcessId(); }
        public static int Main()
        {
            int value = 42;
            int* pointer = &value;
            if (GetCurrentProcessId() == 0u) { return 0; }
            return *pointer;
        }
    }
}
)NS", 2);
    nativesharp::frontend::ProjectCompilation edition2Compilation;
    std::string edition2Error;
    const bool edition2Built = edition2Compilation.compileProjectOopMir(edition2, edition2Error);
    tests.expect(edition2Built,
        "edition 2 preserves legacy raw-pointer and FFI call semantics (" + edition2Error +
            diagnosticsFor(edition2Compilation) + ")");

    return tests.result();
}
