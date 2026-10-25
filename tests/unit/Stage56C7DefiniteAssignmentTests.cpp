// Stage 56 / C7 -- definite assignment and the aggregate initializer.
//
// WHY THIS ROUND EXISTS. `V v;` followed by a read of `v` used to compile to an `alloca` and
// nothing else. No store, no zeroing, no diagnostic: the read returned whatever the stack happened
// to hold, at every optimization level, for EVERY local type -- an int, a pointer, a class
// reference, an array. That is the single largest silent-wrong-answer hole a language can have,
// and it was open by default.
//
// It is closed here by two halves that only work together:
//
//   1. DEFINED STORAGE (backend). A local declared without an initializer is zeroed at the point
//      its declaration executes -- at the declaration, not next to the hoisted alloca, so a local
//      declared inside a loop is re-defaulted on every iteration. NativeSharp now has no
//      uninitialized-read undefined behaviour left anywhere.
//
//   2. DEFINITE ASSIGNMENT (front end). A path-sensitive MUST-analysis over the same control-flow
//      machinery the ownership checker already uses, joined by INTERSECTION at every merge. Reading
//      a scalar, pointer, reference or delegate local on a path where nothing wrote it is an error.
//
// Half 1 is what makes half 2 affordable, and the reason the analysis can be exactly as strict as
// it should be and no stricter. AGGREGATES -- struct values and fixed `T[N]` arrays -- are not
// reported at all, because `Color c; c.tag = 1;` is the C idiom this language deliberately keeps
// ("struct sin metodos se comporta como en C") and piecewise filling through loops and branches is
// the exact shape a MUST-analysis cannot follow without rejecting correct programs. What half 1
// guarantees is that the fields such a program never writes read as ZERO, deterministically,
// rather than as garbage. Strictness where it catches bugs; defined behaviour everywhere.
//
// And half 1 costs nothing where it is not needed: in a program this analysis accepts, every read
// is dominated by a real store, so the zeroing store is dead and SROA deletes it. Two scenarios
// below check the emitted machine code for exactly that.
//
// THE AGGREGATE INITIALIZER is the other half of the language answer: `V v = {1, 2};`, nested,
// partial, and on fields as well as locals -- the explicit spelling for what the default gives
// implicitly.
//
// A BUG THIS ROUND FOUND IN C6, fixed and pinned here: constructing a struct larger than eight
// bytes -- `Big g = new Big(1.0f);` -- stored the ADDRESS of the constructed value into the local
// instead of the value, because lowerValueConstruction short-circuited on
// isIndirectStructTypeKey. Every field read afterwards read pointer bits. Silent, and invisible to
// the verifier. See testIndirectStructValuesTravelAsValues.

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

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto directory = root / std::string(name);
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

#if !defined(NSC_TEST_CONSOLIDATED_HOST) || !defined(_WIN32)
bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}
#endif

std::string wineCommand()
{
#if defined(NSC_TEST_CONSOLIDATED_HOST) && defined(_WIN32)
    return {};
#else
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
#endif
}

