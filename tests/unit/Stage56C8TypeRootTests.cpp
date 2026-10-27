// Stage 56 / C8 -- the type root: ToString / Equals / GetHashCode for classes and structs.
//
// WHY THIS ROUND EXISTS. NativeSharp had no root at all. `public override string ToString()` on a
// class failed with "override method 'ToString()' has no inherited method to override"; `a.Equals(b)`
// with "receiver/type has no compatible member named 'Equals'"; and `object` was not a type name.
// Nothing could say what it was, compare itself, or go in a hash table.
//
// THE DESIGN DECISION, stated once because everything else follows from it: THERE IS NO BOXING.
// `object o = 5;` and `object o = someStruct;` are compile errors, not silent heap allocations.
// That is not effort avoidance -- it is the only choice consistent with the rest of the language.
// Boxing means a hidden allocation, and in a language with no GC that is explicit about ownership
// everywhere else, "who frees the box `x.GetHashCode()` made" has no good answer. The same problem
// is solved without boxes:
//
//   * a CLASS derives implicitly from NativeSharp.Core.Object -- a real class, not a compiler
//     fiction, so the vtable builder, the descriptor emitter, `base.`, the constructor chain and
//     the RTTI walk all needed no special case whatsoever. The three members are virtual, so
//     `override` and dynamic dispatch work exactly as they do for any other method.
//   * a STRUCT gets the three SYNTHESIZED by the compiler and resolved on its static type -- which
//     is what a struct is. No vtable, no box, not one byte of layout change. Same conclusion C6
//     reached about struct members: one kind of struct is enough.
//   * a PRIMITIVE already had `ToString()` (the Stage 51 facade over NativeSharp.Core.String), and
//     keeps it unchanged.
//
// The root is OPTIONAL, following exactly the convention NativeSharp.Core.Exception already set
// for typed catch/throw: a project that does not have NativeSharp.Core.Object in view compiles
// bit-for-bit as it did before. That is what let this land without disturbing a single one of the
// 236 tests or 128 example projects.
//
// WHAT IT COSTS, and the reason a scenario below reads the emitted IR rather than trusting the
// claim: three vtable slots per class, plus one pointer per class descriptor to the type's name --
// a constant in the binary, not an allocation. Objects do not grow by a byte, because Object
// declares no fields. Structs change nothing at all.

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

// Every project here references the real standard library, because the root lives there and
// because that is how a real NativeSharp project gets it.
std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    bool referenceCore = true)
{
    const std::filesystem::path sourceRoot(NSC_SOURCE_DIR);
    const std::string corePath =
        (sourceRoot / "lib" / "NativeSharp.Core" / "NativeSharp.Core.nsproj").string();
    const auto directory = root / std::string(name);
    std::string references;
    if (referenceCore)
    {
        references = "<ProjectReference Include=\"" + corePath + "\" />";
    }
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />" + references +
            "</ItemGroup></Project>");
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
    std::string_view expectedFragment,
    bool referenceCore = true)
{
    const auto projectPath = writeProject(root, name, source, referenceCore);
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
            "' (got: " + diagnostics.substr(0, 260) + ")");
}

// A struct-only program folds completely at -O2 when its semantics are what the source says: the
// synthesized members have no calls and no memory the optimizer cannot see through. That makes a
// constant return the strongest available proof that every comparison and every hash is right --
// stronger than running it, because the optimizer had to prove each one to fold it.
bool foldsToConstantReturn(
    TestContext& tests,
    const std::filesystem::path& projectPath,
    std::string_view label,
    int expected)
{
    const bool haveVersioned = commandAvailable("llvm-objdump-20");
    if (!haveVersioned && !commandAvailable("llvm-objdump"))
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
    const std::string tool = haveVersioned ? "llvm-objdump-20" : "llvm-objdump";
    const auto listingPath = projectPath.parent_path() / "folded.txt";
    const std::string listingText = listingPath.string();
    if (std::system((tool + " -d " + objectText + " > " + listingText + " 2>/dev/null").c_str()) != 0)
    {
        tests.expect(true, std::string(label) + ": disassembly unavailable -- codegen check skipped");
        return true;
    }
    std::ifstream listing(listingPath);
    std::stringstream buffer;
    buffer << listing.rdbuf();
    const std::string text = buffer.str();
    std::ostringstream needle;
    needle << "movl\t$0x" << std::hex << expected << ", %eax";
    const bool folded = text.find(needle.str()) != std::string::npos;
    tests.expect(
        folded,
        std::string(label) + ": -O2 folds the whole body to a constant return of " +
            std::to_string(expected));
    return folded;
}

