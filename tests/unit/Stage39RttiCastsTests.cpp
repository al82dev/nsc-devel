#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodRuntimeTypeOperationKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirNode;
using nativesharp::frontend::ProjectOopHirNodeKind;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
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
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

void expectInvalid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    std::string_view expectedText)
{
    const auto directory = root / std::string(caseName);
    writeProject(directory, caseName);
    writeFile(directory / "Main.ns", source);
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(!ok, std::string(caseName) + " is rejected");
    tests.expect(compilation.diagnostics().hasErrors(), std::string(caseName) + " has an error diagnostic");
    if (!expectedText.empty())
    {
        tests.expect(hasDiagnosticText(compilation, expectedText), std::string(caseName) + " reports the intended rule");
    }
}

std::size_t countBinding(
    const ProjectCompilation& compilation,
    MethodRuntimeTypeOperationKind kind,
    bool targetIsInterface)
{
    if (compilation.methodBodyModel() == nullptr) return 0;
    return static_cast<std::size_t>(std::ranges::count_if(
        compilation.methodBodyModel()->runtimeTypeBindings(),
        [kind, targetIsInterface](const auto& binding)
        {
            return binding.kind == kind && binding.targetIsInterface == targetIsInterface;
        }));
}

std::size_t countHirKind(const ProjectOopHirNode* node, ProjectOopHirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t result = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) result += countHirKind(child.get(), kind);
    return result;
}

std::size_t countHirKind(const ProjectCompilation& compilation, ProjectOopHirNodeKind kind)
{
    if (compilation.oopHirModel() == nullptr) return 0;
    std::size_t result = 0;
    for (const auto& function : compilation.oopHirModel()->functions())
    {
        result += countHirKind(function.body.get(), kind);
    }
    return result;
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr) return 0;
    std::size_t result = node->kind == kind ? 1U : 0U;
    for (const auto& child : node->children) result += countMirKind(child.get(), kind);
    return result;
}

std::size_t countMirKind(const ProjectCompilation& compilation, ProjectOopMirNodeKind kind)
{
    if (compilation.oopMirModel() == nullptr) return 0;
    std::size_t result = 0;
    for (const auto& function : compilation.oopMirModel()->functions())
    {
        result += countMirKind(function.body.get(), kind);
    }
    return result;
}

void testKeywords(TestContext& tests)
{
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::source::SourceManager sources;
    const nativesharp::FileId fileId = sources.addBuffer("stage39_keywords.ns", "is as");
    const auto* file = sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    const auto tokens = lexer.lexAll();
    tests.expect(!diagnostics.hasErrors(), "Stage 39 keyword corpus lexes cleanly");
    tests.expect(tokens.size() == 3, "Stage 39 keyword corpus produces two tokens plus EOF");
    tests.expect(tokens[0].kind() == nativesharp::lexer::TokenKind::KwIs, "is is a reserved keyword");
    tests.expect(tokens[1].kind() == nativesharp::lexer::TokenKind::KwAs, "as is a reserved keyword");
}

void testSemanticAndLoweringPipeline(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Full";
    writeProject(directory, "Stage39Full");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public interface ITag { }
    public interface IOther { }
    public class Base { }
    public class Middle : Base { }
    public class Derived : Middle, ITag { }
    public sealed class Closed : Base { }

    public class Harness
    {
        public static int Run()
        {
            raii Derived d = new Derived();
            borrowed Base b = d;
            bool classTest = b is Derived;
            Derived safeClass = b as Derived;
            Derived checkedClass = (Derived)b;

            bool interfaceTest = b is ITag;
            ITag safeInterface = b as ITag;
            ITag checkedInterface = (ITag)b;

            bool interfaceToClassTest = safeInterface is Derived;
            Derived interfaceToClassSafe = safeInterface as Derived;
            Derived interfaceToClassChecked = (Derived)safeInterface;

            Base nullBase = null;
            bool nullTest = nullBase is Derived;
            Derived nullSafe = nullBase as Derived;
            Derived nullChecked = (Derived)nullBase;

            if (classTest) { return 1; }
            if (interfaceTest) { return 2; }
            if (interfaceToClassTest) { return 3; }
            if (nullTest) { return 4; }
            return 0;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage39Full.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 39 full fixture reaches OOP MIR");
    tests.expect(compilation.methodBodyModel() != nullptr, "Stage 39 builds method-body bindings");
    tests.expect(compilation.oopHirModel() != nullptr, "Stage 39 builds OOP HIR");
    tests.expect(compilation.oopMirModel() != nullptr, "Stage 39 builds OOP MIR");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.oopHirModel() == nullptr ||
        compilation.oopMirModel() == nullptr) return;

    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::TypeTest, false) == 3,
        "class-target is expressions receive runtime type-test bindings");
    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::TypeTest, true) == 1,
        "interface-target is expression receives interface runtime binding");
    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::SafeCast, false) == 3,
        "class-target as expressions receive safe-cast bindings");
    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::SafeCast, true) == 1,
        "interface-target as expression receives interface safe-cast binding");
    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::CheckedCast, false) == 3,
        "dynamic class checked casts receive checked-cast bindings");
    tests.expect(countBinding(compilation, MethodRuntimeTypeOperationKind::CheckedCast, true) == 1,
        "dynamic interface checked cast receives checked-cast binding");

    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::RuntimeTypeTest) == 4,
        "HIR exposes all runtime type tests explicitly");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::SafeReferenceCast) == 4,
        "HIR exposes all safe reference casts explicitly");
    tests.expect(countHirKind(compilation, ProjectOopHirNodeKind::CheckedReferenceCast) == 4,
        "HIR exposes all checked reference casts explicitly");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::RuntimeTypeTest) == 4,
        "MIR preserves runtime type-test operations");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::SafeReferenceCast) == 4,
        "MIR preserves safe reference-cast operations");
    tests.expect(countMirKind(compilation, ProjectOopMirNodeKind::CheckedReferenceCast) == 4,
        "MIR preserves checked reference-cast operations");

    for (const auto& binding : compilation.methodBodyModel()->runtimeTypeBindings())
    {
        tests.expect(!binding.sourceTypeKey.empty(), "runtime type binding retains source type");
        tests.expect(!binding.targetTypeKey.empty(), "runtime type binding retains target type");
        tests.expect(binding.syntax != nullptr, "runtime type binding retains source syntax");
    }
}

