// Stage 56 / C1 -- Interop completion: function-pointer round trip, import identity, callback
// exception boundary.
//
// This is phase C1 of the compiler roadmap (01_Compiler_Roadmap.md, sections 3.1, 3.2 and 3.6).
// Stage 55.x left the interop foundation with three named gaps, and each one is a scenario here:
//
//   1. FUNCTION-POINTER ROUND TRIP (roadmap 3.1). `void*`/`nint` <-> extern C function pointer,
//      in both directions, plus function-pointer RETURNS and explicit reinterpretation between
//      two foreign signatures. This is the primitive every proc-address API is made of
//      (wglGetProcAddress, glfwGetProcAddress, SDL_GL_GetProcAddress, alGetProcAddress), and
//      MEASURED before this round every one of these casts died in the backend with the same
//      contentless "Stage 34 local initialization failed".
//
//   2. IMPORT IDENTITY (roadmap 3.2). An import is identified by the TRIPLE (library, external
//      symbol, signature). MEASURED before this round: declaring `a.dll!Init` and `b.dll!Init`
//      in one project made LLVM rename the second function to `Init.1` behind the compiler's
//      back, and the link ended with "undefined symbol: Init.1".
//
//   3. CALLBACK EXCEPTION BOUNDARY (roadmap 3.6). A NativeSharp exception cannot escape through
//      a C boundary -- neither an `extern C delegate` callback nor an `export extern C` entry
//      point. Before this round nothing stopped it: the SEH RaiseException simply kept unwinding
//      into whatever foreign frames were below.
//
// TESTING POLICY. The project's rule is IR shape + real executable + O0 + O2 (see
// claude/05_metodo_de_trabajo_y_archivo_historico.md). Real execution needs Wine, which is
// probed exactly as every Stage 45+ suite probes it; where Wine is absent the execution half
// degrades to an explicit skip and the compile + IR-shape half still runs, so a missing tool is
// never a false red and never a silent pass either.

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
#include <vector>

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

std::filesystem::path writeProject(
    const std::filesystem::path& root, std::string_view name, std::string_view source)
{
    const auto directory = root / std::string(name);
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}

std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

int runUnderWine(const std::string& wine, const std::filesystem::path& exePath)
{
    const std::string command =
        "WINEDEBUG=-all timeout 25 " + wine + " " + exePath.string() + " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

// The IR text for a project, straight from the driver's own dump action. Used for the "IR shape"
// half of the policy: what the module must contain (or must NOT contain) is checked here even on
// a machine with no Wine at all.
std::string dumpIr(TestContext& tests, const std::filesystem::path& projectPath, std::string_view label)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    // `std::string_view` over a temporary is a dangling read -- the path text must outlive the
    // argument array, which is why every call site here materializes it into a named string first.
    const std::string projectText = projectPath.string();
    const std::array<std::string_view, 2> args = {"--dump-project-oop-llvm", projectText};
    const ExitCode result = driver.run(args, out, err);
    tests.expect(
        result == ExitCode::Success, std::string(label) + ": lowers to LLVM (" + err.str() + ")");
    return out.str();
}

void expectIrContains(
    TestContext& tests, const std::string& ir, std::string_view needle, std::string_view label)
{
    tests.expect(
        ir.find(std::string(needle)) != std::string::npos,
        std::string(label) + ": IR contains '" + std::string(needle) + "'");
}

void expectIrLacks(
    TestContext& tests, const std::string& ir, std::string_view needle, std::string_view label)
{
    tests.expect(
        ir.find(std::string(needle)) == std::string::npos,
        std::string(label) + ": IR does not contain '" + std::string(needle) + "'");
}

std::size_t countOccurrences(const std::string& haystack, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = haystack.find(std::string(needle));
    while (position != std::string::npos)
    {
        ++count;
        position = haystack.find(std::string(needle), position + needle.size());
    }
    return count;
}

