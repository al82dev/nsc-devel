// Stage 52.x -- `var` keyword: local type inference only (requested by Alberto as its own block,
// separate from the generic-collections work -- see claude/10_stage52_correcciones_cierre.md).
//
// Design, verified against the real compiler while implementing this (not assumed):
//   - `var` is a FULL reserved keyword (TokenKind::KwVar), not contextual like real C#'s `var` --
//     grepping the pre-existing codebase confirmed "var" was never used as an identifier anywhere,
//     so there is no ambiguity to resolve, unlike C#'s own contextual-keyword approach.
//   - The parser bypasses parseType() ENTIRELY for `var`: Parser::parseVariableDeclaration
//     special-cases TokenKind::KwVar and constructs a marker TypeSyntax directly instead. This
//     means `var` is syntactically illegal anywhere parseType() is called directly -- fields,
//     parameters, return types, casts, generic arguments -- which is the correct, deliberately
//     narrow v1 scope (local-variable type inference only).
//   - `raii var x = ...;`/`const var x = ...;` work, since `raii`/`const` are consumed by
//     parseVariableDeclaration BEFORE type-parsing begins. `owned var`/`borrowed var` do NOT work
//     in v1: those qualifiers are parsed inside parseType()'s own prefix logic, which `var`
//     bypasses -- a deliberate, documented limitation, not an oversight.
//   - Semantically, ProjectMethodBodyModel::analyzeVariable intercepts the `var` marker before
//     computing typeKey: requires an initializer (error otherwise), analyzes it first with no
//     delegate-target-typing (matching real C#'s restriction that `var` cannot bind a bare lambda
//     or method group), infers typeKey from the initializer's own resolved type (rejecting
//     builtin:void), and reuses that already-computed ExpressionResult later in the function
//     instead of re-analyzing the initializer a second time.
//   - ProjectNameLookup::bindTypeSyntax skips the `var` marker entirely (no real named type to
//     resolve there -- the whole-AST type-binding pass that runs independently of
//     analyzeVariable would otherwise report a spurious "unknown type name 'var'").

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
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

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}

// Compiles a fixture all the way to a verified, emitted COFF object.
void compileAndEmitObject(TestContext& tests, const std::filesystem::path& directory, std::string_view label)
{
    ProjectCompilation mir;
    std::string error;
    const bool lowered = mir.compileProjectOopMir(directory / (directory.filename().string() + ".nsproj"), error);
    tests.expect(lowered, std::string(label) + ": lowers to OOP MIR (" + error + ")");
    if (!lowered || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), std::string(label) + ": lowers to LLVM (" + lowerer.error() + ")");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), std::string(label) + ": LLVM module verifies clean (" + verifyError + ")");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr, std::string(label) + ": obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), std::string(label) + ": applies data layout");

    const auto object = std::filesystem::temp_directory_path() / "nativesharp_stage52_2_var.obj";
    tests.expect(
        target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        std::string(label) + ": emits a real Windows x64 COFF object (" + targetError + ")");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), std::string(label) + ": object passes structural COFF verification");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);
}

// Positive: `var` infers a builtin value type (int), a for-loop initializer, and (with `raii`) a
// reference type -- all compiling end to end to a verified COFF object.
void testVarInfersValueAndReferenceTypes(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "VarPositive";
    writeProject(directory, "VarPositive");
    writeFile(directory / "Main.ns", R"NS(
namespace VarPositive
{
    public class Payload
    {
        public int Value;
    }

    public class Runner
    {
        public static int Add(int a, int b) { return a + b; }

        public static int Run()
        {
            var x = 5;
            var y = Add(x, 10);
            int total = 0;
            for (var i = 0; i < 3; i = i + 1)
            {
                total = total + i;
            }

            raii var payload = new Payload();
            payload.Value = 42;

            const var frozen = 7;

            return y + total + payload.Value + frozen;
        }
    }
}
)NS");
    compileAndEmitObject(tests, directory, "var infers value and reference types");
}

// Negative: `var` with no initializer is a clean diagnostic, not a crash.
void testVarRequiresInitializer(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "VarNoInit";
    writeProject(directory, "VarNoInit");
    writeFile(directory / "Main.ns", R"NS(
namespace VarNoInit
{
    public class Runner
    {
        public static void Run()
        {
            var x;
        }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "VarNoInit.nsproj", error);
    tests.expect(!ok, "var without an initializer is rejected");
    tests.expect(
        hasDiagnosticText(compilation, "'var' requires an initializer"),
        "var without an initializer reports the expected diagnostic");
}

// Negative: `var` cannot infer `void` from a void-returning call.
void testVarRejectsVoidInitializer(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "VarVoidInit";
    writeProject(directory, "VarVoidInit");
    writeFile(directory / "Main.ns", R"NS(
namespace VarVoidInit
{
    public class Runner
    {
        public static void DoNothing() { }

        public static void Run()
        {
            var x = DoNothing();
        }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "VarVoidInit.nsproj", error);
    tests.expect(!ok, "var cannot infer type 'void'");
    tests.expect(
        hasDiagnosticText(compilation, "'var' cannot infer a type from a void-returning expression"),
        "var-from-void reports the expected diagnostic");
}

// Negative: `var` stays illegal anywhere parseType() is called directly -- a field declaration,
// in this fixture -- exactly the deliberately narrow v1 scope (local-variable inference only).
void testVarIllegalAsFieldType(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "VarField";
    writeProject(directory, "VarField");
    writeFile(directory / "Main.ns", R"NS(
namespace VarField
{
    public class Holder
    {
        public var Value;
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "VarField.nsproj", error);
    tests.expect(!ok, "var is rejected as a field type");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "nativesharp_stage52_2_var_tests";
    std::error_code removeError;
    std::filesystem::remove_all(root, removeError);
    std::filesystem::create_directories(root);

    testVarInfersValueAndReferenceTypes(tests, root);
    testVarRequiresInitializer(tests, root);
    testVarRejectsVoidInitializer(tests, root);
    testVarIllegalAsFieldType(tests, root);

    return tests.result();
}
