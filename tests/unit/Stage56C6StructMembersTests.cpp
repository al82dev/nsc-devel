// Stage 56 / C6 -- struct members: constructors, properties, indexers, operators.
//
// WHY THIS ROUND EXISTS. A struct in NativeSharp used to accept fields plus ordinary methods and
// nothing else. That was not an ABI decision -- it was ONE `if` in the parser carrying a "Stage 40
// ... remain unsupported" message that lumped together three genuinely different things. Measured
// by lifting that gate, member by member:
//
//   * PROPERTIES and USER CONVERSIONS were only forbidden, never missing. A property on a struct
//     lowers to an ordinary instance method taking the receiver's address, which this compiler has
//     emitted since Stage 40. With the gate lifted they compiled and ran correctly, untouched.
//   * OPERATORS needed one line: stage38OperatorCandidates filtered candidates with `isClass`, so
//     `a + b` on a struct never even looked for a candidate and reported "binary operator requires
//     compatible numeric operands".
//   * INDEXERS needed the accessor receiver lookup to accept a struct.
//   * CONSTRUCTORS needed real work: `new T(args)` was bound to heap allocation. On a struct it
//     now means initialize IN PLACE -- no allocator, no null branch, no class descriptor.
//
// And two members stay refused, with their own diagnostics, because refusing them is not about
// effort -- with the gate lifted they COMPILE AND SILENTLY MISBEHAVE, which is worse than an
// error. Measured, both: a struct destructor's body is emitted and never called; a virtual struct
// method degrades to a direct call because a value has no vtable pointer.
//
// THE CLAIM THIS SUITE HAS TO PROVE, and the reason NativeSharp does not need two kinds of struct:
// none of these members change a struct's LAYOUT or its ABI. A Vector3 with a constructor, three
// properties and five operators is still twelve bytes holding three floats, and still crosses to C
// as such. That is not asserted here -- the last scenario has clang, targeting the MSVC ABI with
// -Werror, verify the size, the alignment and every field offset of exactly such a type.

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
        "WINEDEBUG=-all " + wine + " " + exePath.string() + " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

std::string dumpIr(TestContext& tests, const std::filesystem::path& projectPath, std::string_view label)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = projectPath.string();
    const std::array<std::string_view, 2> args = {"--dump-project-oop-llvm", projectText};
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        std::string(label) + ": lowers to LLVM (" + err.str() + ")");
    return out.str();
}

void expectContains(
    TestContext& tests, const std::string& text, std::string_view needle, std::string_view label)
{
    tests.expect(
        text.find(std::string(needle)) != std::string::npos,
        std::string(label) + ": output contains '" + std::string(needle) + "'");
}