// Compile at one optimization level and, when Wine is available, run the result. `expected` is
// the process exit code the fixture is written to produce.
void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& projectPath,
    std::string_view optimizationFlag,
    std::string_view label,
    int expected)
{
    const auto exePath =
        projectPath.parent_path() / (std::string(optimizationFlag.substr(1)) + ".exe");
    const std::string projectText = projectPath.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, optimizationFlag, "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result == ExitCode::Success, std::string(label) + ": compiles (" + err.str() + ")");
    if (result != ExitCode::Success) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string(label) + ": Wine unavailable -- execution skipped");
        return;
    }
    const int observed = runUnderWine(wine, exePath);
    tests.expect(
        observed == expected,
        std::string(label) + ": observed " + std::to_string(observed) + ", expected " +
            std::to_string(expected));
}

void runBoth(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected)
{
    const auto projectPath = writeProject(root, name, source);
    compileAndRun(tests, projectPath, "-O0", std::string(name) + " [-O0]", expected);
    compileAndRun(tests, projectPath, "-O2", std::string(name) + " [-O2]", expected);
}

void reject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expectedFragment)
{
    const auto projectPath = writeProject(root, name, source);
    const std::string projectText = projectPath.string();
    const std::array<std::string_view, 2> args = {"--check-project-oop-hir", projectText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result != ExitCode::Success, std::string(name) + ": is rejected at compile time");
    const std::string diagnostics = err.str() + out.str();
    tests.expect(
        diagnostics.find(std::string(expectedFragment)) != std::string::npos,
        std::string(name) + ": diagnostic mentions '" + std::string(expectedFragment) +
            "' (got: " + diagnostics.substr(0, 240) + ")");
}

// --- 1: function-pointer round trip ------------------------------------------------------------

void testAddressRoundTripThroughNint(TestContext& tests, const std::filesystem::path& root)
{
    // The whole round trip in four lines: bind a static method to an extern C function pointer,
    // spell that address as `nint`, spell it back, call through it. MEASURED before C1: the first
    // cast alone ended the build with "Stage 34 local initialization failed".
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() {
            Adder f = AddImpl;
            nint raw = (nint)f;
            if (raw == 0) { return 1; }
            Adder back = (Adder)raw;
            return back(20, 22);
        }
    }
}
)NS";
    runBoth(tests, root, "RoundTripThroughNint", source, 42);
    const std::string ir =
        dumpIr(tests, root / "RoundTripThroughNint" / "RoundTripThroughNint.nsproj", "RoundTripThroughNint");
    // Exactly one instruction in each direction, which is the point: an address is an address,
    // and the round trip must not go anywhere near the delegate cell machinery.
    expectIrContains(tests, ir, "ptrtoint ptr", "RoundTripThroughNint");
    expectIrContains(tests, ir, "inttoptr i64", "RoundTripThroughNint");
    expectIrLacks(tests, ir, "delegate.code", "RoundTripThroughNint");
}

void testAddressRoundTripThroughRawPointer(TestContext& tests, const std::filesystem::path& root)
{
    // The other spelling of the same trip -- `void*` -- which is the one a C header actually
    // uses. Opaque pointers make both casts free; what matters is that the language allows them.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() {
            Adder f = AddImpl;
            void* p = (void*)f;
            Adder back = (Adder)p;
            return back(20, 22);
        }
    }
}
)NS";
    runBoth(tests, root, "RoundTripThroughVoidPointer", source, 42);
}

void testReinterpretBetweenForeignSignatures(TestContext& tests, const std::filesystem::path& root)
{
    // Roadmap 3.1, "reinterpret explicito entre firmas externas compatibles". This is exactly what
    // a proc-address API forces on every caller: the address arrives untyped and the caller
    // asserts a signature for it. The assertion is unchecked BY DESIGN -- what C1 requires is that
    // it be spelled out as a cast instead of happening implicitly.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    extern C delegate int AlsoTwoInts(int x, int y);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() {
            Adder f = AddImpl;
            AlsoTwoInts g = (AlsoTwoInts)f;
            return g(20, 22);
        }
    }
}
)NS";
    runBoth(tests, root, "ReinterpretForeignSignature", source, 42);
}