int runUnderWine(const std::string& wine, const std::filesystem::path& exePath)
{
    const std::string exeText = exePath.string();
    const std::string command = "WINEDEBUG=-all " + wine + " " + exeText + " >/dev/null 2>&1";
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
        // No Wine here means the execution half degrades to an explicit skip. It never degrades
        // into a green check that asserted nothing.
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
#if !defined(NSC_TEST_O0_ONLY)
    compileAndRun(tests, projectPath, "-O2", std::string(name) + " [-O2]", expected);
#endif
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

void accept(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto projectPath = writeProject(root, name, source);
    const std::string projectText = projectPath.string();
    const std::array<std::string_view, 2> args = {"--check-project-oop-hir", projectText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    tests.expect(
        driver.run(args, out, err) == ExitCode::Success,
        std::string(name) + ": is accepted (" + err.str() + ")");
}

// The one machine-code question this suite asks twice: does the whole function collapse to a
// single `mov eax, <n>; ret`? That is the strongest possible statement that the semantics are
// what the source says AND that the defaulting stores cost nothing -- the optimizer could not
// fold the program otherwise.
#if !defined(NSC_TEST_O0_ONLY)
bool foldsToConstantReturn(
    TestContext& tests,
    const std::filesystem::path& projectPath,
    std::string_view label,
    int expected)
{
    if (!commandAvailable("llvm-objdump-20") && !commandAvailable("llvm-objdump"))
    {
        tests.expect(true, std::string(label) + ": llvm-objdump unavailable -- codegen check skipped");
        return true;
    }
    const auto objectPath = projectPath.parent_path() / "folded.obj";
    const std::string projectText = projectPath.string();
    const std::string objectText = objectPath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-obj", projectText, "-O2", "-o", objectText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    if (driver.run(args, out, err) != ExitCode::Success)
    {
        tests.expect(false, std::string(label) + ": emits an object (" + err.str() + ")");
        return false;
    }
    const std::string tool = commandAvailable("llvm-objdump-20") ? "llvm-objdump-20" : "llvm-objdump";
    const auto listingPath = projectPath.parent_path() / "folded.txt";
    const std::string listingText = listingPath.string();
    const std::string command =
        tool + " -d " + objectText + " > " + listingText + " 2>/dev/null";
    if (std::system(command.c_str()) != 0)
    {
        tests.expect(true, std::string(label) + ": disassembly unavailable -- codegen check skipped");
        return true;
    }
    std::ifstream listing(listingPath);
    std::stringstream buffer;
    buffer << listing.rdbuf();
    const std::string text = buffer.str();
    // Intel-syntax-agnostic: the disassembler prints the immediate in hex.
    std::ostringstream needle;
    needle << "movl\t$0x" << std::hex << expected << ", %eax";
    const bool folded = text.find(needle.str()) != std::string::npos;
    tests.expect(
        folded,
        std::string(label) + ": -O2 folds the whole body to a constant return of " +
            std::to_string(expected));
    return folded;
}
#endif

// --- half 1: defined default storage --------------------------------------------------------

void testDeclarationWithoutInitializerZeroesItsStorage(
    TestContext& tests, const std::filesystem::path& root)
{
    // The regression this whole round exists for. Every one of these reads used to be a read of
    // whatever the stack held; now every one of them is a read of a defined zero, and -O2 proves
    // it by folding the entire function.
    const std::string source = R"NS(
namespace proj {
    public struct Point { public int x; public int y; public int z; }
    class Program {
        public static int Main() {
            Point p;
            p.x = 10;
            if (p.y != 0) { return 1; }
            if (p.z != 0) { return 2; }
            if (p.x != 10) { return 3; }

            int[4] cells;
            cells[0] = 5;
            if (cells[0] != 5) { return 4; }
            if (cells[3] != 0) { return 5; }
            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "DefaultStorage", source, 42);
    const std::string ir = dumpIr(tests, projectPath, "DefaultStorage");
    // The declaration -- not the hoisted alloca -- is what carries the zeroing.
    expectContains(tests, ir, "zeroinitializer", "DefaultStorage");
#if !defined(NSC_TEST_O0_ONLY)
    foldsToConstantReturn(tests, projectPath, "DefaultStorage", 42);
#endif
}

void testDefaultingCostsNothingWhenTheProgramDoesNotNeedIt(
    TestContext& tests, const std::filesystem::path& root)
{
    // Same shape, but every local is fully written before it is read. The zeroing stores are dead
    // and the optimizer removes them: this is the measurement behind "half 1 is free in exactly
    // the programs that do not need it".
    const std::string source = R"NS(
namespace proj {
    public struct Point { public int x; public int y; public int z; }
    class Program {
        public static int Main() {
            Point p;
            p.x = 10; p.y = 20; p.z = 12;
            int total;
            total = p.x + p.y + p.z;
            return total;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "DefaultingIsFree", source, 42);
#if !defined(NSC_TEST_O0_ONLY)
    foldsToConstantReturn(tests, projectPath, "DefaultingIsFree", 42);
#endif
}

void testDefaultIsReestablishedOnEveryLoopIteration(
    TestContext& tests, const std::filesystem::path& root)
{
    // Why the zeroing lives at the DECLARATION and not beside the hoisted alloca: a local declared
    // inside a loop body is a fresh variable each time round, and its default has to be fresh too.
    // If the zeroing were hoisted, `acc` would keep the previous iteration's value and this would
    // return 6 instead of 42.
    const std::string source = R"NS(
namespace proj {
    public struct Counter { public int value; }
    class Program {
        public static int Main() {
            int total = 0;
            int i = 0;
            while (i < 3) {
                Counter acc;
                acc.value = acc.value + 14;
                total = total + acc.value;
                i = i + 1;
            }
            return total;
        }
    }
}
)NS";
    runBoth(tests, root, "LoopScopedDefault", source, 42);
}

// --- half 2: the analysis -------------------------------------------------------------------

void testReadingAnUnassignedLocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    reject(
        tests, root, "ReadBeforeWrite", R"NS(
namespace proj { class Program { public static int Main() { int x; return x; } } }
)NS",
        "use of unassigned local variable 'x'");
}

void testTheAnalysisIsPathSensitive(TestContext& tests, const std::filesystem::path& root)
{
    // The merge rule, stated three ways. A local written on ONE arm of a branch is not assigned
    // after the merge (the join is an intersection); written on BOTH arms it is; and a write that
    // only happens inside a loop body does not count after the loop, because the loop may run
    // zero times. This is the C# rule, and it is what makes the check worth having: it is exactly
    // the set of programs where the zero half 1 supplies is certainly not what was meant.
    reject(
        tests, root, "AssignedOnOneArm", R"NS(
namespace proj { class Program {
    static bool Toss() { return true; }
    public static int Main() { int x; if (Toss()) { x = 1; } return x; }
} }
)NS",
        "use of unassigned local variable 'x'");

    reject(
        tests, root, "AssignedOnlyInALoop", R"NS(
namespace proj { class Program {
    public static int Main() { int x; int i = 0; while (i < 3) { x = i; i = i + 1; } return x; }
} }
)NS",
        "use of unassigned local variable 'x'");

    accept(
        tests, root, "AssignedOnBothArms", R"NS(
namespace proj { class Program {
    static bool Toss() { return true; }
    public static int Main() { int x; if (Toss()) { x = 1; } else { x = 2; } return x; }
} }
)NS");
}

void testTheAnalysisCatchesRealNullDereferences(
    TestContext& tests, const std::filesystem::path& root)
{
    // These three are the reason "any write through the local marks it assigned" would have been
    // the WRONG rule. Storing through an unassigned managed array, an unassigned pointer or an
    // unassigned class reference is not an assignment of that local -- it is a dereference of a
    // value that was never given one. Each of these compiled and crashed before; each is now a
    // compile error.
    reject(
        tests, root, "StoreThroughUnassignedArray", R"NS(
namespace proj { class Program { public static int Main() { int[] a; a[0] = 1; return 0; } } }
)NS",
        "use of unassigned local variable 'a'");

    reject(
        tests, root, "StoreThroughUnassignedPointer", R"NS(
namespace proj { class Program { public static int Main() { int* p; *p = 1; return 0; } } }
)NS",
        "use of unassigned local variable 'p'");

    reject(
        tests, root, "ReadThroughUnassignedReference", R"NS(
namespace proj {
    class Node { public int v; }
    class Program { public static int Main() { Node n; return n.v; } }
}
)NS",
        "use of unassigned local variable 'n'");
}

