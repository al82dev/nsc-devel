// Stage 56 / C2-C3-C4 -- C aggregate completeness, FFI aliases, language references.
//
// Phases C2, C3 and C4 of the compiler roadmap (01_Compiler_Roadmap.md, sections 3.3, 3.4, 3.5
// and 3.7). Where C1 made a C FUNCTION expressible, these three make a C DECLARATION expressible:
//
//   C2.1 FIXED ARRAYS (roadmap 3.3). `float rgba[4]` inside a struct: layout, alignment, element
//        access, and the same access for a raw pointer. MEASURED before this round: the TYPE
//        parsed and laid out correctly, but `c.rgba[i]` was rejected with "index expression
//        requires an indexer reference or a pointer with one integral index" -- and the pointer
//        half of that very message was also untrue, because `p[0]` passed that check and then
//        died in HIR with "Stage 38 OOP lowering requires class/interface index expressions to
//        resolve to an indexer accessor". Neither shape had a lowering.
//
//   C2.2 UNION AND EXPLICIT LAYOUT (roadmap 3.4). `union`, `pack(N)`, `align(N)`, and per-field
//        `at(N)` offsets -- the four things a hand-laid-out C record says. Win32's INPUT needs
//        the first, a wire format needs the rest.
//
//   C3   FFI ALIASES (roadmap 3.5). `using DWORD = uint;`, `using HANDLE = void*;`,
//        `using Rgba = float[4];`, chaining, and cycle detection. MEASURED before this round:
//        the alias target could only be a qualified NAME, so all three of those were unwritable.
//
//   C4   REF / OUT / IN (roadmap 3.7). By-reference parameters as rules on top of the pointer
//        machinery C1 certified, with the keyword required at the call site, `in` read-only, and
//        `out` carrying a real obligation.
//
// CROSS-TOOLCHAIN CERTIFICATION WITHOUT WINE. The layout half of C2 is verified here in the way
// that actually proves it: the generated C host header is compiled by CLANG for the MSVC ABI with
// -Werror, and the header's own NS_STATIC_ASSERTs check size, alignment and every field offset
// against the numbers NativeSharp computed. If NativeSharp and clang disagreed about a fixed
// array, a union, a pack or an alignment, that compile fails. This runs on any machine with
// clang -- no Wine needed -- so it is a HARD check, not a skip.

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
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view outputType = {})
{
    const auto directory = root / std::string(name);
    std::string properties = "<Name>" + std::string(name) + "</Name>";
    if (!outputType.empty()) properties += "<OutputType>" + std::string(outputType) + "</OutputType>";
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup>" + properties +
            "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
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

std::string dumpAction(
    TestContext& tests,
    std::string_view action,
    const std::filesystem::path& projectPath,
    std::string_view label)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = projectPath.string();
    const std::array<std::string_view, 2> args = {action, projectText};
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        std::string(label) + ": " + std::string(action) + " succeeds (" + err.str() + ")");
    return out.str();
}

void expectContains(
    TestContext& tests, const std::string& text, std::string_view needle, std::string_view label)
{
    tests.expect(
        text.find(std::string(needle)) != std::string::npos,
        std::string(label) + ": output contains '" + std::string(needle) + "'");
}

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
    tests.expect(
        runUnderWine(wine, exePath) == expected,
        std::string(label) + ": exits with " + std::to_string(expected));
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
    tests.expect(
        driver.run(args, out, err) != ExitCode::Success,
        std::string(name) + ": is rejected at compile time");
    const std::string diagnostics = err.str() + out.str();
    tests.expect(
        diagnostics.find(std::string(expectedFragment)) != std::string::npos,
        std::string(name) + ": diagnostic mentions '" + std::string(expectedFragment) +
            "' (got: " + diagnostics.substr(0, 240) + ")");
}

// --- C2.1: fixed arrays -------------------------------------------------------------------------

void testFixedArrayFieldLayoutAndAccess(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public struct Color { public float[4] rgba; public int tag; }
    class Program {
        public static int Main() {
            Color c;
            c.tag = 1;
            c.rgba[0] = 1;
            c.rgba[3] = 41;
            if (c.tag != 1) { return 1; }
            return (int)(c.rgba[0] + c.rgba[3]);
        }
    }
}
)NS";
    runBoth(tests, root, "FixedArrayField", source, 42);
    const std::string layout = dumpAction(
        tests, "--dump-project-layouts", root / "FixedArrayField" / "FixedArrayField.nsproj",
        "FixedArrayField");
    // The layout a C compiler would give the same declaration: 4 floats end to end, then the int.
    // The type key carries the COUNT now ("builtin:float[4]"), which is what made the element
    // access below expressible at all -- it used to be an opaque fingerprint of the count's syntax.
    expectContains(tests, layout, "type=builtin:float[4] offset=0 size=16 align=4", "FixedArrayField");
    expectContains(tests, layout, "name=tag type=builtin:int offset=16", "FixedArrayField");
    expectContains(tests, layout, "size=20 align=4", "FixedArrayField");
}