void testImplicitConversionBetweenSignaturesStaysRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The negative half of the previous test, and the reason the cast has to be written: two
    // extern C delegate types with the SAME shape are still two types, because on Win64 the
    // calling convention is fixed by `extern C` and what distinguishes them is their signature.
    // Assigning one to the other without a cast is not a round trip, it is a type error.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    extern C delegate int AlsoTwoInts(int x, int y);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() { Adder f = AddImpl; AlsoTwoInts g = f; return g(1, 1); }
    }
}
)NS";
    reject(tests, root, "ImplicitSignatureConversion", source, "cannot initialize local 'g'");
}

void testFunctionPointerReturnFromImport(TestContext& tests, const std::filesystem::path& root)
{
    // Roadmap 3.1, "retorno de function pointers", in its contractual shape: an imported C
    // function whose RETURN type is a function pointer. This is the declaration
    // wglGetProcAddress/SDL_GL_GetProcAddress need; before C1 it was rejected outright with
    // "Stage 47 C ABI does not return delegate values", forcing every binding to declare `nint`
    // and blind-cast at every call site.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int GlFunc(int slot);
    public class Loader {
        extern C from "kernel32.dll" {
            public nint GetModuleHandleA(byte* name);
        }
    }
    public class Api {
        extern C from "opengl32.dll" {
            public GlFunc wglGetProcAddress(byte* name);
        }
    }
    class Program { public static int Main() { return 42; } }
}
)NS";
    const auto projectPath = writeProject(root, "FunctionPointerReturnImport", source);
    const std::string ir = dumpIr(tests, projectPath, "FunctionPointerReturnImport");
    // The import's LLVM prototype returns `ptr` -- a raw function pointer -- and nothing else:
    // no cell, no wrapper, no special case naming the API.
    expectIrContains(tests, ir, "declare ptr @wglGetProcAddress(ptr)", "FunctionPointerReturnImport");
}

void testFunctionPointerParameterAndReturnOnExport(
    TestContext& tests, const std::filesystem::path& root)
{
    // The mirror image, on the export side: NativeSharp code handing a function pointer OUT to a
    // C host, and taking one IN. Roadmap contract cases 3 and 4 ("function pointer parameter",
    // "function pointer return") from the other direction.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static export extern C Adder GetAdder() { return AddImpl; }
        public static export extern C int UseAdder(Adder f, int a, int b) { return f(a, b); }
        public static int Main() {
            Adder f = GetAdder();
            return UseAdder(f, 20, 22);
        }
    }
}
)NS";
    runBoth(tests, root, "ExportFunctionPointerBothWays", source, 42);
    const std::string ir = dumpIr(
        tests, root / "ExportFunctionPointerBothWays" / "ExportFunctionPointerBothWays.nsproj",
        "ExportFunctionPointerBothWays");
    expectIrContains(tests, ir, "define ptr @GetAdder()", "ExportFunctionPointerBothWays");
}

// --- the negatives that used to be silent -----------------------------------------------------

void testOrdinaryDelegateIsNotAnAddress(TestContext& tests, const std::filesystem::path& root)
{
    // An ordinary delegate is a 16-byte heap cell (code + receiver). Casting one to an address
    // would hand out the CELL -- precisely the miscompile Stage 55.x measured on the creation
    // side, where a cell pointer reached C and invoking it executed the cell's bytes. The cast is
    // refused by name, with the fix in the message.
    const std::string source = R"NS(
namespace proj {
    delegate int Managed(int a);
    class Program {
        public static int Impl(int a) { return a; }
        public static int Main() { Managed m = Impl; nint raw = (nint)m; return (int)raw; }
    }
}
)NS";
    reject(
        tests, root, "OrdinaryDelegateToAddress", source,
        "an ordinary delegate is a code+receiver cell, not an address");
}

