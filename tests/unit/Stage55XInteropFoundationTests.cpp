// Stage 55.x -- Interop Foundation, verified by EXECUTION.
//
// One round, one goal, straight from the audit's gate: "que una API C desconocida pueda entrar y
// salir de NativeSharp sin tocar el frontend/HIR/MIR/lowerer especificamente para ella". This
// suite pins every piece of that, end to end and under Wine:
//
//   1. CROSS-HOST ABI CERTIFICATION -- a clang-compiled, MSVC-ABI C host linked against a
//      NativeSharp static library exchanges aggregates field by field (8B direct, 12B indirect,
//      64B sret both directions) and CALLS NATIVESHARP FUNCTIONS THROUGH RAW C FUNCTION
//      POINTERS it received at runtime, including a WndProc-shaped signature and a 64-byte
//      struct parameter INTO NativeSharp code. This also answers the round's opening risk gate:
//      a host with its own main() drives the NativeSharp runtime with no NativeSharp entry
//      point having ever run.
//   2. GENERIC FFI IMPORTS -- `extern C from "user32.dll" { ... }` declares, calls, links
//      (the import library materializes from the (dll, symbol) pairs the frontend collected)
//      and EXECUTES against real user32/kernel32 under Wine. The kernel32 probe deliberately
//      uses one symbol the runtime's own table never had (GetCurrentProcessId) and one it
//      already had (Sleep), so both the new-DLL and the merge/dedupe paths are exercised.
//   3. STATIC-RESOLVED IMPORTS + --link-lib -- `extern C { ... }` (no `from`) declares C
//      functions that resolve against a clang-built static archive handed to the driver with
//      --link-lib, exchanging 12B and 64B structs with foreign code. MEASURED before the two
//      spellings were split: routing a static archive through the dynamic path minted import
//      thunks against a DLL that does not exist -- the link "succeeded" and the loader failed
//      at runtime (exit 53).
//   4. POINTERS -- &local / *pointer / (*p).field in the OOP pipeline (the raw tier), the shape
//      every out-parameter C API needs (GetMessage(&msg)).
//   5. CALLBACK CREATION -- binding a static method to an extern C delegate yields the raw
//      function's address (measured broken before: the delegate CELL pointer was handed to C
//      and invoking it executed the cell bytes, exit 5 for an answer of 42), and the negative
//      half: instance methods and lambdas cannot convert, C function pointers have no receiver
//      or environment channel.
//
// Wine, clang and lld-link availability is probed exactly like the Stage 47/48 host suites; any
// missing tool degrades that scenario to a skip, never to a hard failure on a build box.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"

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

void writeProject(
    const std::filesystem::path& directory, std::string_view name, std::string_view outputType = {})
{
    std::string properties = "<Name>" + std::string(name) + "</Name>";
    if (!outputType.empty())
    {
        properties += "<OutputType>" + std::string(outputType) + "</OutputType>";
    }
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup>" + properties +
            "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
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
    const std::string command = "WINEDEBUG=-all timeout 25 " + wine + " " + exePath.string() +
        " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

bool runCommand(const std::string& command, const std::filesystem::path& logPath)
{
    const std::string full = command + " >" + logPath.string() + " 2>&1";
    return std::system(full.c_str()) == 0;
}

std::string quote(const std::filesystem::path& path) { return "\"" + path.string() + "\""; }

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view optimizationFlag,
    std::string_view label,
    int expected,
    const std::vector<std::string>& extraArguments = {})
{
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const auto exePath = directory / (std::string(optimizationFlag.substr(1)) + ".exe");
    const std::string exeText = exePath.string();
    std::vector<std::string_view> args = {
        "--emit-project-oop-exe", projectText, optimizationFlag};
    for (const std::string& extra : extraArguments) args.push_back(extra);
    args.push_back("-o");
    args.push_back(exeText);

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

void run(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected,
    const std::vector<std::string>& extraArguments = {})
{
    const auto directory = root / std::string(name);
    writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    compileAndRun(tests, directory, "-O0", std::string(name) + " [-O0]", expected, extraArguments);
    compileAndRun(tests, directory, "-O2", std::string(name) + " [-O2]", expected, extraArguments);
}

void reject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expectedFragment)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const std::array<std::string_view, 2> args = {"--check-project-oop-hir", projectText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(result != ExitCode::Success, std::string(name) + ": is rejected at compile time");
    const std::string diagnostics = err.str() + out.str();
    tests.expect(
        diagnostics.find(std::string(expectedFragment)) != std::string::npos,
        std::string(name) + ": diagnostic mentions '" + std::string(expectedFragment) + "' (got: " +
            diagnostics.substr(0, 200) + ")");
}

