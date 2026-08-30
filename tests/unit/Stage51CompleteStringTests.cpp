// Stage 51 -- Complete String.
//
// Cubre dos cosas distintas a proposito:
//  1. La correccion de promocion entera que Alberto encontro y arreglo en este stage
//     (ulong / <literal entero> se promovia incorrectamente a double). Esta es la
//     regresion mas importante del stage: si alguien la rompe accidentalmente en el
//     futuro, este test debe fallar de inmediato, a nivel semantico y a nivel LLVM.
//  2. Que el proyecto de ejemplo empaquetado (examples/project_stage51), que ejercita
//     la superficie ampliada de NativeSharp.Core.String, compila limpio de extremo a
//     extremo: semantica -> HIR -> MIR -> LLVM -> objeto COFF real.
//
// No se ejecuta el binario resultante (no hay Windows/Wine en el entorno de CI de este
// repo): la verificacion de comportamiento en tiempo de ejecucion de cada familia de
// String queda en el propio examples/project_stage51/Main.ns, con checks explicitos que
// solo se pueden confirmar corriendo el .exe en Windows real.

#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodLocalSymbol;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view extraItems = {})
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup>" + std::string(extraItems) +
            "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

const MethodLocalSymbol* findLocal(const ProjectCompilation& compilation, std::string_view name)
{
    if (compilation.methodBodyModel() == nullptr) return nullptr;
    for (const auto& local : compilation.methodBodyModel()->locals())
    {
        if (local.name == name) return &local;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------
// 1. Regresion de la promocion entera ulong / <literal>.
// ---------------------------------------------------------------------------------------

void testUlongDivisionStaysIntegral(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "UlongDivision";
    writeProject(directory, "UlongDivision");
    writeFile(directory / "Main.ns", R"NS(
class H
{
    public static ulong Run(ulong dividend)
    {
        ulong quotient = dividend / 10;
        ulong remainder = dividend % 10;
        return quotient + remainder;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "UlongDivision.nsproj", error);
    tests.expect(ok && error.empty(), "ulong / <int literal> fixture reaches OOP MIR");
    if (!ok) return;

    const MethodLocalSymbol* quotient = findLocal(compilation, "quotient");
    const MethodLocalSymbol* remainder = findLocal(compilation, "remainder");
    tests.expect(quotient != nullptr && quotient->typeKey == "builtin:ulong",
        "Stage 51 fix: 'ulong / 10' keeps the ulong type (regression for the double-promotion bug)");
    tests.expect(remainder != nullptr && remainder->typeKey == "builtin:ulong",
        "Stage 51 fix: 'ulong % 10' keeps the ulong type (same promotion path as division)");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "ulong / <int literal> fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "ulong division fixture LLVM verifies clean");
    const std::string llvm = module->text();
    tests.expect(llvm.find("udiv") != std::string::npos,
        "Stage 51 fix: division lowers to integer 'udiv', not floating-point 'fdiv'");
    tests.expect(llvm.find("urem") != std::string::npos,
        "Stage 51 fix: modulo lowers to integer 'urem', not floating-point 'frem'");
    tests.expect(llvm.find("fdiv") == std::string::npos,
        "Stage 51 fix: no floating-point division appears anywhere in this fixture");
    tests.expect(llvm.find("sitofp") == std::string::npos && llvm.find("uitofp") == std::string::npos,
        "Stage 51 fix: no int-to-float conversion is inserted for this arithmetic");
}

// Caso de control: cuando uno de los operandos SI es double, la promocion a double sigue
// siendo la correcta (el fix es sobre constantes enteras, no sobre aritmetica mixta real).
void testMixedDoubleStillPromotes(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MixedDouble";
    writeProject(directory, "MixedDouble");
    writeFile(directory / "Main.ns", R"NS(
class H
{
    public static double Run(ulong dividend, double factor)
    {
        double result = dividend / factor;
        return result;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "MixedDouble.nsproj", error);
    tests.expect(ok && error.empty(), "ulong/double mixed fixture reaches OOP MIR");
    if (!ok) return;
    const MethodLocalSymbol* result = findLocal(compilation, "result");
    tests.expect(result != nullptr && result->typeKey == "builtin:double",
        "control case: ulong divided by an actual double still promotes to double");
}

// ---------------------------------------------------------------------------------------
// 2. El proyecto de ejemplo Stage 51 empaquetado compila de extremo a extremo.
// ---------------------------------------------------------------------------------------

void testPackagedExample(TestContext& tests)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage51/Stage51String.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 51 Complete String example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        "packaged Stage 51 example passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 51 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        "packaged Stage 51 example lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 51 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "packaged Stage 51 example lowers to LLVM (NativeSharp.Core.String's ~70 new methods included)");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 51 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 51 example obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 51 example applies Windows x64 data layout");

    const auto object = std::filesystem::temp_directory_path() / "nativesharp_stage51_example.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 51 example emits Windows x64 COFF for its full String surface");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 51 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 51 example object machine is AMD64 / 0x8664");
    std::error_code cleanupError;
    std::filesystem::remove(object, cleanupError);
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage51_complete_string_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testUlongDivisionStaysIntegral(tests, root);
    testMixedDoubleStillPromotes(tests, root);
    testPackagedExample(tests);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