void testStaticLegalityRules(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "PrimitiveIs",
        "class A { } class H { public bool F() { int x = 1; return x is A; } }",
        "reference");
    expectInvalid(tests, root, "AsPrimitiveTarget",
        "class A { } class H { public int F(A a) { int x = a as int; return x; } }",
        "class or interface");
    expectInvalid(tests, root, "SealedClassToInterface",
        "interface I { } sealed class A { } class H { public bool F(A a) { return a is I; } }",
        "runtime");
    expectInvalid(tests, root, "InterfaceToSealedNonImplementer",
        "interface I { } sealed class A { } class H { public A F(I i) { return i as A; } }",
        "runtime");
    expectInvalid(tests, root, "UnrelatedSealedClasses",
        "sealed class A { } sealed class B { } class H { public B F(A a) { return (B)a; } }",
        "invalid explicit reference conversion");
    expectInvalid(tests, root, "ImplicitDowncastStillRejected",
        "class A { } class B : A { } class H { public B F(A a) { B b = a; return b; } }",
        "cannot initialize local");
}

void testLlvmAndCoff(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Backend";
    writeProject(directory, "Stage39Backend");
    writeFile(directory / "Main.ns", R"NS(
interface IMarker { }
class Root { }
class Leaf : Root, IMarker { }
class H
{
    public static int Run()
    {
        raii Leaf leaf = new Leaf();
        borrowed Root root = leaf;
        bool classOk = root is Leaf;
        bool interfaceOk = root is IMarker;
        Leaf safeClass = root as Leaf;
        IMarker safeInterface = root as IMarker;
        Leaf checkedClass = (Leaf)root;
        IMarker checkedInterface = (IMarker)root;
        Root absent = null;
        Leaf nullChecked = (Leaf)absent;
        if (classOk) { return 1; }
        if (interfaceOk) { return 2; }
        return 0;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage39Backend.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 39 backend fixture reaches OOP MIR");
    if (!ok || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 39 MIR lowers to LLVM");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 39 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("define internal i1 @__nativesharp_rtti_is_class") != std::string::npos,
        "LLVM defines minimal class ancestry RTTI helper");
    tests.expect(llvm.find("define internal i1 @__nativesharp_rtti_is_interface") != std::string::npos,
        "LLVM defines interface-map RTTI helper");
    tests.expect(llvm.find("call i1 @__nativesharp_rtti_is_class") != std::string::npos,
        "class-target operations call class RTTI helper");
    tests.expect(llvm.find("call i1 @__nativesharp_rtti_is_interface") != std::string::npos,
        "interface-target operations call interface RTTI helper");
    tests.expect(llvm.find("call void @__nativesharp_trap(i32 9)") != std::string::npos,
        "failed checked casts use the dedicated invalid-cast trap path");
    tests.expect(llvm.find("%object.nonnull = icmp ne ptr %object, null") != std::string::npos &&
            llvm.find("rtti.class.fail") != std::string::npos,
        "class RTTI helper has an explicit null path");
    tests.expect(llvm.find("rtti.interface.entry") != std::string::npos &&
            llvm.find("rtti.interface.fail") != std::string::npos,
        "interface RTTI helper has an explicit null path");
    tests.expect(llvm.find("base_descriptor") != std::string::npos || llvm.find("rtti.class.loop") != std::string::npos,
        "class RTTI implementation walks descriptor ancestry");
    tests.expect(llvm.find("interface_map") != std::string::npos || llvm.find("rtti.interface.loop") != std::string::npos,
        "interface RTTI implementation searches interface-map metadata");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 39 obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 39 LLVM accepts Windows x64 layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "Stage 39 LLVM survives O2");
    tests.expect(module->verify(verifyError), "optimized Stage 39 LLVM remains verifier-clean");

    const auto object = directory / "stage39.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 39 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 39 object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 39 object machine is AMD64 / 0x8664");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage39_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testKeywords(tests);
    testSemanticAndLoweringPipeline(tests, root);
    testStaticLegalityRules(tests, root);
    testLlvmAndCoff(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
