#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

template <typename Node>
void collectNodes(const Node* node, std::vector<const Node*>& output)
{
    if (node == nullptr) return;
    output.push_back(node);
    for (const auto& child : node->children) collectNodes(child.get(), output);
}

std::string functionBody(std::string_view ir, std::string_view nameFragment)
{
    const std::size_t at = ir.find(nameFragment);
    if (at == std::string_view::npos) return {};
    const std::size_t open = ir.find('{', at);
    if (open == std::string_view::npos) return {};
    const std::size_t close = ir.find("\n}", open);
    if (close == std::string_view::npos) return {};
    return std::string(ir.substr(open, close - open));
}
}

int main()
{
    using namespace nativesharp::frontend;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_integrated_assembly_mir_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "AssemblyMir.nsproj",
        "<Project><PropertyGroup><Name>AssemblyMir</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        int source = 40;
        int result;
        asm {
            mov $out0, $0
            add $out0, $1
        } out(result : "={rax}") in(source : "m", 2 : "i")
          clobber("cc", "memory", "rbx");
        return result;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(root / "AssemblyMir.nsproj", error);
    tests.expect(built && error.empty(), "validated asm lowers through project OOP MIR");

    if (built && compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr)
    {
        std::vector<const ProjectOopHirNode*> hir;
        std::vector<const ProjectOopMirNode*> mir;
        for (const auto& function : compilation.oopHirModel()->functions())
            collectNodes(function.body.get(), hir);
        for (const auto& function : compilation.oopMirModel()->functions())
            collectNodes(function.body.get(), mir);

        const auto hirOf = [&](ProjectOopHirNodeKind kind)
        {
            std::vector<const ProjectOopHirNode*> found;
            for (const auto* node : hir) if (node->kind == kind) found.push_back(node);
            return found;
        };
        const auto mirOf = [&](ProjectOopMirNodeKind kind)
        {
            std::vector<const ProjectOopMirNode*> found;
            for (const auto* node : mir) if (node->kind == kind) found.push_back(node);
            return found;
        };

        const auto hirAsm = hirOf(ProjectOopHirNodeKind::InlineAssembly);
        const auto mirAsm = mirOf(ProjectOopMirNodeKind::InlineAssembly);
        tests.expect(hirAsm.size() == 1 && mirAsm.size() == 1,
            "HIR and MIR each contain one first-class inline-assembly operation");
        tests.expect(!hirAsm.empty() && !mirAsm.empty() &&
                hirAsm[0]->stringValue == mirAsm[0]->stringValue &&
                hirAsm[0]->stringValue.find("mov $out0, $0") != std::string::npos,
            "the exact assembly source bytes survive HIR-to-MIR lowering");
        tests.expect(!mirAsm.empty() && mirAsm[0]->inlineAssemblyHasSideEffects &&
                mirAsm[0]->inlineAssemblyClobbersMemory &&
                mirAsm[0]->inlineAssemblyClobbersFlags,
            "MIR summarizes mandatory side effects plus explicit memory and flag barriers");

        const auto hirLines = hirOf(ProjectOopHirNodeKind::InlineAssemblyLine);
        const auto mirLines = mirOf(ProjectOopMirNodeKind::InlineAssemblyLine);
        tests.expect(hirLines.size() == 2 && mirLines.size() == 2 &&
                mirLines[0]->stringValue == "mov $out0, $0" &&
                mirLines[1]->stringValue == "add $out0, $1" &&
                mirLines[0]->syntax != nullptr && mirLines[0]->syntax->range().isValid(),
            "instruction records retain order, exact spelling, and source identity");

        const auto inputs = mirOf(ProjectOopMirNodeKind::InlineAssemblyInput);
        const auto outputs = mirOf(ProjectOopMirNodeKind::InlineAssemblyOutput);
        tests.expect(inputs.size() == 2 && outputs.size() == 1 &&
                inputs[0]->integerValue == 0 && inputs[0]->stringValue == "m" &&
                inputs[0]->children.size() == 1 &&
                inputs[0]->children[0]->kind == ProjectOopMirNodeKind::LocalAddress &&
                inputs[1]->integerValue == 1 && inputs[1]->stringValue == "i" &&
                inputs[1]->children.size() == 1 &&
                inputs[1]->children[0]->kind == ProjectOopMirNodeKind::IntegerConstant &&
                outputs[0]->integerValue == 0 && outputs[0]->stringValue == "={rax}" &&
                outputs[0]->booleanValue && outputs[0]->children.size() == 1 &&
                outputs[0]->children[0]->kind == ProjectOopMirNodeKind::LocalAddress,
            "MIR operands preserve ordinals, constraints, explicitness, values, and addresses");

        const auto clobbers = mirOf(ProjectOopMirNodeKind::InlineAssemblyClobber);
        tests.expect(clobbers.size() == 3 &&
                clobbers[0]->stringValue == "cc" && clobbers[0]->elementTypeKey == "flags" &&
                clobbers[1]->stringValue == "memory" &&
                clobbers[1]->elementTypeKey == "memory" &&
                clobbers[2]->stringValue == "rbx" &&
                clobbers[2]->elementTypeKey == "register" && clobbers[2]->booleanValue,
            "MIR clobbers preserve effect categories and nonvolatile preservation requirements");

        const std::string hirDump = ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
        const std::string mirDump = ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
        tests.expect(hirDump.find("inline-assembly-output") != std::string::npos &&
                hirDump.find("constraint=={rax}") != std::string::npos &&
                mirDump.find("schema=1.7") != std::string::npos &&
                mirDump.find("side-effects=true memory-barrier=true flags=true") !=
                    std::string::npos &&
                mirDump.find("category=memory") != std::string::npos,
            "HIR/MIR dumps expose the complete assembly contract and schema revision");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "the structured assembly contract lowers to LLVM inline asm: " + lowerer.error());
        if (module != nullptr)
        {
            std::string verifyError;
            tests.expect(module->verify(verifyError),
                "LLVM accepts the integrated assembly function and constraint types");
            const std::string llvm = module->text();
            tests.expect(llvm.find("asm sideeffect inteldialect") != std::string::npos &&
                    llvm.find("={rax},*m,i,~{cc},~{memory},~{rbx}") != std::string::npos &&
                    llvm.find("asm.result") != std::string::npos,
                "LLVM IR exposes Intel syntax, normalized constraints, clobbers, and output value");

            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, targetError);
            tests.expect(target != nullptr && targetError.empty(),
                "the Windows x64 O0 target is available for the assembly probe");
            if (target != nullptr)
            {
                std::string emitError;
                const auto object = root / "AssemblyMir.obj";
                tests.expect(target->configureModuleForTarget(*module, emitError) &&
                        target->emit(
                            *module,
                            object,
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object,
                            emitError) &&
                        std::filesystem::exists(object) &&
                        std::filesystem::file_size(object) > 0,
                    "the LLVM x64 backend assembles the integrated block into a COFF object");
            }
        }
    }

    const auto stress = root / "stress";
    writeFile(stress / "AssemblyStress.nsproj",
        "<Project><PropertyGroup><Name>AssemblyStress</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(stress / "Stress.ns", R"NS(
public class Stress
{
    public static int ReadWrite(int value)
    {
        asm { add $out0, 2 } out(value : "+r") clobber("cc");
        return value;
    }

    public static void MemoryOutput(int* destination)
    {
        asm { mov dword ptr $out0, 42 } out(*destination : "=m") clobber("memory");
    }

    public static void MemoryReadWrite(int* destination)
    {
        asm { inc dword ptr $out0 } out(*destination : "+m") clobber("cc", "memory");
    }

    public static int FixedInput(int value)
    {
        int result;
        asm { mov $out0, $0 } out(result : "={eax}") in(value : "{ecx}");
        return result;
    }

    public static int TwoOutputs()
    {
        int left;
        int right;
        asm {
            mov $out0, 40
            mov $out1, 2
        } out(left : "=r", right : "=r");
        return left + right;
    }

    public static int LocalBranch()
    {
        int result;
        asm {
            xor eax, eax
        Again:
            inc eax
            cmp eax, 2
            jl Again
        } out(result : "={eax}") clobber("cc");
        return result;
    }

    public static void Touch()
    {
        asm { nop }
    }
}
)NS");

    ProjectCompilation stressCompilation;
    std::string stressError;
    const bool stressBuilt = stressCompilation.compileProjectOopMir(
        stress / "AssemblyStress.nsproj", stressError);
    tests.expect(stressBuilt && stressError.empty(),
        "read-write, memory, multi-output, local-control, and operand-free asm reach MIR");
    if (stressBuilt)
    {
        nativesharp::llvm_backend::ProjectOopLlvmLowerer stressLowerer;
        auto stressModule = stressLowerer.lower(
            *stressCompilation.nameLookup(), *stressCompilation.oopMirModel());
        tests.expect(stressModule != nullptr && stressLowerer.error().empty(),
            "all supported assembly operand shapes lower to LLVM: " + stressLowerer.error());
        if (stressModule != nullptr)
        {
            std::string verifyError;
            tests.expect(stressModule->verify(verifyError),
                "LLVM verifies read-write, indirect-memory, aggregate-output, and local-label asm");
            const std::string llvm = stressModule->text();
            tests.expect(llvm.find("+r,~{cc}") != std::string::npos &&
                    llvm.find("=*m,~{memory}") != std::string::npos &&
                    llvm.find("=*m,*m,~{cc},~{memory}") != std::string::npos &&
                    llvm.find("={eax},{ecx}") != std::string::npos &&
                    llvm.find("asm.result") != std::string::npos &&
                    llvm.find("${:uid}") != std::string::npos,
                "IR retains read-write, indirect output, multi-result, and unique local labels");

            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, targetError);
            std::string emitError;
            tests.expect(target != nullptr &&
                    target->configureModuleForTarget(*stressModule, emitError) &&
                    target->emit(
                        *stressModule,
                        stress / "AssemblyStress.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object,
                        emitError),
                "the x64 backend emits every supported assembly shape to one COFF object: " +
                    emitError);
        }
    }

    const auto barriers = root / "barriers";
    writeFile(barriers / "AssemblyBarriers.nsproj",
        "<Project><PropertyGroup><Name>AssemblyBarriers</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(barriers / "Barriers.ns", R"NS(
public class Barriers
{
    public static int Ordered(int* location)
    {
        *location = 1;
        asm { nop } clobber("memory");
        *location = 2;
        return *location;
    }

    public static int Observable()
    {
        asm { nop }
        return 42;
    }
}
)NS");

    ProjectCompilation barrierCompilation;
    std::string barrierError;
    const bool barrierBuilt = barrierCompilation.compileProjectOopMir(
        barriers / "AssemblyBarriers.nsproj", barrierError);
    tests.expect(barrierBuilt && barrierError.empty(),
        "memory-barrier and operand-free observable asm reach MIR");
    if (barrierBuilt)
    {
        nativesharp::llvm_backend::ProjectOopLlvmLowerer barrierLowerer;
        auto barrierModule = barrierLowerer.lower(
            *barrierCompilation.nameLookup(), *barrierCompilation.oopMirModel());
        std::string targetError;
        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
        nativesharp::llvm_backend::LlvmOptimizer optimizer;
        std::string optimizationError;
        tests.expect(barrierModule != nullptr && target != nullptr &&
                optimizer.optimize(
                    *barrierModule,
                    *target,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O2,
                    optimizationError),
            "the O2 pipeline accepts explicit asm optimization barriers: " + optimizationError);
        if (barrierModule != nullptr && target != nullptr && optimizationError.empty())
        {
            const std::string optimized = barrierModule->text();
            const std::string ordered = functionBody(optimized, "Barriers__Ordered");
            const std::string observable = functionBody(optimized, "Barriers__Observable");
            const std::size_t firstStore = ordered.find("store i32 1");
            const std::size_t memoryBarrier = ordered.find("~{memory}");
            const std::size_t secondStore = ordered.find("store i32 2");
            tests.expect(firstStore != std::string::npos &&
                    memoryBarrier != std::string::npos &&
                    secondStore != std::string::npos &&
                    firstStore < memoryBarrier && memoryBarrier < secondStore,
                "O2 retains and orders stores on both sides of an asm memory clobber");
            tests.expect(observable.find("asm sideeffect inteldialect \"nop\"") !=
                    std::string::npos,
                "O2 cannot delete operand-free asm because every block is observable");
        }
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