void testClassReferenceIsNotAFunctionPointer(TestContext& tests, const std::filesystem::path& root)
{
    // The other side of the same wall: an object reference is not a function pointer either, and
    // the diagnostic names the only three things that are.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Fn(int a);
    class Holder { public int v; }
    class Program {
        public static int Main() { raii Holder h = new Holder(); Fn f = (Fn)h; return f(1); }
    }
}
)NS";
    reject(
        tests, root, "ClassReferenceToFunctionPointer", source,
        "an extern C function pointer converts only to and from");
}

// --- 2: import identity ------------------------------------------------------------------------

void testTwoLibrariesMayShareASymbolName(TestContext& tests, const std::filesystem::path& root)
{
    // Roadmap 3.2's own example, and contract case 13. MEASURED before C1: this program made LLVM
    // rename the second declaration to `Init.1` and the link failed with "undefined symbol:
    // Init.1". A PE import table has exactly one slot per symbol NAME, so the two identities
    // cannot both be resolved statically -- these bind by library handle at first call instead,
    // which is what Windows itself does with an ambiguous name.
    const std::string source = R"NS(
namespace proj {
    public class A { extern C from "a.dll" { public int Init(); } }
    public class B { extern C from "b.dll" { public int Init(); } }
    class Program { public static int Main() { return A.Init() + B.Init(); } }
}
)NS";
    const auto projectPath = writeProject(root, "TwoLibrariesSameSymbol", source);
    const std::string ir = dumpIr(tests, projectPath, "TwoLibrariesSameSymbol");
    // Two distinct identities, each with its own address cache, and NEITHER of them declared as
    // the bare `Init` the old code asked LLVM for.
    expectIrContains(tests, ir, "define i32 @__nsc_import_a_dll__Init()", "TwoLibrariesSameSymbol");
    expectIrContains(tests, ir, "define i32 @__nsc_import_b_dll__Init()", "TwoLibrariesSameSymbol");
    expectIrContains(tests, ir, "@__nsc_import_a_dll__Init__address", "TwoLibrariesSameSymbol");
    expectIrContains(tests, ir, "@__nsc_import_b_dll__Init__address", "TwoLibrariesSameSymbol");
    expectIrContains(tests, ir, "declare ptr @LoadLibraryA(ptr)", "TwoLibrariesSameSymbol");
    expectIrContains(tests, ir, "declare ptr @GetProcAddress(ptr, ptr)", "TwoLibrariesSameSymbol");
    expectIrLacks(tests, ir, "@Init.1", "TwoLibrariesSameSymbol");

    // And it LINKS -- the half that actually failed before. The fixture DLLs do not exist on this
    // machine, so the program is not run: binding is lazy, so the failure would only appear at the
    // first call, which is exactly the trade this design makes.
    const auto exePath = projectPath.parent_path() / "identity.exe";
    const std::string identityProjectText = projectPath.string();
    const std::string identityExeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", identityProjectText, "-O0", "-o", identityExeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        "TwoLibrariesSameSymbol: links to a real executable (" + err.str() + ")");
}

void testSingleClaimantStaysStaticallyBound(TestContext& tests, const std::filesystem::path& root)
{
    // The regression guard for the case above: an import whose name only ONE library claims must
    // keep the certified static-by-name path -- resolved through the import library, visible in
    // the PE's import directory, with no LoadLibrary anywhere. The runtime-binding path is a
    // narrow exception, not a new default.
    const std::string source = R"NS(
namespace proj {
    public class User32 { extern C from "user32.dll" { public int GetSystemMetrics(int index); } }
    class Program { public static int Main() { return User32.GetSystemMetrics(0); } }
}
)NS";
    const auto projectPath = writeProject(root, "SingleClaimantStaticImport", source);
    const std::string ir = dumpIr(tests, projectPath, "SingleClaimantStaticImport");
    expectIrContains(tests, ir, "declare i32 @GetSystemMetrics(i32)", "SingleClaimantStaticImport");
    expectIrLacks(tests, ir, "LoadLibraryA", "SingleClaimantStaticImport");
    expectIrLacks(tests, ir, "__nsc_import_", "SingleClaimantStaticImport");

    const auto exePath = projectPath.parent_path() / "static.exe";
    const std::string staticProjectText = projectPath.string();
    const std::string staticExeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", staticProjectText, "-O0", "-o", staticExeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(
        result == ExitCode::Success,
        "SingleClaimantStaticImport: links (" + err.str() + ")");
    tests.expect(
        out.str().find("user32.dll") != std::string::npos,
        "SingleClaimantStaticImport: the PE still imports user32.dll by name");
}