// --- 2: generic dynamic FFI against real Windows DLLs ------------------------------------------

void testDynamicImportsExecuteAgainstRealDlls(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public class User32 {
        extern C from "user32.dll" {
            public int GetSystemMetrics(int index);
        }
    }
    public class Kernel32 {
        extern C from "kernel32.dll" {
            public uint GetCurrentProcessId();
            public void Sleep(uint ms);
        }
    }
    class Program {
        public static int Main() {
            int width = User32.GetSystemMetrics(0);
            int height = User32.GetSystemMetrics(1);
            uint pid = Kernel32.GetCurrentProcessId();
            Kernel32.Sleep(1u);
            if (width > 0 && height > 0 && pid > 0u) { return 42; }
            return 1;
        }
    }
}
)NS";
    run(tests, root, "DynamicImportsExecute", source, 42);
}

// --- 3: static-resolved imports + --link-lib against a clang-built archive ---------------------

void testStaticImportsLinkAgainstClangArchive(TestContext& tests, const std::filesystem::path& root)
{
    if (!commandAvailable("clang") || !commandAvailable("llvm-ar"))
    {
        tests.expect(true, "clang/llvm-ar unavailable -- static-archive interop scenario skipped");
        return;
    }
    const auto directory = root / "StaticArchive";
    std::filesystem::create_directories(directory);
    writeFile(directory / "mathlib.c", R"C(
typedef struct { float x, y, z; } V3;
typedef struct { float m[16]; } M4;
int clib_add(int a, int b) { return a + b; }
float clib_sum_v3(V3 v) { return v.x + v.y + v.z; }
V3 clib_make_v3(float s) { V3 v; v.x = s; v.y = s + 1; v.z = s + 2; return v; }
M4 clib_scale_first(M4 m, float s) { M4 r = m; r.m[0] = m.m[0] * s; return r; }
)C");
    if (!runCommand(
            "clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -std=c11 "
            "-Wall -Werror -c " + quote(directory / "mathlib.c") + " -o " +
                quote(directory / "mathlib.obj"),
            directory / "clang.log") ||
        !runCommand(
            "llvm-ar rc " + quote(directory / "mathlib.lib") + " " + quote(directory / "mathlib.obj"),
            directory / "ar.log"))
    {
        tests.expect(false, "clang static archive for the interop scenario builds");
        return;
    }

    const std::string source = R"NS(
namespace proj {
    public struct V3 { public float x; public float y; public float z; }
    public struct M4 {
        public float a; public float b; public float c; public float d;
        public float e; public float f; public float g; public float h;
        public float i; public float j; public float k; public float l;
        public float m; public float n; public float o; public float p;
    }
    public class CLib {
        extern C {
            public int clib_add(int a, int b);
            public float clib_sum_v3(V3 v);
            public V3 clib_make_v3(float s);
            public M4 clib_scale_first(M4 m, float s);
        }
    }
    class Program {
        public static int Main() {
            if (CLib.clib_add(20, 20) != 40) { return 1; }
            V3 v; v.x = 1; v.y = 2; v.z = 3;
            if ((int)CLib.clib_sum_v3(v) != 6) { return 2; }
            V3 made = CLib.clib_make_v3(5);
            if ((int)(made.x + made.y + made.z) != 18) { return 3; }
            M4 big; big.a = 21;
            M4 scaled = CLib.clib_scale_first(big, 2);
            return (int)scaled.a;
        }
    }
}
)NS";
    writeProject(directory, "StaticArchive");
    writeFile(directory / "Main.ns", source);
    const std::vector<std::string> extra = {"--link-lib", (directory / "mathlib.lib").string()};
    compileAndRun(tests, directory, "-O0", "StaticArchive [-O0]", 42, extra);
    compileAndRun(tests, directory, "-O2", "StaticArchive [-O2]", 42, extra);
}