void testFixedArrayLocalAndPointerIndexing(TestContext& tests, const std::filesystem::path& root)
{
    // A fixed array LOCAL is one alloca of the whole thing, and a raw pointer indexes with the
    // same arithmetic and the same absence of a bounds check. Both are the raw tier, on purpose:
    // it is the tier C APIs are written in.
    const std::string source = R"NS(
namespace proj {
    class Program {
        public static int Main() {
            int[4] slots;
            slots[0] = 20;
            slots[3] = 22;
            int* first = &slots[0];
            int* last = &slots[3];
            return first[0] + last[0];
        }
    }
}
)NS";
    runBoth(tests, root, "FixedArrayLocal", source, 42);
}

void testFixedArrayCannotCrossACallByValue(TestContext& tests, const std::filesystem::path& root)
{
    // C decays an array parameter to a pointer and refuses to return one at all. NativeSharp says
    // so out loud instead of silently copying 16 bytes.
    const std::string source = R"NS(
namespace proj {
    class Program {
        public static void Take(float[4] values) { }
        public static int Main() { return 0; }
    }
}
)NS";
    reject(
        tests, root, "FixedArrayParameter", source,
        "a fixed array cannot be passed by value");
}

// --- C2.2: union and explicit layout ------------------------------------------------------------

void testUnionPackAlignAndExplicitOffsets(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public union Val { public int i; public float f; public byte[4] bytes; }
    public pack(1) struct Packed { public byte tag; public int value; }
    public align(16) struct Aligned { public int x; }
    public struct Overlap { public int a; public at(0) int b; public at(8) long c; }
    class Program {
        public static int Main() {
            Val v;
            v.i = 0;
            v.bytes[0] = 42;
            if (v.i != 42) { return 1; }
            Overlap o;
            o.a = 7;
            if (o.b != 7) { return 2; }
            Packed p;
            p.tag = 1;
            p.value = 42;
            return (int)p.value;
        }
    }
}
)NS";
    runBoth(tests, root, "UnionAndLayout", source, 42);
    const std::string layout = dumpAction(
        tests, "--dump-project-layouts", root / "UnionAndLayout" / "UnionAndLayout.nsproj",
        "UnionAndLayout");
    // Each of these numbers is what a C compiler produces for the same declaration -- and the
    // header scenario below has clang confirm exactly that, rather than leaving it asserted here.
    expectContains(tests, layout, "proj.Val size=4 align=4", "UnionAndLayout");
    expectContains(tests, layout, "name=f type=builtin:float offset=0", "UnionAndLayout");
    expectContains(tests, layout, "proj.Packed size=5 align=1", "UnionAndLayout");
    expectContains(tests, layout, "name=value type=builtin:int offset=1", "UnionAndLayout");
    expectContains(tests, layout, "proj.Aligned size=16 align=16", "UnionAndLayout");
    expectContains(tests, layout, "name=b type=builtin:int offset=0", "UnionAndLayout");
    expectContains(tests, layout, "name=c type=builtin:long offset=8", "UnionAndLayout");
}

void testLayoutModifiersAreRefusedWhereTheyMeanNothing(
    TestContext& tests, const std::filesystem::path& root)
{
    // A class is a heap object with a header this compiler owns; there is no field placement to
    // negotiate, so accepting the modifier there would make it a decoration nobody honours.
    reject(
        tests, root, "PackOnClass",
        R"NS(
namespace proj {
    public pack(1) class Bad { public int x; }
    class Program { public static int Main() { return 0; } }
}
)NS",
        // El mensaje enumera los modificadores, asi que crece cuando se anade uno: desde que
        // existe `size(N)` dice "pack/align/size". Se comprueba el trozo estable --lo que la regla
        // AFIRMA-- y no la lista, para que anadir un cuarto modificador no vuelva a poner roja
        // esta prueba sin que nada este mal.
        "layout modifiers apply only to struct and union declarations");

    reject(
        tests, root, "OffsetOnStaticField",
        R"NS(
namespace proj {
    public struct Bad { public at(4) static int x; }
    class Program { public static int Main() { return 0; } }
}
)NS",
        "an explicit field offset applies only to an instance field");
}