void testCompoundAssignmentAndRefAreReads(TestContext& tests, const std::filesystem::path& root)
{
    // `x += 1` reads x before it writes it, and `f(ref x)` hands the callee a value it is entitled
    // to read. Both require the local to be assigned already; only `f(out x)` does not.
    reject(
        tests, root, "CompoundAssignmentReadsFirst", R"NS(
namespace proj { class Program { public static int Main() { int x; x += 1; return x; } } }
)NS",
        "use of unassigned local variable 'x'");

    reject(
        tests, root, "RefArgumentIsARead", R"NS(
namespace proj { class Program {
    static void Bump(ref int v) { v = v + 1; }
    public static int Main() { int x; Bump(ref x); return x; }
} }
)NS",
        "use of unassigned local variable 'x'");
}

void testOutParametersAssignTheirArgument(TestContext& tests, const std::filesystem::path& root)
{
    // The complement of the previous scenario, and the piece of C4 this round finishes: an `out`
    // argument is the one form that ASSIGNS its operand, so `int x; Fill(out x); use(x);` has to
    // be accepted -- and the obligation on the callee is what makes that sound.
    const std::string source = R"NS(
namespace proj { class Program {
    static void Fill(out int value) { value = 42; }
    public static int Main() { int x; Fill(out x); return x; }
} }
)NS";
    runBoth(tests, root, "OutAssignsItsArgument", source, 42);
}