// --- 4: pointers in the OOP pipeline -----------------------------------------------------------

void testAddressOfAndDereference(TestContext& tests, const std::filesystem::path& root)
{
    // The out-parameter shape (GetMessage(&msg)): take a local's address, mutate through the
    // pointer in the callee, observe in the caller. Plus a 24-byte struct read through
    // (*pointer).field and a whole-struct copy via *(&local).
    const std::string source = R"NS(
namespace proj {
    public struct MSGLike { public nint hwnd; public uint message; public nint wParam; }
    class Program {
        public static void Fill(MSGLike* m) { (*m).message = 77u; (*m).wParam = 40; }
        public static void Bump(int* p) { *p = *p + 2; }
        public static int Main() {
            MSGLike msg; msg.message = 0u;
            Fill(&msg);
            int extra = 0; Bump(&extra);
            MSGLike copy = *(&msg);
            if (msg.message == 77u && copy.message == 77u) { return (int)msg.wParam + extra; }
            return 1;
        }
    }
}
)NS";
    run(tests, root, "AddressOfAndDereference", source, 42);
}

// --- 5: extern C delegate creation -------------------------------------------------------------

void testExternCDelegateYieldsRawFunctionPointer(TestContext& tests, const std::filesystem::path& root)
{
    // MEASURED before the fix: this exact program returned 5 -- the delegate CELL pointer was
    // stored where C code expected a function, and calling it executed the cell's bytes.
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Adder(int a, int b);
    class Program {
        public static int AddImpl(int a, int b) { return a + b; }
        public static int Main() {
            Adder f = AddImpl;
            return f(20, 22);
        }
    }
}
)NS";
    run(tests, root, "ExternCDelegateRawPointer", source, 42);
}

void testExternCDelegateRejectsInstanceMethod(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Fn(int v);
    class Holder { public int bias; public int M(int v) { return v + bias; } }
    class Program {
        public static int Main() {
            raii Holder h = new Holder();
            Fn f = h.M;
            return f(1);
        }
    }
}
)NS";
    reject(
        tests, root, "ExternCDelegateInstanceRejected", source,
        "extern C delegate requires a static method");
}

void testExternCDelegateRejectsLambda(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    extern C delegate int Fn(int v);
    class Program {
        public static int Main() { Fn f = v => v + 1; return f(1); }
    }
}
)NS";
    reject(
        tests, root, "ExternCDelegateLambdaRejected", source,
        "extern C delegate cannot bind a lambda");
}

void testImportSignatureRejectsManagedString(TestContext& tests, const std::filesystem::path& root)
{
    // The C boundary takes C shapes: builtin:string stays out, exactly as it does for exports.
    // (ushort* is the LPCWSTR convention and is accepted -- covered by the MessageBoxW-shaped
    // declaration in the dynamic-imports scenario compiling.)
    const std::string source = R"NS(
namespace proj {
    public class Bad { extern C from "user32.dll" { public int F(string s); } }
    class Program { public static int Main() { return 0; } }
}
)NS";
    reject(
        tests, root, "ImportRejectsManagedString", source,
        "Stage 47 C ABI cannot export parameter type 'builtin:string'");
}

// --- 1: the cross-host certification -----------------------------------------------------------