void testGeneratedHostHeaderMatchesClangForEveryLayoutShape(
    TestContext& tests, const std::filesystem::path& root)
{
    // THE certification for C2, and the reason this suite is not a set of assertions about itself:
    // NativeSharp emits the C declaration of each aggregate together with NS_STATIC_ASSERTs for
    // its size, its alignment and every field offset -- then CLANG, targeting the MSVC ABI with
    // -Werror, is asked to agree. A disagreement about a fixed array, a union, a pack or an
    // alignment is a failed compile, not a passing test with a wrong number in it.
    if (!commandAvailable("clang"))
    {
        tests.expect(true, "clang unavailable -- host-header layout certification skipped");
        return;
    }
    const std::string source = R"NS(
namespace proj {
    public struct Color { public float[4] rgba; public int tag; }
    public struct Uuid { public byte[16] bytes; }
    public union Val { public int i; public float f; }
    public pack(1) struct Packed { public byte tag; public int value; }
    public align(16) struct Aligned { public int x; }
    class Api {
        public static export extern C float SumColor(Color c) { return c.rgba[0] + c.rgba[3]; }
        public static export extern C int PickVal(Val v) { return v.i; }
        public static export extern C int TightValue(Packed p) { return (int)p.value; }
        public static export extern C int WideValue(Aligned a) { return a.x; }
        public static export extern C int FirstByte(Uuid u) { return (int)u.bytes[0]; }
    }
}
)NS";
    const auto projectPath = writeProject(root, "HostHeaderLayout", source, "StaticLibrary");
    const auto directory = projectPath.parent_path();
    const auto headerPath = directory / "hostlayout.h";
    const std::string projectText = projectPath.string();
    const std::string headerText = headerPath.string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", headerText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        "HostHeaderLayout: header generation succeeds (" + err.str() + ")");
    if (!std::filesystem::exists(headerPath))
    {
        tests.expect(false, "HostHeaderLayout: header file exists");
        return;
    }
    std::ostringstream headerBuffer;
    headerBuffer << std::ifstream(headerPath, std::ios::binary).rdbuf();
    const std::string header = headerBuffer.str();
    expectContains(tests, header, "float rgba[4];", "HostHeaderLayout");
    expectContains(tests, header, "uint8_t bytes[16];", "HostHeaderLayout");
    expectContains(tests, header, "union NS_Val", "HostHeaderLayout");
    expectContains(tests, header, "typedef union NS_Val NS_Val;", "HostHeaderLayout");
    expectContains(tests, header, "#pragma pack(push, 1)", "HostHeaderLayout");
    expectContains(tests, header, "#pragma pack(pop)", "HostHeaderLayout");
    expectContains(tests, header, "NS_ALIGNAS(16)", "HostHeaderLayout");

    writeFile(directory / "host.c", "#include \"hostlayout.h\"\nint main(void) { return 0; }\n");
    const std::string command =
        "clang --target=x86_64-pc-windows-msvc -ffreestanding -std=c11 -Wall -Wextra -Werror -c \"" +
        (directory / "host.c").string() + "\" -o \"" + (directory / "host.obj").string() +
        "\" -I \"" + directory.string() + "\" > \"" + (directory / "clang.log").string() + "\" 2>&1";
    tests.expect(
        std::system(command.c_str()) == 0,
        "HostHeaderLayout: clang agrees with every size/alignment/offset assertion (MSVC ABI, -Werror)");
}

void testExplicitOffsetsAreRefusedAtTheHostBoundary(
    TestContext& tests, const std::filesystem::path& root)
{
    // A plain C struct declaration cannot express overlapping fields, so emitting one would
    // produce a header whose own assertions fail on the host. Refusing it here, with the reason
    // and the alternative, is the honest outcome.
    const std::string source = R"NS(
namespace proj {
    public struct Overlap { public int a; public at(0) float b; }
    class Api { public static export extern C int Pick(Overlap o) { return o.a; } }
}
)NS";
    const auto projectPath = writeProject(root, "HostHeaderOverlap", source, "StaticLibrary");
    const std::string projectText = projectPath.string();
    const std::string headerText = (projectPath.parent_path() / "overlap.h").string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", headerText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) != ExitCode::Success,
        "HostHeaderOverlap: explicit offsets are refused at the C boundary");
    expectContains(
        tests, err.str() + out.str(), "expose it as a union across the C boundary",
        "HostHeaderOverlap");
}