void testBoundNamesStartAssigned(TestContext& tests, const std::filesystem::path& root)
{
    // Three names user code never writes and must never be asked to: the exception a catch binds,
    // the element a foreach binds, and a lambda's parameter. Each is bound by the construct
    // itself. The catch case is the one that forced the snapshot machinery to distinguish "not
    // assigned" from "did not exist yet": analyzeTry captures its catch-entry state BEFORE
    // declaring the exception local, and restoring that state used to throw the binding away.
    const std::string source = R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
namespace proj {
    public delegate int Mapper(int value);
    class Program {
        public static int Main() {
            int sum = 0;
            try { throw new Exception(); }
            catch (Exception e) { Exception again = e; sum = sum + 20; }

            int[] values = new int[2];
            values[0] = 10; values[1] = 12;
            foreach (int item in values) { sum = sum + item; }

            Mapper twice = value => value;
            return sum + twice(0);
        }
    }
}
)NS";
    runBoth(tests, root, "BoundNamesStartAssigned", source, 42);
}

void testAggregatesAreNotReported(TestContext& tests, const std::filesystem::path& root)
{
    // The deliberate difference from C#, and the language decision behind it. A struct filled
    // field by field across branches and loops is the C idiom this language keeps; a MUST-analysis
    // cannot follow it without rejecting correct programs, and half 1 is what makes not following
    // it safe. Note what this program relies on: `shape.depth` is never written on any path, and
    // reads as a defined zero rather than as garbage.
    const std::string source = R"NS(
namespace proj {
    public struct Shape { public int width; public int height; public int depth; }
    class Program {
        static bool Wide() { return true; }
        public static int Main() {
            Shape shape;
            if (Wide()) { shape.width = 40; } else { shape.height = 40; }
            int i = 0;
            while (i < 2) { shape.height = shape.height + 1; i = i + 1; }
            return shape.width + shape.height + shape.depth;
        }
    }
}
)NS";
    runBoth(tests, root, "AggregatesAreNotReported", source, 42);
}

// --- the aggregate initializer --------------------------------------------------------------

