// Stage 51.x -- Enum support in the OOP pipeline.
//
// Before this fix, an enum declared with `public enum Name { ... }` was fully usable through the
// classic single-file semantic pipeline, but entirely unusable from a class/struct method body in
// the newer OOP pipeline: `EnumType.Member` failed name resolution ("unknown identifier"), and
// even a cast-to-int workaround failed at LLVM lowering ("unsupported Stage 34 LLVM conversion").
// This gap is what forced Stage 51 "Complete String" to expose `bool ignoreCase` instead of a real
// `StringComparison` parameter -- see lib/NativeSharp.Core/String.ns.
//
// This test covers the fix end to end: member registration (ProjectMemberModel), qualified access
// and equality/parameter type-checking (ProjectMethodBodyModel), constant-field materialization
// (ProjectOopMirModel, reusing the Stage 37 native constant-field mechanism), and LLVM lowering
// (constant globals + explicit enum<->int cast), for auto-incremented members, literal members
// and typed integral constant expressions over preceding members.

#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ClassMemberKind;
using nativesharp::frontend::ClassMemberSymbol;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

const ClassMemberSymbol* findEnumConstant(
    const ProjectCompilation& compilation, std::string_view typeSimpleName, std::string_view name)
{
    if (compilation.memberModel() == nullptr) return nullptr;
    for (const ClassMemberSymbol& member : compilation.memberModel()->members())
    {
        if (member.isEnumConstant && member.name == name &&
            member.declaredTypeKey.find(typeSimpleName) != std::string::npos)
        {
            return &member;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------
// 1. Member registration: auto-increment, literals and constant expressions, at member-model level.
// ---------------------------------------------------------------------------------------

void testEnumMemberRegistration(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Registration";
    writeProject(directory, "Registration");
    writeFile(directory / "Main.ns", R"NS(
public enum Mode
{
    First,
    Second,
    Third
}

public enum Explicit
{
    Alpha = 10,
    Beta,
    Gamma = 100
}

public enum ColorWriteChannels
{
    Red = 1,
    Green = 2,
    Blue = 4,
    Alpha = 8,
    All = Red | Green | Blue | Alpha
}

class H
{
    public static int Touch() { return 0; }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Registration.nsproj", error);
    tests.expect(ok && error.empty(), "enum declarations fixture reaches OOP MIR");
    if (!ok) return;

    const ClassMemberSymbol* first = findEnumConstant(compilation, "Mode", "First");
    const ClassMemberSymbol* second = findEnumConstant(compilation, "Mode", "Second");
    const ClassMemberSymbol* third = findEnumConstant(compilation, "Mode", "Third");
    tests.expect(first != nullptr && first->enumConstantValue == 0, "auto-increment enum member starts at 0");
    tests.expect(second != nullptr && second->enumConstantValue == 1, "auto-increment enum member #2 is 1");
    tests.expect(third != nullptr && third->enumConstantValue == 2, "auto-increment enum member #3 is 2");
    tests.expect(first != nullptr && first->isStatic && first->isConst && first->isReadonly,
        "enum member is registered as a static const readonly field (reuses Stage 37 machinery)");
    tests.expect(
        first != nullptr && first->declaredTypeKey != "builtin:int" && first->declaredTypeKey.find("Mode") != std::string::npos,
        "enum member's declared type is the enum itself, not its int-sized underlying storage");

    const ClassMemberSymbol* alpha = findEnumConstant(compilation, "Explicit", "Alpha");
    const ClassMemberSymbol* beta = findEnumConstant(compilation, "Explicit", "Beta");
    const ClassMemberSymbol* gamma = findEnumConstant(compilation, "Explicit", "Gamma");
    tests.expect(alpha != nullptr && alpha->enumConstantValue == 10, "explicit-literal enum member keeps its value");
    tests.expect(beta != nullptr && beta->enumConstantValue == 11, "auto-increment resumes from the last explicit value");
    tests.expect(gamma != nullptr && gamma->enumConstantValue == 100, "a second explicit literal overrides the running counter");

    const ClassMemberSymbol* all = findEnumConstant(compilation, "ColorWriteChannels", "All");
    tests.expect(
        all != nullptr && all->enumConstantValue == 15,
        "a bitwise expression over preceding enum members is folded in an enum initializer");
}

// ---------------------------------------------------------------------------------------
// 2. Qualified access, equality, parameter passing, and explicit cast -- end to end to LLVM.
// ---------------------------------------------------------------------------------------

void testEnumUsageEndToEnd(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Usage";
    writeProject(directory, "Usage");
    writeFile(directory / "Main.ns", R"NS(
public enum Mode
{
    First = 0,
    Second = 1,
    Third
}

public enum Buttons : byte
{
    None = 0,
    Left = 1,
    Right = 2
}

class Checker
{
    public static bool IsSecond(Mode value)
    {
        return value == Mode.Second;
    }

    public static bool IsPressed(Buttons buttons, Buttons button)
    {
        return (buttons & button) != 0;
    }

    public static bool IsNone(Buttons buttons)
    {
        return 0 == buttons;
    }

    public static int Run()
    {
        Mode m = Mode.Second;
        if (!IsSecond(m)) { return 1; }
        if (IsSecond(Mode.First)) { return 2; }
        if ((int)Mode.Third != 2) { return 3; }
        if (!IsPressed(Buttons.Left, Buttons.Left)) { return 4; }
        if (IsPressed(Buttons.Left, Buttons.Right)) { return 5; }
        if (!IsNone(Buttons.None)) { return 6; }
        return 0;
    }
}
)NS");

    ProjectCompilation methodBodies;
    std::string error;
    tests.expect(methodBodies.compileProjectMethodBodies(directory / "Usage.nsproj", error),
        "qualified enum access, equality, parameter passing and explicit cast all type-check");
    tests.expect(error.empty(), "no loader error on the enum usage fixture");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(directory / "Usage.nsproj", error), "enum usage fixture lowers to OOP MIR");
    tests.expect(error.empty(), "no MIR lowering error on the enum usage fixture");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "enum usage fixture lowers to LLVM (Type.Member read, equality, explicit (int) cast)");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "enum usage fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("constant i32 0") != std::string::npos, "Mode.First lowers to a real LLVM constant i32 global");
    tests.expect(llvm.find("constant i32 1") != std::string::npos, "Mode.Second lowers to a real LLVM constant i32 global");
    tests.expect(llvm.find("constant i32 2") != std::string::npos, "Mode.Third lowers to a real LLVM constant i32 global");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "enum usage fixture obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "enum usage fixture applies Windows x64 data layout");

    const auto object = std::filesystem::temp_directory_path() / "nativesharp_stage51_enum_usage.obj";
    tests.expect(
        target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "enum usage fixture emits a real Windows x64 COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "enum usage fixture object passes structural COFF verification");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);
}

// ---------------------------------------------------------------------------------------
// 3. Regression guard: duplicate members and out-of-range explicit values are still rejected.
// ---------------------------------------------------------------------------------------

void testEnumValidationStillRejectsBadInput(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "Duplicate";
        writeProject(directory, "Duplicate");
        writeFile(directory / "Main.ns", R"NS(
public enum Mode
{
    First,
    First
}
class H
{
    public static int Touch() { return 0; }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "Duplicate.nsproj", error);
        tests.expect(!ok || !error.empty(), "duplicate enum member name is still rejected as a semantic error");
    }
    {
        const auto directory = root / "NonConstant";
        writeProject(directory, "NonConstant");
        writeFile(directory / "Main.ns", R"NS(
public enum Mode
{
    First = H.Value()
}
class H
{
    public static int Value() { return 2; }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "NonConstant.nsproj", error);
        tests.expect(!ok || !error.empty(),
            "a dynamic enum initializer is rejected rather than silently mis-evaluated");
    }
    {
        const auto directory = root / "EnumNonZeroComparison";
        writeProject(directory, "EnumNonZeroComparison");
        writeFile(directory / "Main.ns", R"NS(
public enum Buttons { None = 0, Left = 1 }
class H
{
    public static bool Invalid(Buttons buttons) { return buttons != 1; }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMethodBodies(
            directory / "EnumNonZeroComparison.nsproj", error);
        tests.expect(!ok || !error.empty(),
            "an enum comparison with a nonzero integer literal remains rejected");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage51_enum_oop_support_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testEnumMemberRegistration(tests, root);
    testEnumUsageEndToEnd(tests, root);
    testEnumValidationStillRejectsBadInput(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