// --- C5: the foreign-ABI size matrix --------------------------------------------------------------

void testForeignAbiSizeMatrixMatchesClang(TestContext& tests, const std::filesystem::path& root)
{
    // Roadmap section 4, "Nivel B -- foreign ABI": the aggregate size matrix
    // 1, 2, 3, 4, 8, 12, 16, 24, 64, 80 bytes, checked against a foreign toolchain rather than
    // against this compiler's own opinion. Each struct is exported, so the generated header
    // declares it with NS_STATIC_ASSERTs for size, alignment and every field offset, and clang --
    // targeting the MSVC ABI, with -Werror -- either agrees or fails to compile.
    //
    // Sizes 3, 12, 24 and 80 are deliberately in the list: they are the ones that are neither a
    // register width nor a round number, and they are exactly where a layout bug hides.
    if (!commandAvailable("clang"))
    {
        tests.expect(true, "clang unavailable -- foreign ABI size matrix skipped");
        return;
    }
    const std::string source = R"NS(
namespace proj {
    public struct S1  { public byte a; }
    public struct S2  { public ushort a; }
    public struct S3  { public byte a; public byte b; public byte c; }
    public struct S4  { public int a; }
    public struct S8  { public float a; public float b; }
    public struct S12 { public float a; public float b; public float c; }
    public struct S16 { public byte[16] bytes; }
    public struct S24 { public long a; public long b; public long c; }
    public struct S64 { public float[16] m; }
    public struct S80 { public float[16] m; public long a; public long b; }
    class Api {
        public static export extern C int Take1(S1 v) { return (int)v.a; }
        public static export extern C int Take2(S2 v) { return (int)v.a; }
        public static export extern C int Take3(S3 v) { return (int)v.a + (int)v.c; }
        public static export extern C int Take4(S4 v) { return v.a; }
        public static export extern C float Take8(S8 v) { return v.a + v.b; }
        public static export extern C float Take12(S12 v) { return v.a + v.c; }
        public static export extern C int Take16(S16 v) { return (int)v.bytes[15]; }
        public static export extern C long Take24(S24 v) { return v.a + v.c; }
        public static export extern C float Take64(S64 v) { return v.m[0] + v.m[15]; }
        public static export extern C long Take80(S80 v) { return v.b; }
    }
}
)NS";
    const auto projectPath = writeProject(root, "ForeignAbiMatrix", source, "StaticLibrary");
    const auto directory = projectPath.parent_path();
    const std::string projectText = projectPath.string();
    const std::string headerText = (directory / "abi.h").string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", headerText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        "ForeignAbiMatrix: header generation succeeds (" + err.str() + ")");
    if (!std::filesystem::exists(headerText)) return;

    // The sizes are asserted from the C side too, so a header that silently stopped emitting a
    // struct could not pass by omission.
    writeFile(
        directory / "abi.c",
        "#include \"abi.h\"\n"
        "_Static_assert(sizeof(NS_S1) == 1, \"S1\");\n"
        "_Static_assert(sizeof(NS_S2) == 2, \"S2\");\n"
        "_Static_assert(sizeof(NS_S3) == 3, \"S3\");\n"
        "_Static_assert(sizeof(NS_S4) == 4, \"S4\");\n"
        "_Static_assert(sizeof(NS_S8) == 8, \"S8\");\n"
        "_Static_assert(sizeof(NS_S12) == 12, \"S12\");\n"
        "_Static_assert(sizeof(NS_S16) == 16, \"S16\");\n"
        "_Static_assert(sizeof(NS_S24) == 24, \"S24\");\n"
        "_Static_assert(sizeof(NS_S64) == 64, \"S64\");\n"
        "_Static_assert(sizeof(NS_S80) == 80, \"S80\");\n"
        "int main(void) { return 0; }\n");
    const std::string command =
        "clang --target=x86_64-pc-windows-msvc -ffreestanding -std=c11 -Wall -Wextra -Werror -c \"" +
        (directory / "abi.c").string() + "\" -o \"" + (directory / "abi.obj").string() +
        "\" -I \"" + directory.string() + "\" > \"" + (directory / "clang.log").string() + "\" 2>&1";
    tests.expect(
        std::system(command.c_str()) == 0,
        "ForeignAbiMatrix: clang agrees on 1/2/3/4/8/12/16/24/64/80-byte aggregates (MSVC ABI)");
}

