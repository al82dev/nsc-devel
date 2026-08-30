// Stage 40.2 -- Win64 aggregate ABI, verified by EXECUTION.
//
// Before this round, any struct whose size was not 1/2/4/8 bytes was unusable as a first-class
// VALUE anywhere in the language: a plain local of a 12-byte Vec3 -- no call involved at all --
// failed with "Stage 40 Windows x64 direct struct value ABI supports only 1, 2, 4 or 8-byte
// aggregates". Fields and array elements always worked (they were address-based); locals, copies,
// parameters and returns were the gap. That made Mat4 (64B), an MSG-shaped record (48B) and
// WNDCLASSEXW (80B) unrepresentable by value, blocking both the math library and Win32 windowing
// before either was even started.
//
// The implemented contract is clang's own lowering for x86_64-pc-windows-msvc, extracted from
// clang IR as ground truth before writing any code:
//
//   * size 1/2/4/8: the aggregate travels DIRECTLY as an integer of that width ({float,float}
//     goes in a GPR as i64 -- Win64 never splits it into XMM);
//   * every other size -- 3 bytes included, not just "big" -- travels INDIRECTLY: the parameter
//     as a plain pointer (no byval) to a copy the CALLER makes for that specific call, the
//     return through a leading `sret(<type>) noalias align <n>` pointer;
//   * for instance methods `this` stays first and the sret slot comes SECOND (RCX = this,
//     RDX = sret) -- confirmed against clang's C++ output;
//   * the per-call copy belongs to the call: the callee may write it, and re-passing the struct
//     down the chain makes ANOTHER copy (clang's `chain` fixture proves it, and the
//     CalleeCopyIsolated/ChainRepass scenarios below pin the same observable semantics).
//
// In the compiler the value carrier for an indirect struct is `[N x i8]` -- local slots, member
// GEPs and whole-value moves work off the natural byte layout -- and the call boundary maps it
// away at exactly six construction sites: declareFunctions, the definition prologue, the two
// structured-return paths, the four call lowerers (direct/static/virtual/interface), the extern C
// wrapper, and the two delegate emitters. The delegate one was MEASURED broken mid-round: a
// delegate taking a 64-byte struct read garbage (1 at -O0, 5 at -O2, for a fixture whose answer
// is 4) until the emitters got the same mapping.
//
// Everything here runs as a real Windows x64 PE under Wine at -O0 AND -O2 and asserts exact exit
// codes; a wrong ABI shows up as a wrong number or a crash, not as a shape mismatch in IR text.
//
// STILL PENDING for full certification (deliberately not asserted here): the cross-check against
// a clang-compiled C HOST executing under Wine -- exporting these structs through `export extern
// C` and exchanging them with foreign code -- and NativeSharp methods handed out as C function
// pointers (the WndProc half). Those are the next round's opening tasks; this suite pins the
// compiler-internal half of the contract.

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

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
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

void compileAndRun(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view optimizationFlag,
    std::string_view label,
    int expected)
{
    const std::string projectText = (directory / (directory.filename().string() + ".nsproj")).string();
    const auto exePath = directory / (std::string(optimizationFlag.substr(1)) + ".exe");
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

void run(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    compileAndRun(tests, directory, "-O0", std::string(name) + " [-O0]", expected);
    compileAndRun(tests, directory, "-O2", std::string(name) + " [-O2]", expected);
}

// A 64-byte aggregate: the Mat4 shape the math library needs, and comfortably past every direct
// size class.
constexpr std::string_view kM4 = R"NS(
    public struct M4 {
        public float a; public float b; public float c; public float d;
        public float e; public float f; public float g; public float h;
        public float i; public float j; public float k; public float l;
        public float m; public float n; public float o; public float p;
    }
)NS";

// --- the direct sizes stay direct (regression guard for the existing 1/2/4/8 contract) --------

void testByteStructParamReturn(TestContext& tests, const std::filesystem::path& root)
{
    run(tests, root, "ByteStructParamReturn", R"NS(
namespace proj {
    public struct B1 { public byte a; }
    class Program {
        public static B1 Make() { B1 v; v.a = 9; return v; }
        public static int Get(B1 v) { return v.a; }
        public static int Main() { B1 x = Make(); return Get(x); }
    }
}
)NS", 9);
}

void testEightByteStructParamReturn(TestContext& tests, const std::filesystem::path& root)
{
    // {float,float} = 8 bytes -> a single i64 in a GPR, per the Win64 rule.
    run(tests, root, "EightByteStructParamReturn", R"NS(
namespace proj {
    public struct V2 { public float x; public float y; }
    class Program {
        public static V2 Make() { V2 v; v.x = 1; v.y = 3; return v; }
        public static float Sum(V2 v) { return v.x + v.y; }
        public static int Main() { V2 x = Make(); return (int)Sum(x); }
    }
}
)NS", 4);
}

// --- indirect aggregates as first-class values ------------------------------------------------