void testConflictingSignaturesForOneIdentityAreRejected(
    TestContext& tests, const std::filesystem::path& root)
{
    // The third component of the triple. Two declarations of the SAME (library, symbol) with
    // different signatures cannot both be right, and silently picking either one would miscompile
    // every call site of the other.
    const std::string source = R"NS(
namespace proj {
    public class A { extern C from "a.dll" { public int Ping(int x); } }
    public class B { extern C from "A.DLL" { public int Ping(); } }
    class Program { public static int Main() { return A.Ping(1) + B.Ping(); } }
}
)NS";
    reject(
        tests, root, "ConflictingImportSignatures", source,
        "an import's identity is (library, symbol, signature)");
}

void testSameIdentityDeclaredTwiceIsAccepted(TestContext& tests, const std::filesystem::path& root)
{
    // The positive control for the check above: the same triple declared twice (two classes, two
    // spellings of the DLL name) is ONE import, not a conflict -- library names compare
    // case-insensitively, exactly as the linker's import-library planner merges them.
    const std::string source = R"NS(
namespace proj {
    public class A { extern C from "kernel32.dll" { public uint GetCurrentProcessId(); } }
    public class B { extern C from "KERNEL32.DLL" { public uint GetCurrentProcessId(); } }
    class Program {
        public static int Main() {
            if (A.GetCurrentProcessId() == B.GetCurrentProcessId()) { return 42; }
            return 1;
        }
    }
}
)NS";
    runBoth(tests, root, "SameIdentityTwice", source, 42);
    const std::string ir =
        dumpIr(tests, root / "SameIdentityTwice" / "SameIdentityTwice.nsproj", "SameIdentityTwice");
    expectIrLacks(tests, ir, "LoadLibraryA", "SameIdentityTwice");
    tests.expect(
        countOccurrences(ir, "declare i32 @GetCurrentProcessId()") == 1,
        "SameIdentityTwice: one identity, one declaration");
}

void testArchiveSymbolCannotShareItsNameWithADll(
    TestContext& tests, const std::filesystem::path& root)
{
    // The one combination the triple cannot rescue, stated as a diagnostic rather than left to
    // fail in the linker: an `extern C { ... }` symbol resolves out of a static archive BY NAME,
    // and nothing downstream can rename it, so it can never share its name with a DLL import.
    const std::string source = R"NS(
namespace proj {
    public class Archive { extern C { public int Init(); } }
    public class Dynamic { extern C from "b.dll" { public int Init(); } }
    class Program { public static int Main() { return Archive.Init() + Dynamic.Init(); } }
}
)NS";
    reject(
        tests, root, "ArchiveSymbolNameClash", source,
        "a statically resolved symbol cannot share its name");
}

// --- 3: callback exception boundary -------------------------------------------------------------

void testCallbackAddressIsAGuardedThunk(TestContext& tests, const std::filesystem::path& root)
{
    // Roadmap 3.6. What C receives is a thunk with the target's exact signature (which since
    // Stage 40.2 already IS the Win64 C ABI), whose only addition is one catchpad. The filter is
    // __nativesharp_eh_filter, so a FOREIGN SEH exception is declined and keeps propagating --
    // the boundary contains this compiler's exceptions, it does not swallow the OS's.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() { Adder f = AddImpl; return f(20, 22); }
    }
}
)NS";
    runBoth(tests, root, "CallbackGuardThunk", source, 42);
    const std::string ir =
        dumpIr(tests, root / "CallbackGuardThunk" / "CallbackGuardThunk.nsproj", "CallbackGuardThunk");
    expectIrContains(tests, ir, "__c_boundary", "CallbackGuardThunk");
    expectIrContains(tests, ir, "catchpad within %boundary.catchswitch [ptr @__nativesharp_eh_filter]", "CallbackGuardThunk");
    expectIrContains(tests, ir, "call void @__nativesharp_trap(i32 13)", "CallbackGuardThunk");
    // "unwind to caller", not to a handler of ours: a foreign exception is none of our business.
    expectIrContains(tests, ir, "unwind to caller", "CallbackGuardThunk");
}