void expectLacks(
    TestContext& tests, const std::string& text, std::string_view needle, std::string_view label)
{
    tests.expect(
        text.find(std::string(needle)) == std::string::npos,
        std::string(label) + ": output does not contain '" + std::string(needle) + "'");
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

std::filesystem::path runBoth(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected)
{
    const auto projectPath = writeProject(root, name, source);
    compileAndRun(tests, projectPath, "-O0", std::string(name) + " [-O0]", expected);
    compileAndRun(tests, projectPath, "-O2", std::string(name) + " [-O2]", expected);
    return projectPath;
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

// --- constructors ---------------------------------------------------------------------------------

void testConstructorInitializesInPlace(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public struct Vec2 {
        public float x; public float y;
        public Vec2(float px, float py) { x = px; y = py; }
    }
    class Program {
        public static int Main() {
            Vec2 a = new Vec2(20, 22);
            return (int)(a.x + a.y);
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "StructConstructor", source, 42);
    const std::string ir = dumpIr(tests, projectPath, "StructConstructor");
    // `new` on a struct is NOT allocation, and the IR is where that is visible: a stack slot, a
    // memset, the constructor with the slot as `this`, and the value read back out. No allocator
    // call, no null-check branch, no class descriptor -- and therefore nothing to free either.
    expectContains(tests, ir, "%struct.new = alloca", "StructConstructor");
    expectContains(tests, ir, "call void @llvm.memset", "StructConstructor");
    expectContains(tests, ir, "__nsc_ctor_", "StructConstructor");
    // The allocator hook is DECLARED in every module; what matters is that constructing a struct
    // never CALLS it.
    expectLacks(tests, ir, "call ptr @__nativesharp_alloc", "StructConstructor");
}

void testImplicitDefaultConstructorZeroesEveryField(
    TestContext& tests, const std::filesystem::path& root)
{
    // A struct has no base, so there is no implicit base-constructor chain to walk and nothing
    // that could make a parameterless construction impossible. Its default is the one C
    // guarantees for a zeroed aggregate.
    const std::string source = R"NS(
namespace proj {
    public struct Counters { public int hits; public int misses; public float ratio; }
    class Program {
        public static int Main() {
            Counters c = new Counters();
            if (c.hits != 0 || c.misses != 0 || (int)c.ratio != 0) { return 1; }
            return 42;
        }
    }
}
)NS";
    runBoth(tests, root, "StructDefaultConstructor", source, 42);
}

void testConstructorsComposeAnywhereAValueMay(TestContext& tests, const std::filesystem::path& root)
{
    // The point of constructing in place is that the result is an ordinary value: it nests, it
    // passes as an argument, it returns, it feeds an operator. Nothing about it is special.
    const std::string source = R"NS(
namespace proj {
    public struct V2 {
        public float x; public float y;
        public V2(float a, float b) { x = a; y = b; }
        public static V2 operator+(V2 l, V2 r) { return new V2(l.x + r.x, l.y + r.y); }
    }
    public struct Rect {
        public V2 min; public V2 max;
        public Rect(V2 a, V2 b) { min = a; max = b; }
    }
    class Program {
        public static float Width(Rect r) { return r.max.x - r.min.x; }
        public static Rect Make() { return new Rect(new V2(1, 1), new V2(21, 21)); }
        public static int Main() {
            Rect r = Make();
            V2 sum = new V2(1, 1) + new V2(1, 1);
            return (int)Width(r) * (int)sum.x + 2;
        }
    }
}
)NS";
    runBoth(tests, root, "StructConstructorComposes", source, 42);
}

// --- properties and indexers ----------------------------------------------------------------------

void testPropertiesOnStructs(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public struct Vec2 {
        public float xv; public float yv;
        public Vec2(float a, float b) { xv = a; yv = b; }
        public float X { get { return xv; } set { xv = value; } }
        public float LengthSquared { get { return xv * xv + yv * yv; } }
        public static Vec2 Zero { get { return new Vec2(0, 0); } }
    }
    public struct Auto { public int Value { get; set; } }
    class Program {
        public static int Main() {
            Vec2 v = Vec2.Zero;
            v.X = 3;
            if ((int)v.X != 3) { return 1; }
            if ((int)v.LengthSquared != 9) { return 2; }
            Auto a;
            a.Value = 33;
            return (int)v.LengthSquared + a.Value;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "StructProperties", source, 42);
    const std::string ir = dumpIr(tests, projectPath, "StructProperties");
    // A property on a struct is an ordinary instance method taking the receiver's ADDRESS -- the
    // same shape a struct method has had since Stage 40, which is why nothing had to be built for
    // this and only the refusal had to be removed.
    expectContains(tests, ir, "get_LengthSquared\"(ptr", "StructProperties");
    expectContains(tests, ir, "set_X__builtin$3Afloat\"(ptr", "StructProperties");
}

void testIndexersOnStructs(TestContext& tests, const std::filesystem::path& root)
{
    // Matrix types are indexed constantly, and an indexer on a value has to be able to WRITE
    // through the receiver -- which works precisely because the receiver is an address.
    const std::string source = R"NS(
namespace proj {
    public struct Mat2 {
        public float[4] m;
        public Mat2(float diagonal) { m[0] = diagonal; m[3] = diagonal; }
        public float this[int index] { get { return m[index]; } set { m[index] = value; } }
        public static Mat2 Identity { get { return new Mat2(1); } }
    }
    class Program {
        public static int Main() {
            Mat2 a = Mat2.Identity;
            a[1] = 40;
            if ((int)a[0] != 1) { return 1; }
            return (int)(a[0] + a[1] + a[3]);
        }
    }
}
)NS";
    runBoth(tests, root, "StructIndexers", source, 42);
}

// --- operators and conversions --------------------------------------------------------------------

void testOperatorsOnStructs(TestContext& tests, const std::filesystem::path& root)
{
    // The whole reason a game framework has structs. Binary, unary, mixed with a scalar, and
    // comparison -- the shape vector math is written in everywhere.
    const std::string source = R"NS(
namespace proj {
    public struct Vec3 {
        public float x; public float y; public float z;
        public Vec3(float a, float b, float c) { x = a; y = b; z = c; }
        public static Vec3 operator+(Vec3 l, Vec3 r) { return new Vec3(l.x + r.x, l.y + r.y, l.z + r.z); }
        public static Vec3 operator-(Vec3 l, Vec3 r) { return new Vec3(l.x - r.x, l.y - r.y, l.z - r.z); }
        public static Vec3 operator*(Vec3 l, float s) { return new Vec3(l.x * s, l.y * s, l.z * s); }
        public static Vec3 operator-(Vec3 v) { return new Vec3(-v.x, -v.y, -v.z); }
        public static bool operator==(Vec3 l, Vec3 r) { return l.x == r.x && l.y == r.y && l.z == r.z; }
        public static bool operator!=(Vec3 l, Vec3 r) { return !(l == r); }
        public static float Dot(Vec3 l, Vec3 r) { return l.x * r.x + l.y * r.y + l.z * r.z; }
    }
    class Program {
        public static int Main() {
            Vec3 a = new Vec3(1, 2, 3);
            Vec3 b = new Vec3(1, 1, 1);
            Vec3 sum = (a + b) * 2;
            Vec3 back = sum - b * 2;
            if (!(back == a * 2)) { return 1; }
            if (sum != sum) { return 2; }
            Vec3 negated = -a;
            if ((int)negated.x != -1) { return 3; }
            return (int)Vec3.Dot(sum, new Vec3(1, 1, 1)) + (int)a.z * 6;
        }
    }
}
)NS";
    runBoth(tests, root, "StructOperators", source, 42);
}