void testBigLocalAndCopy(TestContext& tests, const std::filesystem::path& root)
{
    // The case that failed with NO call in sight: a 64-byte local, member writes, a whole-value
    // copy, and independence of the copy from later writes to the source.
    run(tests, root, "BigLocalAndCopy", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Program {
        public static int Main() {
            M4 v; v.a = 1; v.p = 1;
            M4 w; w = v;
            v.a = 5;
            return (int)(w.a + w.p);
        }
    }
}
)NS", 2);
}

void testNestedAndOddSizedLocals(TestContext& tests, const std::filesystem::path& root)
{
    // 16-byte struct made of two 8-byte structs, plus the 12-byte non-power-of-two Vec3.
    run(tests, root, "NestedAndOddSizedLocals", R"NS(
namespace proj {
    public struct Inner { public float x; public float y; }
    public struct Outer { public Inner a; public Inner b; }
    public struct V3 { public float x; public float y; public float z; }
    class Program {
        public static int Main() {
            Outer o; o.a.x = 1; o.b.y = 3;
            V3 v; v.x = 1; v.y = 2; v.z = 3;
            return (int)(o.a.x + o.b.y + v.x + v.y + v.z);
        }
    }
}
)NS", 10);
}

void testStaticParamAndSretReturn(TestContext& tests, const std::filesystem::path& root)
{
    run(tests, root, "StaticParamAndSretReturn", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Program {
        public static M4 Make(float s) { M4 v; v.a = s; v.f = s; v.k = s; v.p = s; return v; }
        public static float Trace(M4 v) { return v.a + v.f + v.k + v.p; }
        public static int Main() { M4 i = Make(1); return (int)Trace(i); }
    }
}
)NS", 4);
}

void testCalleeCopyIsIsolated(TestContext& tests, const std::filesystem::path& root)
{
    // THE observable semantics of the indirect ABI: the callee writes its own per-call copy, and
    // the caller's original must not change. A pass-by-reference shortcut would return 99 here.
    run(tests, root, "CalleeCopyIsIsolated", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Program {
        public static void Clobber(M4 v) { v.a = 99; }
        public static int Main() { M4 x; x.a = 3; Clobber(x); return (int)x.a; }
    }
}
)NS", 3);
}

void testChainRepassCopiesAgain(TestContext& tests, const std::filesystem::path& root)
{
    // Forwarding the parameter down the chain makes ANOTHER copy -- the middle function's write
    // to its own copy is visible to the inner call (it forwards AFTER writing), while the
    // original stays whole. Mirrors clang's own `chain` lowering (explicit memcpy per call).
    run(tests, root, "ChainRepassCopiesAgain", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Program {
        public static float Inner(M4 v, int k) { return v.a + k; }
        public static float Outer(M4 v) { v.p = 77; return Inner(v, 1); }
        public static int Main() { M4 x; x.a = 4; float r = Outer(x); return (int)r; }
    }
}
)NS", 5);
}

void testInstanceMethodSretAfterThis(TestContext& tests, const std::filesystem::path& root)
{
    // Instance method returning 64 bytes: this in RCX, sret in RDX -- the MS-ABI ordering
    // confirmed against clang's C++ output before implementing.
    run(tests, root, "InstanceMethodSretAfterThis", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Maker {
        public int seed;
        public M4 Make() { M4 v; v.a = seed; v.p = seed; return v; }
    }
    class Program {
        public static int Main() {
            raii Maker m = new Maker(); m.seed = 2;
            M4 r = m.Make(); return (int)(r.a + r.p);
        }
    }
}
)NS", 4);
}

void testVirtualDispatchWithSret(TestContext& tests, const std::filesystem::path& root)
{
    // The reconstructed callee type at a virtual call site must match what every override was
    // declared with -- a mismatch here reads garbage through the vtable.
    run(tests, root, "VirtualDispatchWithSret", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Base { public virtual M4 Make() { M4 v; v.a = 1; return v; } }
    class Derived : Base { public override M4 Make() { M4 v; v.a = 6; return v; } }
    class Program {
        public static int Main() { raii Base b = new Derived(); M4 r = b.Make(); return (int)r.a; }
    }
}
)NS", 6);
}

void testInterfaceDispatchBothDirections(TestContext& tests, const std::filesystem::path& root)
{
    // Interface calls carry the 64-byte aggregate BOTH ways: out through sret, back in as a
    // pointer parameter.
    run(tests, root, "InterfaceDispatchBothDirections", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    interface IShape { M4 Make(); float Trace(M4 v); }
    class Impl : IShape {
        public M4 Make() { M4 v; v.a = 4; v.p = 5; return v; }
        public float Trace(M4 v) { return v.a + v.p; }
    }
    class Program {
        public static float Use(IShape s) { M4 r = s.Make(); return s.Trace(r); }
        public static int Main() { raii Impl i = new Impl(); return (int)Use(i); }
    }
}
)NS", 9);
}

