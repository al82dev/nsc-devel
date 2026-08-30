#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
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
using nativesharp::frontend::GenericTemplateKind;
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
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

void testPositivePipeline(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage44Positive");
    writeFile(directory / "Main.ns", R"NS(
namespace Demo
{
    public struct Box<T>
    {
        public T Value;
        public T Get() { return Value; }
    }

    public struct Pair<TLeft, TRight>
    {
        public TLeft Left;
        public TRight Right;
    }

    public class Holder<T>
    {
        public T Value;
        public Holder(T value) { Value = value; }
        public T Get() { return Value; }
    }

    public class Algorithms
    {
        public static T Identity<T>(T value) { return value; }
        public static T First<T>(T[] values) { return values[0]; }

        public static int Run()
        {
            Box<int> a;
            a.Value = 7;
            Box<int> repeated;
            repeated.Value = 8;
            Box<long> b;
            b.Value = 9;
            Pair<int, int> pair;
            pair.Left = a.Get();
            pair.Right = repeated.Get();
            Holder<int> holder = new Holder<int>(pair.Left);
            int[] values = new int[2];
            values[0] = holder.Get();
            values[1] = 4;
            int x = Identity<int>(values[0]);
            int y = Identity<int>(values[1]);
            long z = Identity<long>(b.Get());
            int first = First<int>(values);
            delete[] values;
            return x + y + first + (int)z;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage44Positive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 44 generic fixture reaches concrete OOP MIR");
    tests.expect(compilation.genericModel() != nullptr, "Stage 44 exposes a project generic model");
    tests.expect(compilation.nameLookup() != nullptr, "name lookup runs after monomorphization");
    tests.expect(compilation.memberModel() != nullptr, "member binding sees monomorphized declarations");
    tests.expect(compilation.objectLayoutModel() != nullptr, "concrete generic types receive native layouts");
    tests.expect(compilation.oopHirModel() != nullptr, "monomorphized callables reach HIR");
    tests.expect(compilation.oopMirModel() != nullptr, "monomorphized callables reach MIR");
    if (!ok || compilation.genericModel() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr) return;

    const auto templates = compilation.genericModel()->templates();
    const auto specializations = compilation.genericModel()->specializations();
    tests.expect(templates.size() == 5, "five generic templates are registered exactly once");
    tests.expect(specializations.size() == 7, "seven unique concrete specializations are materialized");
    tests.expect(std::ranges::count_if(templates, [](const auto& item) {
        return item.kind == GenericTemplateKind::Struct;
    }) == 2, "two struct templates are distinguished");
    tests.expect(std::ranges::count_if(templates, [](const auto& item) {
        return item.kind == GenericTemplateKind::Class;
    }) == 1, "one class template is distinguished");
    tests.expect(std::ranges::count_if(templates, [](const auto& item) {
        return item.kind == GenericTemplateKind::Method;
    }) == 2, "two method templates are distinguished");
    tests.expect(std::ranges::count_if(specializations, [](const auto& item) {
        return item.specializationKey.find("Box`1<int>") != std::string::npos;
    }) == 1, "repeated Box<int> use hits the specialization cache");
    tests.expect(std::ranges::count_if(specializations, [](const auto& item) {
        return item.specializationKey.find("Identity``1<int>") != std::string::npos;
    }) == 1, "repeated Identity<int> call hits the method specialization cache");
    tests.expect(std::ranges::any_of(specializations, [](const auto& item) {
        return item.specializationKey.find("Box`1<long>") != std::string::npos;
    }), "one template can produce a distinct long specialization");
    tests.expect(std::ranges::any_of(specializations, [](const auto& item) {
        return item.specializationKey.find("Pair`2<int,int>") != std::string::npos;
    }), "two-parameter generic struct substitution is canonicalized");
    tests.expect(std::ranges::any_of(specializations, [](const auto& item) {
        return item.specializationKey.find("Holder`1<int>") != std::string::npos;
    }), "generic class specialization is materialized");
    tests.expect(std::ranges::all_of(specializations, [](const auto& item) {
        return item.specializedSyntax != nullptr && !item.specializedName.empty();
    }), "every cache record points at a concrete synthetic AST declaration");

    const std::string genericDump = nativesharp::frontend::ProjectGenericModelDumper::dump(*compilation.genericModel());
    tests.expect(genericDump.find("templates=5 specializations=7") != std::string::npos,
        "generic dump exposes template and specialization cardinality");
    tests.expect(genericDump.find("Box__g1__int") != std::string::npos,
        "generic dump exposes deterministic concrete type names");
    tests.expect(genericDump.find("Identity__g1__long") != std::string::npos,
        "generic dump exposes deterministic concrete method names");
    tests.expect(genericDump.find("Pair__g2__int__int") != std::string::npos,
        "generic dump preserves generic arity in the synthetic name");

    const std::string hir = nativesharp::frontend::ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
    tests.expect(hir.find("generic-specialization=Stage44Positive::Demo.Algorithms.Identity``1<int>") != std::string::npos,
        "HIR records the canonical method specialization key");
    tests.expect(hir.find("generic-specialization=Stage44Positive::Demo.Box`1<int>") != std::string::npos,
        "HIR records specialization provenance for methods cloned from a generic type");
    const std::string mir = nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
    tests.expect(mir.find("type:Stage44Positive::Demo.Box__g1__int__h") != std::string::npos && mir.find("size=4 align=4") != std::string::npos,
        "MIR uses Stage 40 physical layout for Box<int>");
    tests.expect(mir.find("type:Stage44Positive::Demo.Box__g1__long__h") != std::string::npos && mir.find("size=8 align=8") != std::string::npos,
        "MIR specializes value layout for Box<long>");
    tests.expect(mir.find("type:Stage44Positive::Demo.Pair__g2__int__int__h") != std::string::npos && mir.find("size=8 align=4") != std::string::npos,
        "MIR computes concrete layout after two-parameter substitution");
    tests.expect(mir.find("Holder__g1__int") != std::string::npos,
        "generic class references use the normal native class ABI");
    tests.expect(mir.find("First__g1__int") != std::string::npos && mir.find("builtin:int[]") != std::string::npos,
        "T[] inside a generic method substitutes to the Stage 41 dynamic array representation");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Stage 44 concrete MIR lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Stage 44 LLVM verifies before optimization");
    const std::string llvm = module->text();
    tests.expect(llvm.find("Box__g1__int") != std::string::npos, "LLVM contains concrete Box<int> symbols");
    tests.expect(llvm.find("Box__g1__long") != std::string::npos, "LLVM contains concrete Box<long> symbols");
    tests.expect(llvm.find("Holder__g1__int") != std::string::npos, "LLVM contains the concrete generic class descriptor/callables");
    tests.expect(llvm.find("Identity__g1__int") != std::string::npos, "LLVM contains concrete Identity<int>");
    tests.expect(llvm.find("Identity__g1__long") != std::string::npos, "LLVM contains concrete Identity<long>");
    tests.expect(llvm.find("<T>") == std::string::npos, "LLVM receives no source-level generic parameter syntax");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 44 obtains the Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError), "Stage 44 applies Win64 data layout");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *target, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "monomorphized LLVM survives O2");
    tests.expect(module->verify(verifyError), "optimized Stage 44 LLVM remains verifier-clean");
    const auto object = directory / "stage44.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 44 emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 44 COFF passes structural verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 44 COFF machine is AMD64 / 0x8664");
}