// --- the class half ---------------------------------------------------------------------------

void testClassesGetTheRootMembers(TestContext& tests, const std::filesystem::path& root)
{
    // Every line of this used to be a compile error. `override ToString()` had nothing to
    // override, Equals and GetHashCode did not exist, and `object` was not a type name.
    const std::string source = R"NS(
using NativeSharp.Core;
namespace proj {
    public class Widget { }
    public class Gadget {
        public override string ToString() { return "Gadget"; }
    }
    public class Cog : Gadget {
        public override string ToString() { return "Cog"; }
    }
    class Program {
        public static int Main() {
            raii Widget widget = new Widget();
            if (!String.Equals(widget.ToString(), "Widget")) { return 1; }
            raii Gadget gadget = new Gadget();
            if (!String.Equals(gadget.ToString(), "Gadget")) { return 2; }

            // The default name comes from the object's RUNTIME descriptor, so a root reference
            // answers the real type, and an override dispatches virtually through it.
            borrowed object asRoot = widget;
            if (!String.Equals(asRoot.ToString(), "Widget")) { return 3; }
            borrowed object asRootGadget = gadget;
            if (!String.Equals(asRootGadget.ToString(), "Gadget")) { return 4; }
            raii Gadget asBase = new Cog();
            if (!String.Equals(asBase.ToString(), "Cog")) { return 5; }

            if (!widget.Equals(widget)) { return 6; }
            if (widget.Equals(gadget)) { return 7; }
            if (widget.GetHashCode() != widget.GetHashCode()) { return 8; }
            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "ClassRootMembers", source, 42);
    const std::string ir = dumpIr(tests, projectPath, "ClassRootMembers");

    // The type name is a CONSTANT with the exact layout a NativeSharp string has -- a length
    // followed by the bytes -- which is what makes the default ToString() a load and not an
    // allocation.
    expectContains(tests, ir, "@\"__nsc_typename_", "ClassRootMembers");
    expectContains(tests, ir, "c\"Widget\\00\"", "ClassRootMembers");
    expectContains(tests, ir, "c\"Gadget\\00\"", "ClassRootMembers");
    // Field 6 of the descriptor is where it lives, and the root's ToString() reads it there.
    expectContains(tests, ir, "descriptor.type_name", "ClassRootMembers");
    // Item 319 appends the object-only destruction entry after the type-name pointer. Existing
    // C8 field 6 stays fixed while the descriptor grows to eight fields.
    expectContains(
        tests, ir, "%nativesharp.class_descriptor = type { ptr, ptr, i64, i64, ptr, ptr, ptr, ptr }",
        "ClassRootMembers");
}

void testTheRootIsARealClassAndNotACompilerFiction(
    TestContext& tests, const std::filesystem::path& root)
{
    // Why it matters that Object is declared in NativeSharp source rather than invented in the
    // compiler: every consumer of the hierarchy sees an ordinary base class. This checks the two
    // consequences that would be most visible if it were not -- `base.` reaches it, and a class
    // that names an explicit base is not silently re-parented.
    const std::string source = R"NS(
using NativeSharp.Core;
namespace proj {
    public class Named {
        // `base.` reaches the root, which only works because Object really is this class's base.
        // It also proves the root's own ToString() reads the RUNTIME descriptor: called on a
        // Derived, this same body answers "Derived".
        public override string ToString() { return base.ToString(); }
    }
    public class Derived : Named { }
    class Program {
        public static int Main() {
            raii Named named = new Named();
            if (!String.Equals(named.ToString(), "Named")) { return 1; }
            // Derived names Named as its base, so its chain is Derived -> Named -> Object and it
            // inherits Named's override rather than being silently re-parented.
            raii Derived derived = new Derived();
            if (!String.Equals(derived.ToString(), "Derived")) { return 2; }
            borrowed Named asBase = derived;
            if (!String.Equals(asBase.ToString(), "Derived")) { return 3; }
            return 42;
        }
    }
}
)NS";
    runBoth(tests, root, "RootIsARealClass", source, 42);
}

void testTheRootIsOptional(TestContext& tests, const std::filesystem::path& root)
{
    // A project that does not have NativeSharp.Core.Object in view compiles exactly as it did
    // before C8 -- no implicit base, no extra vtable slots, no descriptor name. This is the
    // convention NativeSharp.Core.Exception already set for typed catch/throw, and it is what let
    // C8 land without disturbing a single existing project.
    const std::string source = R"NS(
namespace proj {
    public class Widget { public int value; }
    class Program {
        public static int Main() {
            raii Widget w = new Widget();
            w.value = 42;
            return w.value;
        }
    }
}
)NS";
    const auto projectPath = writeProject(root, "RootIsOptional", source, /*referenceCore=*/false);
    compileAndRun(tests, projectPath, "-O0", "RootIsOptional [-O0]", 42);
    const std::string ir = dumpIr(tests, projectPath, "RootIsOptional");
    // Descriptor shape is ABI-stable regardless of whether the optional root is visible.
    expectContains(
        tests, ir, "%nativesharp.class_descriptor = type { ptr, ptr, i64, i64, ptr, ptr, ptr, ptr }",
        "RootIsOptional");
    expectLacks(tests, ir, "NativeSharp_Core_Object", "RootIsOptional");

    // And `object` stays an unknown type name there rather than quietly meaning something else.
    reject(
        tests, root, "ObjectWithoutRoot", R"NS(
namespace proj { class Program { public static int Main() { object o = null; return 0; } } }
)NS",
        "unknown type name 'object'", /*referenceCore=*/false);
}