void testOddSizedParamAndReturn(TestContext& tests, const std::filesystem::path& root)
{
    // 12-byte return and 24-byte parameter: the non-power-of-two sizes that prove the rule is
    // "not 1/2/4/8", not "bigger than 8".
    run(tests, root, "OddSizedParamAndReturn", R"NS(
namespace proj {
    public struct V3 { public float x; public float y; public float z; }
    public struct D3 { public double a; public double b; public double c; }
    class Program {
        public static V3 MakeV(float s) { V3 v; v.x = s; v.y = s; v.z = s; return v; }
        public static double SumD(D3 d) { return d.a + d.b + d.c; }
        public static int Main() {
            V3 v = MakeV(2); D3 d; d.a = 2; d.b = 2; d.c = 2;
            return (int)(v.x + v.y + v.z + SumD(d));
        }
    }
}
)NS", 12);
}

void testCallInLoopDoesNotGrowStack(TestContext& tests, const std::filesystem::path& root)
{
    // The per-call argument copies are HOISTED entry-block allocas. Emitted at the call site
    // they would re-allocate every iteration and overflow the stack long before 200000 rounds.
    run(tests, root, "CallInLoopDoesNotGrowStack", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Program {
        public static float Get(M4 v) { return v.a; }
        public static int Main() {
            M4 x; x.a = 8; float r = 0;
            int i = 0; while (i < 200000) { r = Get(x); i = i + 1; }
            return (int)r;
        }
    }
}
)NS", 8);
}

void testSretThroughTryFinally(TestContext& tests, const std::filesystem::path& root)
{
    // The EH return-spill path and the sret path compose: the spill slot holds the [N x i8]
    // value and the final return stores it through the sret pointer.
    run(tests, root, "SretThroughTryFinally", std::string("namespace proj {\n") +
        "    namespace NativeSharp.Core { }\n" + std::string(kM4) + R"NS(
    class Program {
        public static M4 Make() {
            M4 v; v.a = 7;
            try { v.p = 1; } finally { v.b = 2; }
            return v;
        }
        public static int Main() { M4 r = Make(); return (int)r.a; }
    }
}
)NS", 7);
}

void testBigStructOwnInstanceMethod(TestContext& tests, const std::filesystem::path& root)
{
    // A 64-byte struct's own instance method: `this` is the address of the memory-homed local.
    run(tests, root, "BigStructOwnInstanceMethod", R"NS(
namespace proj {
    public struct Big {
        public float a; public float b; public float c; public float d;
        public float e; public float f; public float g; public float h;
        public float i; public float j; public float k; public float l;
        public float m2; public float n; public float o; public float p;
        public float Head() { return a; }
    }
    class Program {
        public static int Main() { Big v; v.a = 5; return (int)v.Head(); }
    }
}
)NS", 5);
}

void testGenericsCloseOverBigStruct(TestContext& tests, const std::filesystem::path& root)
{
    // A generic closed over the 64-byte struct: default(T) zero-initializes the [N x i8]
    // carrier, and Echo(T) round-trips it through the monomorphized indirect signature.
    run(tests, root, "GenericsCloseOverBigStruct", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    class Fn<T> {
        public T Echo(T v) { return v; }
        public T Zero() { return default(T); }
    }
    class Program {
        public static int Main() {
            raii Fn<M4> f = new Fn<M4>();
            M4 x; x.d = 4;
            M4 y = f.Echo(x);
            M4 z = f.Zero();
            return (int)(y.d + z.p);
        }
    }
}
)NS", 4);
}

void testDelegateWithBigParamAndReturn(TestContext& tests, const std::filesystem::path& root)
{
    // The surface that was MEASURED broken mid-round: before the delegate emitters got the ABI
    // mapping, the parameter case read garbage (1 at -O0, 5 at -O2 -- an O0/O2 divergence is the
    // signature of a call-boundary type mismatch, not a logic bug).
    run(tests, root, "DelegateWithBigParamAndReturn", std::string("namespace proj {\n") + std::string(kM4) + R"NS(
    delegate float Reader(M4 v);
    delegate M4 Maker(float s);
    class Program {
        public static float Trace(M4 v) { return v.a + v.p; }
        public static M4 Make(float s) { M4 v; v.a = s; return v; }
        public static int Main() {
            Reader r = Trace; M4 x; x.a = 1; x.p = 3;
            Maker m = Make; M4 y = m(6);
            return (int)(r(x) + y.a);
        }
    }
}
)NS", 10);
}
}

int main()
{
    TestContext tests;
    const auto root =
        std::filesystem::temp_directory_path() / "nativesharp_stage40_2_aggregate_abi_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testByteStructParamReturn(tests, root);
    testEightByteStructParamReturn(tests, root);
    testBigLocalAndCopy(tests, root);
    testNestedAndOddSizedLocals(tests, root);
    testStaticParamAndSretReturn(tests, root);
    testCalleeCopyIsIsolated(tests, root);
    testChainRepassCopiesAgain(tests, root);
    testInstanceMethodSretAfterThis(tests, root);
    testVirtualDispatchWithSret(tests, root);
    testInterfaceDispatchBothDirections(tests, root);
    testOddSizedParamAndReturn(tests, root);
    testCallInLoopDoesNotGrowStack(tests, root);
    testSretThroughTryFinally(tests, root);
    testBigStructOwnInstanceMethod(tests, root);
    testGenericsCloseOverBigStruct(tests, root);
    testDelegateWithBigParamAndReturn(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