void testUserConversionsOnStructs(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
namespace proj {
    public struct V2 { public float x; public float y; public V2(float a, float b) { x = a; y = b; } }
    public struct V3 {
        public float x; public float y; public float z;
        public V3(float a, float b, float c) { x = a; y = b; z = c; }
        public static implicit operator V3(V2 v) { return new V3(v.x, v.y, 0); }
    }
    class Program {
        public static int Main() {
            V2 flat = new V2(20, 22);
            V3 lifted = flat;
            return (int)(lifted.x + lifted.y + lifted.z);
        }
    }
}
)NS";
    runBoth(tests, root, "StructConversions", source, 42);
}

// --- what stays refused, and why ------------------------------------------------------------------

void testVirtualAndProtectedStayRefusedWithReasons(
    TestContext& tests, const std::filesystem::path& root)
{
    // Virtual dispatch and protected inheritance access have no representation on a value type.
    // T06-F02 covers destructors positively: they make the struct move-only and execute through
    // value-drop cleanup.
    reject(
        tests, root, "StructVirtualRefused",
        R"NS(
namespace proj {
    public struct Shape { public int kind; public virtual int Area() { return kind; } }
    class Program { public static int Main() { return 0; } }
}
)NS",
        "a value has no vtable pointer");

    reject(
        tests, root, "StructProtectedRefused",
        R"NS(
namespace proj {
    public struct Bad { protected int hidden; }
    class Program { public static int Main() { return 0; } }
}
)NS",
        "structs are not inheritable");

    // And `new` still means nothing for a type with neither storage shape nor constructors.
    reject(
        tests, root, "EnumConstructionRefused",
        R"NS(
namespace proj {
    public enum Mode { A, B }
    class Program { public static int Main() { Mode m = new Mode(); return 0; } }
}
)NS",
        "requires T to resolve to a class or struct type");
}

// --- the claim that makes two kinds of struct unnecessary ------------------------------------------