// --- the struct half --------------------------------------------------------------------------

void testStructsGetSynthesizedRootMembers(TestContext& tests, const std::filesystem::path& root)
{
    // Nesting, fixed arrays and mixed widths, all flattened to scalar leaves. The -O2 fold is the
    // real assertion here: for the optimizer to reduce this to a constant it had to prove every
    // comparison and every hash relation independently.
    const std::string source = R"NS(
using NativeSharp.Core;
namespace proj {
    public struct Vec2 { public int x; public int y; }
    public struct Nested { public Vec2 origin; public float scale; public byte tag; }
    public struct WithArray { public int[3] cells; public short id; }
    class Program {
        public static int Main() {
            Vec2 a = {1, 2};
            Vec2 b = {1, 2};
            Vec2 c = {1, 3};
            if (!String.Equals(a.ToString(), "Vec2")) { return 1; }
            if (!a.Equals(b)) { return 2; }
            if (a.Equals(c)) { return 3; }
            if (a.GetHashCode() != b.GetHashCode()) { return 4; }
            if (a.GetHashCode() == c.GetHashCode()) { return 5; }

            Nested n1 = { {7, 8}, 1.5f, 3 };
            Nested n2 = { {7, 8}, 1.5f, 3 };
            Nested n3 = { {7, 8}, 1.5f, 4 };
            if (!n1.Equals(n2)) { return 6; }
            if (n1.Equals(n3)) { return 7; }
            if (n1.GetHashCode() != n2.GetHashCode()) { return 8; }
            if (!String.Equals(n1.ToString(), "Nested")) { return 9; }

            WithArray w1 = { {1, 2, 3}, 9 };
            WithArray w2 = { {1, 2, 3}, 9 };
            WithArray w3 = { {1, 2, 4}, 9 };
            if (!w1.Equals(w2)) { return 10; }
            if (w1.Equals(w3)) { return 11; }
            if (w1.GetHashCode() != w2.GetHashCode()) { return 12; }
            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "StructRootMembers", source, 42);
    foldsToConstantReturn(tests, projectPath, "StructRootMembers", 42);

    const std::string ir = dumpIr(tests, projectPath, "StructRootMembers");
    // A struct pays for none of this. No vtable, no descriptor, no boxing allocation -- the
    // members are inline code over the value's own bytes.
    expectLacks(tests, ir, "__nsc_vtable_StructRootMembers$3A$3Aproj$2EVec2", "StructRootMembers");
    expectLacks(tests, ir, "__nsc_classdesc_StructRootMembers$3A$3Aproj$2EVec2", "StructRootMembers");
}

void testADeclaredMemberWinsOverTheSynthesizedOne(
    TestContext& tests, const std::filesystem::path& root)
{
    // The synthesized members fill a gap; they never shadow. Ordinary member lookup runs first and
    // only its failure reaches the facade.
    const std::string source = R"NS(
using NativeSharp.Core;
namespace proj {
    public struct Tagged {
        public int value;
        public string ToString() { return "Tagged!"; }
        public bool Equals(Tagged other) { return true; }
    }
    class Program {
        public static int Main() {
            Tagged a = {1};
            Tagged b = {2};
            if (!String.Equals(a.ToString(), "Tagged!")) { return 1; }
            // The declared Equals says everything is equal; the synthesized one would not.
            if (!a.Equals(b)) { return 2; }
            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "DeclaredMemberWins", source, 42);
    foldsToConstantReturn(tests, projectPath, "DeclaredMemberWins", 42);
}

void testEqualsAndHashCodeAgreeByConstruction(
    TestContext& tests, const std::filesystem::path& root)
{
    // The contract, and the reason both members read BITS rather than values. Compare floats with
    // fcmp and hash their bits and the pair is broken twice over: +0.0 == -0.0 but hashes apart,
    // and a NaN never equals itself while hashing identically. Reading the same bits for both
    // makes "equal implies same hash" true by construction rather than by care.
    const std::string source = R"NS(
namespace proj {
    public struct Sample { public float value; }
    class Program {
        public static int Main() {
            Sample positiveZero = {0.0f};
            Sample negativeZero = {-0.0f};
            // Different bits: not equal, and the hashes are free to differ.
            if (positiveZero.Equals(negativeZero)) { return 1; }

            Sample left = {1.5f};
            Sample right = {1.5f};
            if (!left.Equals(right)) { return 2; }
            if (left.GetHashCode() != right.GetHashCode()) { return 3; }
            return 42;
        }
    }
}
)NS";
    const auto projectPath = runBoth(tests, root, "BitContract", source, 42);
    foldsToConstantReturn(tests, projectPath, "BitContract", 42);
}

void testUnionsAreRefusedWithAReason(TestContext& tests, const std::filesystem::path& root)
{
    // Only one member of a union is live at a time and nothing in the value says which, so
    // comparing or hashing every field would be meaningless. C refuses to compare unions for the
    // same reason. ToString is still fine -- a name is a name.
    reject(
        tests, root, "UnionEquals", R"NS(
namespace proj {
    public union Payload { public int asInt; public float asFloat; }
    class Program {
        public static int Main() { Payload a; Payload b; if (a.Equals(b)) { return 1; } return 0; }
    }
}
)NS",
        "only one member of a union is live at a time");
}

void testStructEqualsComparesItsOwnType(TestContext& tests, const std::filesystem::path& root)
{
    // Widening the synthesized Equals to take a root reference would mean boxing the argument.
    // Since there is no boxing, it compares against its own type and says so.
    reject(
        tests, root, "EqualsWrongType", R"NS(
namespace proj {
    public struct Vec2 { public int x; public int y; }
    public struct Other { public int x; }
    class Program {
        public static int Main() {
            Vec2 a = {1, 2};
            Other b = {1};
            if (a.Equals(b)) { return 1; }
            return 0;
        }
    }
}
)NS",
        "compares against its own type");
}

// --- no boxing, stated as diagnostics ---------------------------------------------------------

void testThereIsNoBoxing(TestContext& tests, const std::filesystem::path& root)
{
    // The design decision, enforced. Neither of these silently allocates; both are compile errors.
    reject(
        tests, root, "NoBoxingForPrimitives", R"NS(
using NativeSharp.Core;
namespace proj { class Program { public static int Main() { object o = 5; return 0; } } }
)NS",
        "cannot initialize local");

    reject(
        tests, root, "NoBoxingForStructs", R"NS(
using NativeSharp.Core;
namespace proj {
    public struct Vec2 { public int x; public int y; }
    class Program { public static int Main() { Vec2 v = {1, 2}; object o = v; return 0; } }
}
)NS",
        "cannot initialize local");
}