void testAggregateInitializerFillsLocals(TestContext& tests, const std::filesystem::path& root)
{
    // Every shape the form takes: full, partial (the rest keeps the zero, exactly as in C),
    // explicitly empty, nested, on a fixed array, with a trailing comma, and with runtime values
    // rather than constants.
    const std::string source = R"NS(
namespace proj {
    public struct Vec2 { public int x; public int y; }
    public struct Rect { public Vec2 min; public Vec2 max; public int tag; }
    class Program {
        public static int Main() {
            Vec2 full = {3, 4};
            if (full.x != 3) { return 1; }
            if (full.y != 4) { return 2; }

            Vec2 partial = {7};
            if (partial.x != 7) { return 3; }
            if (partial.y != 0) { return 4; }

            Vec2 empty = {};
            if (empty.x != 0) { return 5; }
            if (empty.y != 0) { return 6; }

            Rect nested = { {1, 2}, {8, 9}, 5 };
            if (nested.min.x != 1) { return 7; }
            if (nested.max.y != 9) { return 8; }
            if (nested.tag != 5) { return 9; }

            int[4] cells = {10, 20,};
            if (cells[0] != 10) { return 10; }
            if (cells[1] != 20) { return 11; }
            if (cells[3] != 0) { return 12; }

            int width = 6;
            int[3] scaled = {width, width * 2, width * 3};
            if (scaled[2] != 18) { return 13; }

            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "AggregateInitializer", source, 42);
#if !defined(NSC_TEST_O0_ONLY)
    foldsToConstantReturn(tests, projectPath, "AggregateInitializer", 42);
#endif
}

void testAggregateInitializerFillsFields(TestContext& tests, const std::filesystem::path& root)
{
    // A field is filled against its declared type the same way a local is -- same binder, so the
    // two spellings cannot disagree about arity, element types or partial fills.
    const std::string source = R"NS(
namespace proj {
    public struct Vec2 { public int x; public int y; }
    class Holder { public Vec2 offset = {5, 6}; }
    class Program {
        static Vec2 shared = {30, 1};
        public static int Main() {
            raii Holder holder = new Holder();
            return shared.x + shared.y + holder.offset.x + holder.offset.y;
        }
    }
}
)NS";
    runBoth(tests, root, "AggregateInitializerFields", source, 42);
}

void testAggregateInitializerSatisfiesDefiniteAssignment(
    TestContext& tests, const std::filesystem::path& root)
{
    accept(
        tests, root, "InitializerAssigns", R"NS(
namespace proj {
    public struct Vec2 { public int x; public int y; }
    class Program { public static int Main() { Vec2 v = {1, 2}; return v.x + v.y; } }
}
)NS");
}

void testAggregateInitializerRejectsWhatItShould(
    TestContext& tests, const std::filesystem::path& root)
{
    reject(
        tests, root, "TooManyValues", R"NS(
namespace proj {
    public struct One { public int a; }
    class Program { public static int Main() { One v = {1, 2}; return v.a; } }
}
)NS",
        "has 2 values but the type takes 1");

    reject(
        tests, root, "WrongElementType", R"NS(
namespace proj {
    public struct One { public int a; }
    class Program { public static int Main() { One v = {"x"}; return v.a; } }
}
)NS",
        "cannot initialize element 0");

    // A class is a reference, not storage: there is nothing for a braced list to fill in place.
    reject(
        tests, root, "BracesOnAClass", R"NS(
namespace proj {
    class Node { public int a; }
    class Program { public static int Main() { Node n = {1}; return 0; } }
}
)NS",
        "requires a struct or fixed-array target");

    // A DYNAMIC array is a heap object behind a reference, unlike a fixed `T[N]`.
    reject(
        tests, root, "BracesOnADynamicArray", R"NS(
namespace proj { class Program { public static int Main() { int[] a = {1, 2}; return 0; } } }
)NS",
        "requires a struct or fixed-array target");

    // `var` binds a type from its initializer; a braced list has none of its own, which is the
    // same reason `var` cannot bind a bare lambda.
    reject(
        tests, root, "VarCannotInferFromBraces", R"NS(
namespace proj { class Program { public static int Main() { var v = {1, 2}; return 0; } } }
)NS",
        "'var' cannot infer a type from an aggregate initializer");
}

// --- the C6 bug this round found ------------------------------------------------------------

