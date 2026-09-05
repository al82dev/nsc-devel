#include "nativesharp/Frontend/ProjectCompilation.h"
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

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirModel;
using nativesharp::frontend::ProjectOopMirModel;
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

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1u : 0u;
    for (const auto& child : node->children)
    {
        count += countMirKind(child.get(), kind);
    }
    return count;
}

const ProjectOopMirNode* findMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind == kind)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* found = findMirKind(child.get(), kind); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

void testEndToEndOopLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "EndToEnd";
    writeProject(directory, "Stage30Test");
    writeFile(directory / "A.ns",
        "namespace Demo; public partial class Counter { byte Tag; int Value; "
        "public Counter(int start) { this.Value = start; } "
        "public int Add(int delta) { this.Value = this.Value + delta; return this.Value; } "
        "public int SumTo(int n) { int i = 0; int total = 0; while (i < n) { total = total + i; i = i + 1; } return total; } }");
    writeFile(directory / "B.ns",
        "namespace Demo; public partial class Counter { "
        "public int AddTwice(int delta) { return Add(delta) + Add(delta); } "
        "public Counter Make(int start) { return new Counter(start); } "
        "public Counter MakeAfterAdd(int delta) { return new Counter(Add(delta)); } } "
        "public class Empty { } public class Factory { public Empty MakeEmpty() { return new Empty(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage30Test.nsproj", error);
    tests.expect(ok, "Stage 30 project compiles through OOP MIR");
    tests.expect(error.empty(), "Stage 30 OOP MIR build has no loader/lowering error");
    tests.expect(compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr,
        "ProjectCompilation retains both OOP HIR and OOP MIR models");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const ProjectOopHirModel& hir = *compilation.oopHirModel();
    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 7 && mir.functions().size() == 7,
        "seven explicit instance callables lower through both OOP IR layers");
    tests.expect(hir.functions()[0].mangledName.starts_with("__nsc_ctor_") &&
            hir.functions()[0].parameterTypeKeys.size() == 1,
        "constructor HIR has deterministic mangling and excludes implicit this from source parameter list");
    tests.expect(mir.functions()[0].constructor && mir.functions()[0].returnTypeKey == "builtin:void",
        "constructor MIR freezes void-return internal ABI");

    std::size_t fieldLoads = 0;
    std::size_t fieldAddresses = 0;
    std::size_t directCalls = 0;
    std::size_t constructions = 0;
    std::size_t loops = 0;
    for (const auto& function : mir.functions())
    {
        fieldLoads += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldLoad);
        fieldAddresses += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldAddress);
        directCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        constructions += countMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        loops += countMirKind(function.body.get(), ProjectOopMirNodeKind::While);
    }
    tests.expect(fieldLoads >= 2 && fieldAddresses >= 2,
        "field reads/writes survive into OOP MIR as explicit address/load operations");
    tests.expect(directCalls == 3, "three Add(delta) calls become direct instance-call MIR operations");
    tests.expect(constructions == 3, "two explicit and one implicit-default new expressions become allocation MIR operations");
    tests.expect(loops == 1, "structured while control flow survives into OOP MIR");

    const ProjectOopMirNode* fieldAddress = findMirKind(mir.functions()[0].body.get(), ProjectOopMirNodeKind::FieldAddress);
    tests.expect(fieldAddress != nullptr && fieldAddress->immediate == 12 && fieldAddress->alignment == 4,
        "MIR consumes the Macro A object header and resolves Counter.Value to byte offset twelve");

    const ProjectOopMirNode* explicitConstruction = nullptr;
    const ProjectOopMirNode* defaultConstruction = nullptr;
    for (const auto& function : mir.functions())
    {
        const ProjectOopMirNode* construction = findMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        if (construction != nullptr)
        {
            if (construction->calleeName.empty()) defaultConstruction = construction;
            else explicitConstruction = construction;
        }
    }
    tests.expect(explicitConstruction != nullptr && explicitConstruction->immediate == 16 &&
            explicitConstruction->zeroInitialize && explicitConstruction->allocationFailureReturnsNull,
        "explicit construction MIR consumes the header-inclusive object size plus zero-init/null-on-failure contract");
    tests.expect(defaultConstruction != nullptr && defaultConstruction->immediate == 8 && defaultConstruction->calleeName.empty(),
        "root implicit default construction allocates the descriptor-only object without synthesizing a constructor entry point");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("abi=this:first") != std::string::npos &&
            hirDump.find("object-construction") != std::string::npos,
        "OOP HIR dump exposes internal ABI and semantic construction nodes");
    tests.expect(mirDump.find("offset=12 align=4") != std::string::npos &&
            mirDump.find("zero=true failure=null") != std::string::npos,
        "OOP MIR dump exposes header-adjusted field offsets and allocation contract");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), mir);
    tests.expect(module != nullptr, "Stage 30 lowers project OOP MIR to an LLVM module");
    tests.expect(lowerer.error().empty(), "LLVM OOP lowering reports no internal error for valid project");
    if (module == nullptr)
    {
        return;
    }
    std::string verificationError;
    tests.expect(module->verify(verificationError), "LLVM verifier accepts Stage 30 OOP module");
    tests.expect(verificationError.empty(), "verified Stage 30 LLVM module has no verifier diagnostics");

    const std::string llvm = module->text();
    tests.expect(llvm.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
        "OOP LLVM module uses NativeSharp Windows x64 target triple");
    tests.expect(llvm.find("declare ptr @__nativesharp_alloc(i64)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp allocation hook");
    tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp deallocation hook");
    tests.expect(llvm.find("@llvm.memset.p0.i64") != std::string::npos,
        "OOP object initialization materializes zero-initialization with LLVM memset");
    tests.expect(llvm.find("getelementptr i8, ptr %this, i64 12") != std::string::npos,
        "field addressing lowers exactly to the explicit receiver plus the Macro A byte offset");
    // NSC-LIMIT-009. LA BUSQUEDA LLEVA `@\"` Y NO `@`, y no es un adorno: todo simbolo que sale
    // de una identidad lleva el `$3A$3A` del `::` del modulo, y LLVM entrecomilla cualquier
    // nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Lo impreso es
    // `@\"__nsc_...\"`. Escrito a mano sin la comilla encuentra CERO, y en una asercion de
    // ausencia eso seria un verde falso. Ver TypeIdentity.h e `irTextSymbolReference`.
    tests.expect(llvm.find("call void @\"__nsc_ctor_") != std::string::npos,
        "explicit object construction lowers selected constructor call after allocation");
    tests.expect(llvm.find("call i32 @\"__nsc_method_") != std::string::npos,
        "instance method calls lower to direct internal-ABI calls with implicit this");
    tests.expect(llvm.find("while.cond") != std::string::npos && llvm.find("while.body") != std::string::npos,
        "structured OOP while lowers to explicit LLVM control-flow blocks");
    tests.expect(llvm.find("alloc.failed") != std::string::npos && llvm.find("object.init") != std::string::npos,
        "allocation failure is checked before zero-init/constructor execution");

    const std::size_t makeAfterStart = llvm.find(
        "define ptr @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__MakeAfterAdd__builtin$3Aint");
    const std::size_t makeAfterEnd = makeAfterStart == std::string::npos ? std::string::npos : llvm.find("\n}", makeAfterStart);
    const std::size_t argumentCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find(
        "call i32 @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__Add__builtin$3Aint",
        makeAfterStart);
    const std::size_t allocationCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find("call ptr @__nativesharp_alloc", makeAfterStart);
    tests.expect(
        makeAfterStart != std::string::npos && makeAfterEnd != std::string::npos &&
            argumentCall != std::string::npos && allocationCall != std::string::npos &&
            argumentCall < allocationCall && allocationCall < makeAfterEnd,
        "object-construction arguments are evaluated before allocation and dominate the success branch");

    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr && targetError.empty(), "Windows x64 target machine is available for OOP codegen");
    if (targetMachine == nullptr)
    {
        return;
    }
    tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
        "target data layout is applied to verified OOP LLVM module");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "standard LLVM O2 pipeline accepts project OOP module");

    const auto objectPath = directory / "stage30.obj";
    tests.expect(targetMachine->emit(*module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "optimized project OOP LLVM module emits a COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
    tests.expect(objectVerifier.verify(objectPath), "Stage 30 emitted object passes structural COFF verification");
    tests.expect(objectVerifier.info().machine == 0x8664 && objectVerifier.info().sectionCount > 0,
        "Stage 30 OOP object targets AMD64 and contains sections");
}

void testDeallocationContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Deallocation";
    writeProject(directory, "Deallocation");
    writeFile(directory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete value; delete null; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Deallocation.nsproj", error);
    tests.expect(ok, "class-reference delete lowers through Stage 30 OOP MIR");
    tests.expect(error.empty(), "valid class-reference delete has no project lowering error");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto& hir = *compilation.oopHirModel();
    const auto& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 1 && mir.functions().size() == 1,
        "deallocation fixture produces one instance callable");
    tests.expect(countMirKind(mir.functions().front().body.get(), ProjectOopMirNodeKind::Deallocate) == 2,
        "delete reference and delete null both become explicit OOP MIR deallocations");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("deallocation type=builtin:void") != std::string::npos,
        "OOP HIR dump exposes deallocation as a first-class lifetime operation");
    tests.expect(mirDump.find("deallocate type=builtin:void") != std::string::npos,
        "OOP MIR dump preserves executable deallocation intent");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr && lowerer.error().empty(),
        "class-reference deallocation lowers to verified LLVM");
    if (module != nullptr)
    {
        std::string verificationError;
        tests.expect(module->verify(verificationError) && verificationError.empty(),
            "LLVM verifier accepts class-reference deallocation");
        const std::string llvm = module->text();
        tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
            "Stage 30 LLVM declares the NativeSharp free hook");
        std::size_t calls = 0;
        std::size_t position = 0;
        while ((position = llvm.find("call void @__nativesharp_free(ptr", position)) != std::string::npos)
        {
            ++calls;
            ++position;
        }
        tests.expect(calls == 2,
            "delete reference and delete null each materialize one runtime free call; runtime free owns null no-op semantics");

        std::string targetError;
        auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
        tests.expect(targetMachine != nullptr && targetError.empty(),
            "deallocation fixture has Windows x64 target machine");
        if (targetMachine != nullptr)
        {
            tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
                "deallocation LLVM receives Windows x64 data layout");
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
                "deallocation LLVM survives O2 optimization");
            const auto objectPath = directory / "deallocation.obj";
            tests.expect(targetMachine->emit(
                    *module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
                "deallocation LLVM emits Windows x64 COFF");
            nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
            tests.expect(objectVerifier.verify(objectPath) && objectVerifier.info().machine == 0x8664,
                "deallocation object passes AMD64 COFF verification");
        }
    }

    const auto invalidDirectory = root / "DeleteArrayClass";
    writeProject(invalidDirectory, "DeleteArrayClass");
    writeFile(invalidDirectory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete[] value; } }");
    ProjectCompilation invalidCompilation;
    error.clear();
    const bool invalidOk = invalidCompilation.compileProjectOopMir(
        invalidDirectory / "DeleteArrayClass.nsproj", error);
    tests.expect(!invalidOk, "Stage 41 still rejects delete[] on a plain class reference; only T[] is a dynamic-array reference");
    tests.expect(error.find("delete[] requires a native dynamic array reference") != std::string::npos,
        "plain class-reference delete[] boundary remains explicit after dynamic arrays were introduced");
}