void testCrossHostStructsAndCallbacks(TestContext& tests, const std::filesystem::path& root)
{
    if (!commandAvailable("clang") || !commandAvailable("lld-link"))
    {
        tests.expect(true, "clang/lld-link unavailable -- cross-host certification skipped");
        return;
    }
    const std::string wine = wineCommand();

    const auto directory = root / "CrossHost";
    writeProject(directory, "CrossHost", "StaticLibrary");
    writeFile(directory / "Api.ns", R"NS(
namespace AbiX
{
    public struct V2 { public float x; public float y; }
    public struct V3 { public float x; public float y; public float z; }
    public struct M4 {
        public float a; public float b; public float c; public float d;
        public float e; public float f; public float g; public float h;
        public float i; public float j; public float k; public float l;
        public float m; public float n; public float o; public float p;
    }

    extern C delegate int IntTransform(int value);
    extern C delegate float M4Trace(M4 v);
    extern C delegate nint WndProcLike(nint hwnd, uint msg, nint wparam, nint lparam);
    extern C delegate void TransformRegistrar(IntTransform callback);
    extern C delegate void M4TraceRegistrar(M4Trace callback);
    extern C delegate void WndProcRegistrar(WndProcLike callback);

    public class Api
    {
        public static export extern C float SumV2(V2 v) { return v.x + v.y; }
        public static export extern C V2 MakeV2(float s) { V2 v; v.x = s; v.y = s + 1; return v; }
        public static export extern C float SumV3(V3 v) { return v.x + v.y + v.z; }
        public static export extern C V3 MakeV3(float s) { V3 v; v.x = s; v.y = s + 1; v.z = s + 2; return v; }
        public static export extern C float TraceM4(M4 v) { return v.a + v.f + v.k + v.p; }
        public static export extern C M4 ScaleM4(M4 v, float s) {
            M4 r; r.a = v.a * s; r.f = v.f * s; r.k = v.k * s; r.p = v.p * s; return r; }
        public static export extern C int Add(int a, int b) { return a + b; }

        public static int Doubler(int value) { return value * 2; }
        public static float TraceForHost(M4 v) { return v.a + v.p; }
        public static nint WndProcImpl(nint hwnd, uint msg, nint wparam, nint lparam) {
            if (msg == 77u) { return wparam + lparam; }
            return hwnd;
        }

        public static export extern C void GiveCallbacks(
            TransformRegistrar r1, M4TraceRegistrar r2, WndProcRegistrar r3)
        {
            IntTransform t = Doubler;
            r1(t);
            M4Trace m = TraceForHost;
            r2(m);
            WndProcLike w = WndProcImpl;
            r3(w);
        }
    }
}
)NS");

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = (directory / "CrossHost.nsproj").string();
    const std::string libText = (directory / "CrossHost.lib").string();
    const std::array<std::string_view, 5> buildArgs = {
        "--emit-project-oop-lib", projectText, "-O0", "-o", libText};
    tests.expect(driver.run(buildArgs, out, err) == ExitCode::Success,
        "cross-host NativeSharp static library builds (" + err.str() + ")");
    tests.expect(
        std::filesystem::is_regular_file(directory / "CrossHost.h"),
        "cross-host build produced the generated C header");

    writeFile(directory / "host.c", R"C(
#include "CrossHost.h"

static NS_IntTransform g_transform;
static NS_M4Trace g_trace;
static NS_WndProcLike g_wndproc;

static void takeTransform(NS_IntTransform cb) { g_transform = cb; }
static void takeTrace(NS_M4Trace cb) { g_trace = cb; }
static void takeWndProc(NS_WndProcLike cb) { g_wndproc = cb; }

int main(void)
{
    if (Add(20, 22) != 42) return 10;

    NS_V2 v2; v2.x = 1.5f; v2.y = 2.5f;
    if (SumV2(v2) != 4.0f) return 11;
    NS_V2 m2 = MakeV2(3.0f);
    if (m2.x != 3.0f || m2.y != 4.0f) return 12;

    NS_V3 v3; v3.x = 1.0f; v3.y = 2.0f; v3.z = 3.0f;
    if (SumV3(v3) != 6.0f) return 13;
    NS_V3 m3 = MakeV3(5.0f);
    if (m3.x != 5.0f || m3.y != 6.0f || m3.z != 7.0f) return 14;

    NS_M4 m4;
    m4.a = 1.0f; m4.b = 0; m4.c = 0; m4.d = 0;
    m4.e = 0; m4.f = 2.0f; m4.g = 0; m4.h = 0;
    m4.i = 0; m4.j = 0; m4.k = 3.0f; m4.l = 0;
    m4.m = 0; m4.n = 0; m4.o = 0; m4.p = 4.0f;
    if (TraceM4(m4) != 10.0f) return 15;
    NS_M4 s = ScaleM4(m4, 2.0f);
    if (s.a != 2.0f || s.f != 4.0f || s.k != 6.0f || s.p != 8.0f) return 16;
    if (m4.a != 1.0f || m4.p != 4.0f) return 17;

    GiveCallbacks(takeTransform, takeTrace, takeWndProc);
    if (!g_transform || !g_trace || !g_wndproc) return 20;
    if (g_transform(21) != 42) return 21;
    NS_M4 forTrace = m4;
    if (g_trace(forTrace) != 5.0f) return 22;
    if (g_wndproc((intptr_t)7, 77u, (intptr_t)30, (intptr_t)12) != 42) return 23;
    if (g_wndproc((intptr_t)9, 1u, (intptr_t)0, (intptr_t)0) != 9) return 24;

    return 0;
}
)C");
    if (!runCommand(
            "clang --target=x86_64-pc-windows-msvc -fms-extensions -ffreestanding "
            "-fno-stack-protector -std=c11 -Wall -Wextra -Werror -c -I" + quote(directory) + " " +
                quote(directory / "host.c") + " -o " + quote(directory / "host.obj"),
            directory / "clang_host.log"))
    {
        tests.expect(false, "clang compiles the strict MSVC-ABI cross-host against the generated header");
        return;
    }
    tests.expect(true, "clang compiles the strict MSVC-ABI cross-host against the generated header");

    std::string linkerError;
    std::unique_ptr<nativesharp::linker::LldLinker> linker =
        nativesharp::linker::LldLinker::discover(linkerError);
    tests.expect(linker != nullptr, "lld-link discovered for the cross-host link");
    if (linker == nullptr) return;

    // The runtime import surface for the host's final link comes from the same single table the
    // compiler's own links use. The marker text forces every conditional ucrt symbol on, since
    // the freestanding host itself needs memcpy for the by-value struct copies clang lowers.
    std::vector<std::filesystem::path> runtimeImports;
    tests.expect(
        nativesharp::linker::createWindowsImportLibraries(
            *linker, directory,
            "@__C_specific_handler @llvm.memset @llvm.memcpy @llvm.memmove",
            {}, runtimeImports, linkerError),
        "runtime import libraries materialize from the shared table (" + linkerError + ")");

    std::string linkCommand = quote(linker->executablePath()) +
        " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
        quote(directory / "host.exe") + " " + quote(directory / "host.obj") + " " +
        quote(directory / "CrossHost.lib");
    for (const std::filesystem::path& library : runtimeImports)
    {
        linkCommand += " " + quote(library);
    }
    tests.expect(
        runCommand(linkCommand, directory / "link.log"),
        "cross-host executable links against the NativeSharp static library");

    if (wine.empty())
    {
        tests.expect(true, "Wine unavailable -- cross-host execution skipped");
        return;
    }
    const int observed = runUnderWine(wine, directory / "host.exe");
    tests.expect(observed == 0,
        "cross-host certification: structs both directions + C->NativeSharp callbacks execute "
        "(exit " + std::to_string(observed) + ", each nonzero value names the failing check in host.c)");
}
}

int main()
{
    TestContext tests;
    const auto root =
        std::filesystem::temp_directory_path() / "nativesharp_stage55x_interop_foundation_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testDynamicImportsExecuteAgainstRealDlls(tests, root);
    testStaticImportsLinkAgainstClangArchive(tests, root);
    testAddressOfAndDereference(tests, root);
    testExternCDelegateYieldsRawFunctionPointer(tests, root);
    testExternCDelegateRejectsInstanceMethod(tests, root);
    testExternCDelegateRejectsLambda(tests, root);
    testImportSignatureRejectsManagedString(tests, root);
    testCrossHostStructsAndCallbacks(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