void testTheSameMethodAlwaysYieldsTheSameCallbackAddress(
    TestContext& tests, const std::filesystem::path& root)
{
    // The thunk is cached per target, and it has to be: a host that is handed the same callback
    // twice must see one pointer, or every identity check it does (unregistering a handler,
    // comparing two WndProcs) breaks. One target, one thunk definition in the module.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() {
            Adder first = AddImpl;
            Adder second = AddImpl;
            if ((nint)first != (nint)second) { return 1; }
            return first(20, 22);
        }
    }
}
)NS";
    runBoth(tests, root, "CallbackAddressIsStable", source, 42);
    const std::string ir = dumpIr(
        tests, root / "CallbackAddressIsStable" / "CallbackAddressIsStable.nsproj",
        "CallbackAddressIsStable");
    tests.expect(
        countOccurrences(ir, "__c_boundary\"(i32 %0, i32 %1) #") == 1,
        "CallbackAddressIsStable: one target, exactly one boundary thunk definition");
}

void testExceptionInsideACallbackTrapsInsteadOfEscaping(
    TestContext& tests, const std::filesystem::path& root)
{
    // The contract itself, as a running program: a callback whose body throws, called through its
    // C function pointer. The exception never reaches Main's own handler-free frame -- it is
    // stopped at the boundary and the process exits with the trap's defined code, 128 + 13 = 141
    // (see LlvmTrapReason::CallbackExceptionEscape). Without the boundary this same program would
    // let a NativeSharp SEH exception unwind whatever sits below the callback.
    // The exception base is declared inline, the way every Stage 45 EH fixture declares it, so
    // the project stays self-contained (no ProjectReference to lib/NativeSharp.Core).
    const std::string source = R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
extern C delegate int Failing(int a);
class Boom : Exception { }
class Program {
    public static int Throws(int a) { throw new Boom(); }
    public static int Main() {
        Failing f = Throws;
        return f(1);
    }
}
)NS";
    runBoth(tests, root, "CallbackExceptionTraps", source, 141);
}

void testExportedEntryPointCarriesTheSameBoundary(
    TestContext& tests, const std::filesystem::path& root)
{
    // The other door foreign code enters through. An `export extern C` function is called by a
    // host exactly the way a callback is, so it carries the same hazard and gets the same guard --
    // the wrapper already existed, and what changed is that its call to the internal function is
    // now an invoke with the boundary's catchpad behind it.
    const std::string source = R"NS(
namespace proj {
    class Program {
        public static export extern C int Ping(int x) { return x + 1; }
        public static int Main() { return Ping(41); }
    }
}
)NS";
    runBoth(tests, root, "ExportBoundary", source, 42);
    const std::string ir = dumpIr(tests, root / "ExportBoundary" / "ExportBoundary.nsproj", "ExportBoundary");
    expectIrContains(tests, ir, "define i32 @Ping(i32", "ExportBoundary");
    expectIrContains(tests, ir, "cabi.unwind", "ExportBoundary");
    expectIrContains(tests, ir, "call void @__nativesharp_trap(i32 13)", "ExportBoundary");
}

// --- the contract fixtures on disk ---------------------------------------------------------------