void testAggregateCallableBoundary(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "AggregateBoundary";
    writeProject(directory, "AggregateBoundary");
    writeFile(directory / "Main.ns",
        "struct Large { int A; int B; int Z; } class Box { public Large Echo(Large value) { return value; } }");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "AggregateBoundary.nsproj", error);
    tests.expect(ok, "large aggregate-by-value callable remains semantically valid through project OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr)
    {
        return;
    }
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    // HISTORY: this test used to assert the OPPOSITE -- that a 12-byte aggregate crossing a
    // callable boundary was REJECTED ("Stage 40 direct struct value ABI supports only 1, 2, 4
    // or 8-byte aggregates"), pinning the documented limitation so it could not silently turn
    // into a miscompile. The limitation is now implemented instead of documented: any aggregate
    // whose size is not 1/2/4/8 travels indirectly, exactly the way clang lowers it for
    // x86_64-pc-windows-msvc -- the parameter as a pointer to a caller-owned per-call copy, the
    // return through a leading sret pointer -- so the same fixture must now LOWER, and the IR
    // must show the indirect shape rather than any direct aggregate value in the signature.
    tests.expect(module != nullptr && lowerer.error().empty(),
        "a 12-byte aggregate-by-value callable now lowers through the Win64 indirect ABI (" +
            lowerer.error() + ")");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError),
            "indirect-aggregate module verifies clean (" + verifyError + ")");
        const std::string llvm = module->text();
        const std::size_t echoAt = llvm.find("define ");
        tests.expect(echoAt != std::string::npos &&
                llvm.find("sret([12 x i8])") != std::string::npos,
            "the Large return travels through an sret([12 x i8]) slot, not a direct value");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_project_oop_lowering_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testEndToEndOopLowering(tests, root);
    testDeallocationContract(tests, root);
    testAggregateCallableBoundary(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