// --- C3: FFI aliases ------------------------------------------------------------------------------

void testAliasesCoverEveryShapeACHeaderUses(TestContext& tests, const std::filesystem::path& root)
{
    // The four kinds of typedef a C header actually contains: a builtin rename, a pointer, a
    // fixed array, and a function pointer. Plus chaining, which is how real headers are written
    // (`LPDWORD` is `PDWORD` is `DWORD*` is `unsigned long*`).
    const std::string source = R"NS(
using DWORD = uint;
using HANDLE = void*;
using PDWORD = DWORD*;
using LPDWORD = PDWORD;
using Rgba = float[4];
namespace proj {
    extern C delegate int WndProcLike(nint hwnd, uint msg);
    using Callback = WndProcLike;
    public struct Paint { public Rgba color; public DWORD flags; }
    class Program {
        public static int Impl(nint hwnd, uint msg) { return (int)msg; }
        public static int Main() {
            DWORD flags = 40u;
            HANDLE handle = null;
            LPDWORD pointer = &flags;
            Paint paint;
            paint.color[0] = 1;
            paint.flags = 2u;
            Callback callback = Impl;
            if (handle != null) { return 1; }
            if (*pointer != 40u) { return 2; }
            if (paint.flags != 2u) { return 3; }
            return (int)flags + callback(0, 2u);
        }
    }
}
)NS";
    runBoth(tests, root, "FfiAliases", source, 42);
    const std::string members = dumpAction(
        tests, "--dump-project-members", root / "FfiAliases" / "FfiAliases.nsproj", "FfiAliases");
    // An alias is a typedef, not a distinct type: by the time anything downstream looks, the
    // field's type IS what the alias named. That is why nothing else in the compiler needed to
    // learn about aliases.
    expectContains(tests, members, "field color type=builtin:float[4]", "FfiAliases");
    expectContains(tests, members, "field flags type=builtin:uint", "FfiAliases");
}

void testNamespaceAliasesStillWork(TestContext& tests, const std::filesystem::path& root)
{
    // The regression guard for the change above: `using X = A.B;` where A.B is a NAMESPACE keeps
    // meaning what it always meant. The two forms are told apart by whether the target resolves
    // as a namespace, not by syntax.
    const std::string source = R"NS(
namespace proj.deep { public struct Point { public int x; } }
namespace proj {
    using Deep = proj.deep;
    class Program {
        public static int Main() {
            Deep.Point p;
            p.x = 42;
            return p.x;
        }
    }
}
)NS";
    runBoth(tests, root, "NamespaceAlias", source, 42);
}

void testUnknownAliasTargetIsReported(TestContext& tests, const std::filesystem::path& root)
{
    reject(
        tests, root, "UnknownAliasTarget",
        R"NS(
using Missing = NotAType;
namespace proj { class Program { public static int Main() { return 0; } } }
)NS",
        "does not name a visible namespace or type");
}

// --- C4: ref / out / in ---------------------------------------------------------------------------

void testByReferenceParametersRoundTrip(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public struct Big { public long a; public long b; public long c; }
    class Program {
        public static void Bump(ref int value) { value = value + 1; }
        public static void Init(out int value) { value = 39; }
        public static int Read(in int value) { return value; }
        public static void Fill(ref Big big) { big.a = 1; big.b = 1; }
        public static int Main() {
            int counter = 1;
            Bump(ref counter);
            int fresh;
            Init(out fresh);
            Big big;
            big.a = 0; big.b = 0; big.c = 0;
            Fill(ref big);
            if (counter != 2) { return 1; }
            if (Read(in counter) != 2) { return 2; }
            if (big.a + big.b != 2) { return 3; }
            return fresh + counter + (int)big.c + 1;
        }
    }
}
)NS";
    runBoth(tests, root, "ByReferenceParameters", source, 42);
    const std::string ir = dumpAction(
        tests, "--dump-project-oop-llvm",
        root / "ByReferenceParameters" / "ByReferenceParameters.nsproj", "ByReferenceParameters");
    // Nothing new reaches the backend: a by-reference parameter is a pointer parameter, which is
    // machinery C1 already certified end to end. `ref`/`out`/`in` are rules on top of it.
    expectContains(tests, ir, "pointer.load", "ByReferenceParameters");
}