void testAdvancedMonomorphization(TestContext& tests, const std::filesystem::path& root)
{
    {
        const auto directory = root / "NestedRecursive";
        writeProject(directory, "NestedRecursive");
        writeFile(directory / "Main.ns", R"NS(
class Box<T> { public T Value; }
class Node<T> { public Node<T> Next; public T Value; }
class H { public Box<Box<int>> Nested; public Node<int> Root; }
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectNames(directory / "NestedRecursive.nsproj", error);
        tests.expect(ok && error.empty(), "nested and self-recursive generic uses monomorphize without parser whitespace hacks");
        tests.expect(compilation.genericModel() != nullptr && compilation.genericModel()->specializations().size() == 3,
            "nested Box<Box<int>> and recursive Node<int> reuse the specialization cache");
        if (compilation.genericModel() != nullptr)
        {
            const std::string dump = nativesharp::frontend::ProjectGenericModelDumper::dump(*compilation.genericModel());
            tests.expect(dump.find("Box__g1__int__h") != std::string::npos,
                "inner nested generic specialization is concrete");
            tests.expect(dump.find("Node__g1__int__h") != std::string::npos,
                "recursive generic reference resolves to the in-progress cached specialization");
        }
    }

    {
        const auto directory = root / "ArityOverload";
        writeProject(directory, "ArityOverload");
        writeFile(directory / "Main.ns", R"NS(
class Box<T> { public T A; }
class Box<T, U> { public T A; public U B; }
class H
{
    public Box<int> One;
    public Box<int, long> Two;
    public static T I<T>(T value) { return value; }
    public static T I<T, U>(T value, U ignored) { return value; }
    public static int F() { return I<int>(1) + I<int, long>(2, 3); }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(directory / "ArityOverload.nsproj", error);
        tests.expect(ok && error.empty(), "generic identities are disambiguated by arity for types and methods");
        tests.expect(compilation.genericModel() != nullptr && compilation.genericModel()->templates().size() == 4,
            "same source name with different generic arity yields distinct templates");
        tests.expect(compilation.genericModel() != nullptr && compilation.genericModel()->specializations().size() == 4,
            "both generic arities produce independent concrete specializations");
    }

    {
        const auto core = root / "Reference" / "Core";
        const auto app = root / "Reference" / "App";
        writeProject(core, "GenericCore");
        writeFile(core / "Generic.ns", R"NS(
namespace Shared
{
    public class Box<T>
    {
        public T Value;
        public Box(T value) { Value = value; }
        public T Get() { return Value; }
    }
    public class Util { public static T Id<T>(T value) { return value; } }
}
)NS");
        writeFile(app / "GenericApp.nsproj",
            "<Project><PropertyGroup><Name>GenericApp</Name></PropertyGroup><ItemGroup>"
            "<Compile Include=\"**/*.ns\" /><ProjectReference Include=\"../Core/GenericCore.nsproj\" />"
            "</ItemGroup></Project>");
        writeFile(app / "Main.ns", R"NS(
using Shared;
class H
{
    public static int F()
    {
        Box<int> box = new Box<int>(4);
        return Util.Id<int>(box.Get());
    }
}
)NS");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectOopMir(app / "GenericApp.nsproj", error);
        tests.expect(ok && error.empty(), "generic templates remain usable across ProjectReference visibility boundaries");
        tests.expect(compilation.genericModel() != nullptr && compilation.genericModel()->specializations().size() == 2,
            "referenced generic class and generic method are each monomorphized once");
    }
}

void testNegativeContracts(TestContext& tests, const std::filesystem::path& root)
{
    const auto expectGenericReject = [&](std::string_view name, std::string_view source, std::string_view expected) {
        const auto directory = root / std::string(name);
        writeProject(directory, name);
        writeFile(directory / "Main.ns", source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectGenerics(directory / (std::string(name) + ".nsproj"), error);
        tests.expect(!ok, std::string(name) + " is rejected");
        tests.expect(hasDiagnosticText(compilation, expected), std::string(name) + " reports its Stage 44 contract");
    };

    expectGenericReject("WrongTypeArity", R"NS(
class Box<T> { public T Value; }
class H { public static void F() { Box<int, long> value; } }
)NS", "expects 1 type argument(s) but received 2");

    expectGenericReject("OpenGenericUse", R"NS(
class Box<T> { public T Value; }
class H { public static void F() { Box value; } }
)NS", "expects 1 type argument(s) but received 0");

    expectGenericReject("UnknownGenericType", R"NS(
class H { public Missing<int> Value; }
)NS", "is not a visible generic class/struct template");

    expectGenericReject("WrongMethodArity", R"NS(
class H
{
    public static T Identity<T>(T value) { return value; }
    public static int F() { return Identity<int, long>(1); }
}
)NS", "expects 1 type argument(s) but received 2");

    expectGenericReject("UnknownGenericMethod", R"NS(
class H { public static int F() { return Missing<int>(1); } }
)NS", "does not name a unique visible Stage 44 generic method template");

    expectGenericReject("PartialGeneric", R"NS(
partial class Box<T> { public T Value; }
)NS", "partial generic types are deferred beyond Stage 44");

    expectGenericReject("GenericMethodInsideGenericType", R"NS(
class Box<T> { public U Convert<U>(U value) { return value; } }
class H { public static void F() { Box<int> value = null; } }
)NS", "generic methods declared inside generic types are deliberately deferred beyond Stage 44");

    expectGenericReject("DuplicateGenericIdentity", R"NS(
class Box<T> { public T A; }
class Box<U> { public U B; }
)NS", "duplicate generic template identity");

    const auto syntaxReject = [&](std::string_view name, std::string_view source, std::string_view expected) {
        const auto directory = root / std::string(name);
        writeProject(directory, name);
        writeFile(directory / "Main.ns", source);
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectSyntax(directory / (std::string(name) + ".nsproj"), error);
        tests.expect(!ok, std::string(name) + " is rejected during generic syntax validation");
        tests.expect(hasDiagnosticText(compilation, expected), std::string(name) + " reports the parser contract");
    };

    syntaxReject("DuplicateParameter", R"NS(
class Box<T, T> { public T Value; }
)NS", "duplicate generic type parameter");

    syntaxReject("ConstraintsDeferred", R"NS(
class Box<T> where T { public T Value; }
)NS", "generic constraints are deferred beyond Stage 44");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage44_generics_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testPositivePipeline(tests, root);
    testAdvancedMonomorphization(tests, root);
    testNegativeContracts(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