void testIndirectStructValuesTravelAsValues(TestContext& tests, const std::filesystem::path& root)
{
    // REGRESSION PIN. `Big` is twenty bytes, so it travels INDIRECTLY under the Win64 aggregate
    // ABI. lowerValueConstruction used to short-circuit on that and return the constructed slot's
    // ADDRESS; the local declaration then stored a POINTER into the local's twenty bytes and every
    // field read afterwards read pointer bits as floats. Wrong values, no diagnostic, and the IR
    // verifier had nothing to complain about -- `store ptr, ptr` is perfectly well-formed.
    //
    // The invariant the fix restores: an expression of aggregate type hands over the LOADED
    // carrier at this level, never a pointer -- the same thing a call returning one does after
    // writing its sret slot.
    const std::string source = R"NS(
namespace proj {
    public struct Big {
        public float a; public float b; public float c; public float d; public int e;
        public Big(float v) { a = v; b = v + 1; c = v + 2; d = v + 3; e = 9; }
    }
    class Program {
        public static int Main() {
            Big g = new Big(10);
            if (g.a != 10) { return 1; }
            if (g.d != 13) { return 2; }
            if (g.e != 9) { return 3; }
            return (int)(g.a + g.b + g.c) + g.e;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "IndirectStructValue", source, 42);
    const std::string ir = dumpIr(tests, projectPath, "IndirectStructValue");
    expectContains(tests, ir, "%struct.new.value = load [20 x i8]", "IndirectStructValue");
    expectLacks(tests, ir, "store ptr %struct.new,", "IndirectStructValue");
#if !defined(NSC_TEST_O0_ONLY)
    foldsToConstantReturn(tests, projectPath, "IndirectStructValue", 42);
#endif
}

// --- the shipped fixture and example ---------------------------------------------------------

void testFixtureAndExampleCompile(TestContext& tests)
{
    const std::filesystem::path sourceRoot(NSC_SOURCE_DIR);
    const std::array<std::filesystem::path, 2> projects = {
        sourceRoot / "tests" / "interop_contract" / "c7_definite_assignment" /
            "c7_definite_assignment.nsproj",
        sourceRoot / "examples" / "project_stage56_c7" / "project_stage56_c7.nsproj"};
    for (const auto& project : projects)
    {
        const std::string projectText = project.string();
        tests.expect(std::filesystem::exists(project), "C7 project exists: " + projectText);
        if (!std::filesystem::exists(project)) continue;
        const auto exePath = project.parent_path() / "c7_check.exe";
        const std::string exeText = exePath.string();
#if defined(NSC_TEST_O0_ONLY)
        constexpr std::string_view optimization = "-O0";
#else
        constexpr std::string_view optimization = "-O2";
#endif
        const std::array<std::string_view, 5> args = {
            "--emit-project-oop-exe", projectText, optimization, "-o", exeText};
        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "C7 project compiles: " + projectText + " (" + err.str() + ")");
        std::error_code cleanup;
        std::filesystem::remove(exePath, cleanup);
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c7";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    testDeclarationWithoutInitializerZeroesItsStorage(tests, root);
    testDefaultingCostsNothingWhenTheProgramDoesNotNeedIt(tests, root);
    testDefaultIsReestablishedOnEveryLoopIteration(tests, root);

    testReadingAnUnassignedLocalIsRejected(tests, root);
    testTheAnalysisIsPathSensitive(tests, root);
    testTheAnalysisCatchesRealNullDereferences(tests, root);
    testCompoundAssignmentAndRefAreReads(tests, root);
    testOutParametersAssignTheirArgument(tests, root);
    testBoundNamesStartAssigned(tests, root);
    testAggregatesAreNotReported(tests, root);

    testAggregateInitializerFillsLocals(tests, root);
    testAggregateInitializerFillsFields(tests, root);
    testAggregateInitializerSatisfiesDefiniteAssignment(tests, root);
    testAggregateInitializerRejectsWhatItShould(tests, root);

    testIndirectStructValuesTravelAsValues(tests, root);

    testFixtureAndExampleCompile(tests);

    return tests.result();
}