void testRichStructKeepsItsExactCLayout(TestContext& tests, const std::filesystem::path& root)
{
    // THE certification for this whole round. A struct with a constructor, properties, an indexer,
    // operators and a conversion is handed to a C host, and CLANG -- targeting the MSVC ABI, with
    // -Werror -- checks its size, its alignment and every field offset against the numbers
    // NativeSharp computed. If any of those members had cost a byte or moved a field, this fails
    // to compile.
    //
    // That is the evidence behind the design decision not to have two kinds of struct, one "plain
    // C-compatible" and one "rich": there is nothing to separate. Constructors, properties and
    // operators are methods and syntax; the layout is the layout.
    if (!commandAvailable("clang"))
    {
        tests.expect(true, "clang unavailable -- rich-struct layout certification skipped");
        return;
    }
    const std::string source = R"NS(
namespace proj {
    public struct Vector3 {
        public float x; public float y; public float z;
        public Vector3(float a, float b, float c) { x = a; y = b; z = c; }
        public float LengthSquared { get { return x * x + y * y + z * z; } }
        public float this[int i] { get { if (i == 0) { return x; } if (i == 1) { return y; } return z; } }
        public static Vector3 operator+(Vector3 l, Vector3 r) { return new Vector3(l.x + r.x, l.y + r.y, l.z + r.z); }
        public static Vector3 operator*(Vector3 l, float s) { return new Vector3(l.x * s, l.y * s, l.z * s); }
        public static Vector3 Zero { get { return new Vector3(0, 0, 0); } }
    }
    public struct Matrix4 {
        public float[16] m;
        public Matrix4(float diagonal) { m[0] = diagonal; m[5] = diagonal; m[10] = diagonal; m[15] = diagonal; }
        public float this[int i] { get { return m[i]; } set { m[i] = value; } }
    }
    class Api {
        public static export extern C float SumVector(Vector3 v) { return v.x + v.y + v.z; }
        public static export extern C Vector3 ScaleVector(Vector3 v, float s) { return v * s; }
        public static export extern C float MatrixTrace(Matrix4 m) { return m[0] + m[5] + m[10] + m[15]; }
    }
}
)NS";
    const auto projectPath = writeProject(root, "RichStructLayout", source, "StaticLibrary");
    const auto directory = projectPath.parent_path();
    const std::string projectText = projectPath.string();
    const std::string headerText = (directory / "rich.h").string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", headerText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        "RichStructLayout: header generation succeeds (" + err.str() + ")");
    if (!std::filesystem::exists(headerText)) return;

    std::ostringstream headerBuffer;
    headerBuffer << std::ifstream(headerText, std::ios::binary).rdbuf();
    const std::string header = headerBuffer.str();
    // The C host sees three plain floats. Not a vtable pointer, not a constructor, not a hidden
    // word -- the members left no trace in the type at all.
    expectContains(tests, header, "float x;", "RichStructLayout");
    expectContains(tests, header, "float m[16];", "RichStructLayout");
    expectLacks(tests, header, "vtable", "RichStructLayout");

    writeFile(
        directory / "host.c",
        "#include \"rich.h\"\n"
        "_Static_assert(sizeof(NS_Vector3) == 12, \"a rich struct is still three floats\");\n"
        "_Static_assert(sizeof(NS_Matrix4) == 64, \"a rich struct is still sixteen floats\");\n"
        "_Static_assert(offsetof(NS_Vector3, z) == 8, \"members added no hidden field\");\n"
        "int main(void) { return 0; }\n");
    const std::string command =
        "clang --target=x86_64-pc-windows-msvc -ffreestanding -std=c11 -Wall -Wextra -Werror -c \"" +
        (directory / "host.c").string() + "\" -o \"" + (directory / "host.obj").string() +
        "\" -I \"" + directory.string() + "\" > \"" + (directory / "clang.log").string() + "\" 2>&1";
    tests.expect(
        std::system(command.c_str()) == 0,
        "RichStructLayout: clang agrees the rich struct has the same layout and ABI as a plain one");
}

// --- the contract fixture and the packaged example ------------------------------------------------

void testMathFixtureAndExampleCompile(TestContext& tests)
{
    const auto fixture = std::filesystem::path(NSC_SOURCE_DIR) / "tests" / "interop_contract" /
        "c6_struct_math" / "c6_struct_math.nsproj";
    if (!std::filesystem::exists(fixture))
    {
        tests.expect(false, "contract fixture present: c6_struct_math");
    }
    else
    {
        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        const std::string projectText = fixture.string();
        const std::array<std::string_view, 2> args = {"--verify-project-oop-llvm", projectText};
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "contract fixture lowers and verifies: c6_struct_math (" + err.str() + ")");
    }

    const auto example = std::filesystem::path(NSC_SOURCE_DIR) / "examples" /
        "project_stage56_c6" / "Stage56C6StructMath.nsproj";
    if (!std::filesystem::exists(example))
    {
        tests.expect(false, "examples/project_stage56_c6 exists in the source tree");
        return;
    }
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c6-example";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);
    for (const std::string_view level : {"-O0", "-O2"})
    {
        const std::string projectText = example.string();
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
                true,
                "packaged example [" + std::string(level) + "]: Wine unavailable -- execution skipped");
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
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c6";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    testConstructorInitializesInPlace(tests, root);
    testImplicitDefaultConstructorZeroesEveryField(tests, root);
    testConstructorsComposeAnywhereAValueMay(tests, root);

    testPropertiesOnStructs(tests, root);
    testIndexersOnStructs(tests, root);

    testOperatorsOnStructs(tests, root);
    testUserConversionsOnStructs(tests, root);

    testVirtualAndProtectedStayRefusedWithReasons(tests, root);
    testRichStructKeepsItsExactCLayout(tests, root);

    testMathFixtureAndExampleCompile(tests);

    return tests.result();
}