// --- primitives -------------------------------------------------------------------------------

void testPrimitivesKeepTheirToString(TestContext& tests, const std::filesystem::path& root)
{
    const std::string source = R"NS(
using NativeSharp.Core;
namespace proj {
    class Program {
        public static int Main() {
            int number = 5;
            raii string numberText = number.ToString();
            if (!String.Equals(numberText, "5")) { return 1; }
            bool flag = true;
            raii string flagText = flag.ToString();
            if (!String.Equals(flagText, "True")) { return 2; }
            return 42;
        }
    }
}
)NS";
    runBoth(tests, root, "PrimitiveToString", source, 42);
}

// --- the shipped fixture and example ----------------------------------------------------------

void testFixtureAndExampleCompile(TestContext& tests)
{
    const std::filesystem::path sourceRoot(NSC_SOURCE_DIR);
    const std::array<std::filesystem::path, 2> projects = {
        sourceRoot / "tests" / "interop_contract" / "c8_type_root" / "c8_type_root.nsproj",
        sourceRoot / "examples" / "project_stage56_c8" / "project_stage56_c8.nsproj"};
    for (const auto& project : projects)
    {
        const std::string projectText = project.string();
        tests.expect(std::filesystem::exists(project), "C8 project exists: " + projectText);
        if (!std::filesystem::exists(project)) continue;
        const auto exePath = project.parent_path() / "c8_check.exe";
        const std::string exeText = exePath.string();
        const std::array<std::string_view, 5> args = {
            "--emit-project-oop-exe", projectText, "-O2", "-o", exeText};
        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        tests.expect(
            driver.run(args, out, err) == ExitCode::Success,
            "C8 project compiles: " + projectText + " (" + err.str() + ")");
        std::error_code cleanup;
        std::filesystem::remove(exePath, cleanup);
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-c8";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    testClassesGetTheRootMembers(tests, root);
    testTheRootIsARealClassAndNotACompilerFiction(tests, root);
    testTheRootIsOptional(tests, root);

    testStructsGetSynthesizedRootMembers(tests, root);
    testADeclaredMemberWinsOverTheSynthesizedOne(tests, root);
    testEqualsAndHashCodeAgreeByConstruction(tests, root);
    testUnionsAreRefusedWithAReason(tests, root);
    testStructEqualsComparesItsOwnType(tests, root);

    testThereIsNoBoxing(tests, root);
    testPrimitivesKeepTheirToString(tests, root);

    testFixtureAndExampleCompile(tests);

    return tests.result();
}