// Roadmap section 5 asks for a tests/interop_contract/ tree that outlives any one suite: the
// contract cases as REAL projects a human can open, build by hand and diff, not only as string
// literals inside a .cpp. These are the C1 rows of that matrix; each one is compiled here from its
// own directory, so the files can never rot into decoration.
void testInteropContractFixturesCompile(TestContext& tests)
{
    const std::filesystem::path contractRoot =
        std::filesystem::path(NSC_SOURCE_DIR) / "tests" / "interop_contract";
    if (!std::filesystem::exists(contractRoot))
    {
        tests.expect(false, "tests/interop_contract exists in the source tree");
        return;
    }
    const std::array<std::string_view, 5> fixtures = {
        "c1_03_function_pointer_parameter",
        "c1_04_function_pointer_return",
        "c1_05_raw_pointer_cast",
        "c1_13_two_libraries_same_symbol",
        "c1_14_callback_exception_boundary"};
    for (const std::string_view fixture : fixtures)
    {
        const auto projectPath =
            contractRoot / std::string(fixture) / (std::string(fixture) + ".nsproj");
        if (!std::filesystem::exists(projectPath))
        {
            tests.expect(false, "contract fixture present: " + std::string(fixture));
            continue;
        }
        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        const std::string projectText = projectPath.string();
        const std::array<std::string_view, 2> args = {
            "--verify-project-oop-llvm", projectText};
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "contract fixture lowers and verifies: " + std::string(fixture) + " (" + err.str() + ")");
    }
}
// The packaged example (examples/project_stage56_c1) is the human-readable half of this round:
// one program that walks every C1 primitive in roadmap order and returns its own numbered exit
// code per failed check. It is compiled here, from the source tree, so it can never drift away
// from the compiler it documents.
void testPackagedExampleCompiles(TestContext& tests)
{
    const auto projectPath = std::filesystem::path(NSC_SOURCE_DIR) / "examples" /
        "project_stage56_c1" / "Stage56C1InteropCompletion.nsproj";
    if (!std::filesystem::exists(projectPath))
    {
        tests.expect(false, "examples/project_stage56_c1 exists in the source tree");
        return;
    }
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c1-example";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);
    for (const std::string_view level : {"-O0", "-O2"})
    {
        const std::string projectText = projectPath.string();
        const std::string exeText = (root / (std::string(level.substr(1)) + ".exe")).string();
        const std::array<std::string_view, 5> args = {
            "--emit-project-oop-exe", projectText, level, "-o", exeText};
        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "packaged example compiles [" + std::string(level) + "] (" + err.str() + ")");
        const std::string wine = wineCommand();
        if (wine.empty())
        {
            tests.expect(true, "packaged example [" + std::string(level) + "]: Wine unavailable -- execution skipped");
            continue;
        }
        tests.expect(
            runUnderWine(wine, exeText) == 0,
            "packaged example runs clean [" + std::string(level) + "]");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c1";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    testAddressRoundTripThroughNint(tests, root);
    testAddressRoundTripThroughRawPointer(tests, root);
    testReinterpretBetweenForeignSignatures(tests, root);
    testImplicitConversionBetweenSignaturesStaysRejected(tests, root);
    testFunctionPointerReturnFromImport(tests, root);
    testFunctionPointerParameterAndReturnOnExport(tests, root);
    testOrdinaryDelegateIsNotAnAddress(tests, root);
    testClassReferenceIsNotAFunctionPointer(tests, root);

    testTwoLibrariesMayShareASymbolName(tests, root);
    testSingleClaimantStaysStaticallyBound(tests, root);
    testConflictingSignaturesForOneIdentityAreRejected(tests, root);
    testSameIdentityDeclaredTwiceIsAccepted(tests, root);
    testArchiveSymbolCannotShareItsNameWithADll(tests, root);

    testCallbackAddressIsAGuardedThunk(tests, root);
    testTheSameMethodAlwaysYieldsTheSameCallbackAddress(tests, root);
    testExceptionInsideACallbackTrapsInsteadOfEscaping(tests, root);
    testExportedEntryPointCarriesTheSameBoundary(tests, root);

    testInteropContractFixturesCompile(tests);
    testPackagedExampleCompiles(tests);

    return tests.result();
}