void testTheKeywordIsRequiredAndExact(TestContext& tests, const std::filesystem::path& root)
{
    // A reader of a call must be able to see that the argument can change, without going to read
    // the signature. So the keyword is required where the parameter declares one...
    reject(
        tests, root, "MissingRefKeyword",
        R"NS(
namespace proj { class Program {
    public static void Bump(ref int value) { value = value + 1; }
    public static int Main() { int a = 1; Bump(a); return a; }
} }
)NS",
        "must be passed with 'ref'");

    // ...and refused where none is.
    reject(
        tests, root, "UnexpectedRefKeyword",
        R"NS(
namespace proj { class Program {
    public static void Take(int value) { }
    public static int Main() { int a = 1; Take(ref a); return 0; }
} }
)NS",
        "is not declared 'ref'");
}

void testInIsReadOnlyAndOutIsAnObligation(TestContext& tests, const std::filesystem::path& root)
{
    // `in` exists to avoid a copy, not to open a channel back to the caller.
    reject(
        tests, root, "WriteThroughIn",
        R"NS(
namespace proj { class Program {
    public static void Bad(in int value) { value = 5; }
    public static int Main() { int a = 1; Bad(in a); return a; }
} }
)NS",
        "not assignable");

    // `out` is why a caller may pass an uninitialized variable: the callee promises to fill it.
    reject(
        tests, root, "OutNeverAssigned",
        R"NS(
namespace proj { class Program {
    public static void Bad(out int value) { int other = 1; }
    public static int Main() { int a; Bad(out a); return a; }
} }
)NS",
        "is never assigned in the body of");
}

void testRefNessParticipatesInOverloadResolution(
    TestContext& tests, const std::filesystem::path& root)
{
    // `F(int)` and `F(ref int)` are different members -- of the language and of the emitted
    // module. They lower to the same machine signature, so both the signature key and the mangled
    // name have to keep them apart, or the two would silently become one function.
    const std::string source = R"NS(
namespace proj {
    class Program {
        public static int Choose(int value) { return value; }
        public static int Choose(ref int value) { value = value * 2; return value; }
        public static int Main() {
            int a = 21;
            if (Choose(a) != 21) { return 1; }
            return Choose(ref a);
        }
    }
}
)NS";
    runBoth(tests, root, "RefOverload", source, 42);
}

// --- the contract fixtures and the packaged example ----------------------------------------------

void testInteropContractFixturesCompile(TestContext& tests)
{
    const std::filesystem::path contractRoot =
        std::filesystem::path(NSC_SOURCE_DIR) / "tests" / "interop_contract";
    const std::array<std::string_view, 3> fixtures = {
        "c2_11_fixed_field_array",
        "c2_12_union",
        "c4_ref_out_in"};
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
        const std::array<std::string_view, 2> args = {"--verify-project-oop-llvm", projectText};
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "contract fixture lowers and verifies: " + std::string(fixture) + " (" + err.str() + ")");
    }
}

void testPackagedExampleCompiles(TestContext& tests)
{
    const auto projectPath = std::filesystem::path(NSC_SOURCE_DIR) / "examples" /
        "project_stage56_c2c4" / "Stage56C2C4LanguageCompletion.nsproj";
    if (!std::filesystem::exists(projectPath))
    {
        tests.expect(false, "examples/project_stage56_c2c4 exists in the source tree");
        return;
    }
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c2c4-example";
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
            tests.expect(
                true, "packaged example [" + std::string(level) + "]: Wine unavailable -- execution skipped");
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
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c2c4";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    testFixedArrayFieldLayoutAndAccess(tests, root);
    testFixedArrayLocalAndPointerIndexing(tests, root);
    testFixedArrayCannotCrossACallByValue(tests, root);

    testUnionPackAlignAndExplicitOffsets(tests, root);
    testLayoutModifiersAreRefusedWhereTheyMeanNothing(tests, root);
    testGeneratedHostHeaderMatchesClangForEveryLayoutShape(tests, root);
    testExplicitOffsetsAreRefusedAtTheHostBoundary(tests, root);
    testForeignAbiSizeMatrixMatchesClang(tests, root);

    testAliasesCoverEveryShapeACHeaderUses(tests, root);
    testNamespaceAliasesStillWork(tests, root);
    testUnknownAliasTargetIsReported(tests, root);

    testByReferenceParametersRoundTrip(tests, root);
    testTheKeywordIsRequiredAndExact(tests, root);
    testInIsReadOnlyAndOutIsAnObligation(tests, root);
    testRefNessParticipatesInOverloadResolution(tests, root);

    testInteropContractFixturesCompile(tests);
    testPackagedExampleCompiles(tests);

    return tests.result();
}
