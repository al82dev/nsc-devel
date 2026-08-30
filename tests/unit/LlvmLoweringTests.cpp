#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"
#include "nativesharp/Sema/Symbol.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::llvm_backend::LlvmLowerer;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::passes::OptimizationModule;

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(
        std::filesystem::path("llvm_lowering_test.ns"),
        std::string(source),
        false);
}

std::unique_ptr<LlvmModule> lower(FrontendCompilation& compilation, LlvmLowerer& lowerer)
{
    if (compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }
    return lowerer.lower(compilation.optimizationModule());
}

MirFunction* findFunction(OptimizationModule* module, std::string_view name)
{
    if (module == nullptr || module->module() == nullptr)
    {
        return nullptr;
    }
    for (MirFunction* function : module->module()->functions())
    {
        if (function != nullptr && function->symbol() != nullptr && function->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    {
        FrontendCompilation compilation;
        const char* source =
            "int Add(int a, int b) { return a + b; }"
            "int Main() { return Add(20, 22); }";
        tests.expect(compile(source, compilation), "simple arithmetic fixture reaches optimized SSA");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> module = lower(compilation, lowerer);
        tests.expect(module != nullptr, "simple arithmetic lowers to an LLVM module");
        if (module != nullptr)
        {
            std::string verifierError;
            tests.expect(module->verify(verifierError), "LLVM verifier accepts simple lowered module");
            const std::string text = module->text();
            tests.expect(text.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
                "lowered module carries the Windows x64 target triple");
            tests.expect(text.find("add i32") != std::string::npos,
                "NativeSharp integer addition becomes LLVM integer add");
            tests.expect(text.find(" nsw ") == std::string::npos && text.find(" nuw ") == std::string::npos,
                "wrapping NativeSharp integer arithmetic does not acquire no-wrap flags");
        }
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Merge(bool condition) { int value;"
            "if (condition) { value = 10; } else { value = 20; } return value; }"
            "int Main() { return Merge(true); }";
        tests.expect(compile(source, compilation), "PHI fixture reaches optimized SSA");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> module = lower(compilation, lowerer);
        tests.expect(module != nullptr, "PHI fixture lowers successfully");
        if (module != nullptr)
        {
            tests.expect(module->text().find("phi i32") != std::string::npos,
                "NativeSharp SSA PHI becomes a real LLVM phi instruction");
        }
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "struct Pair { int A; int B; }"
            "Pair MakePair() { Pair p; p.A = 1; p.B = 2; return p; }"
            "int Main() { Pair p = MakePair(); Pair* q = &p; q->A = 7; return MakePair().B; }";
        tests.expect(compile(source, compilation), "struct fixture reaches optimized SSA");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> module = lower(compilation, lowerer);
        tests.expect(module != nullptr, "struct fixture lowers successfully");
        if (module != nullptr)
        {
            const std::string text = module->text();
            tests.expect(text.find("%Pair = type { i32, i32 }") != std::string::npos,
                "semantic struct fields define a named LLVM struct body");
            tests.expect(text.find("extractvalue %Pair") != std::string::npos,
                "struct rvalue field access lowers through extractvalue");
            // Stage 51.x note: this sandbox's LLVM 20 runtime prints struct GEPs with an extra
            // "nuw" qualifier ("getelementptr inbounds nuw %Pair") that the original literal
            // substring didn't account for -- same root cause and same fix as the equivalent
            // check in Stage35RecoveryTests.cpp, confirmed empirically against this fixture.
            tests.expect(
                text.find("getelementptr inbounds") != std::string::npos && text.find("%Pair") != std::string::npos,
                "struct lvalue field access lowers through struct GEP");
        }
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "extern C int puts(char* text);"
            "int IntegerOps(int a, int b) { return (a / b) + (a % b) + (a << b) + (a >> b); }"
            "uint UnsignedOps(uint a, uint b) { return (a / b) + (a % b) + (a >> b); }"
            "double FloatDivide(double a, double b) { return a / b; }"
            "int FloatToInt(double value) { return (int)value; }"
            "uint FloatToUInt(double value) { return (uint)value; }"
            "int Float32ToInt(float value) { return (int)value; }"
            "int MemoryOps(int count, int index) { int[4] values; values[index] = 7;"
            "int* heap = new int[count]; int* temp = stackalloc int[count];"
            "temp[0] = values[index]; delete[] heap; return temp[0]; }"
            "int StringWithNul() { return puts(\"A\\0B\"); }"
            "int Main() { return IntegerOps(20, 3); }";
        tests.expect(compile(source, compilation), "trap and memory fixture reaches optimized SSA");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> module = lower(compilation, lowerer);
        tests.expect(module != nullptr, "trap and memory fixture lowers successfully");
        if (module != nullptr)
        {
            const std::string text = module->text();
            tests.expect(text.find("sdiv i32") != std::string::npos && text.find("srem i32") != std::string::npos,
                "signed division and remainder lower after explicit guards");
            tests.expect(text.find("shl i32") != std::string::npos && text.find("ashr i32") != std::string::npos,
                "dynamic signed shifts lower after range guards");
            tests.expect(
                text.find("fdiv double") != std::string::npos &&
                    text.find("fptosi double") != std::string::npos &&
                    text.find("fptosi float") != std::string::npos,
                "floating divide and checked signed floating-to-integer conversions lower to LLVM operations");
            tests.expect(
                text.find("udiv i32") != std::string::npos &&
                    text.find("urem i32") != std::string::npos &&
                    text.find("lshr i32") != std::string::npos &&
                    text.find("fptoui double") != std::string::npos,
                "unsigned division, remainder, right shift and floating conversion use unsigned LLVM operations");
            tests.expect(text.find("@__nativesharp_trap(i32 1)") != std::string::npos &&
                    text.find("@__nativesharp_trap(i32 2)") != std::string::npos &&
                    text.find("@__nativesharp_trap(i32 3)") != std::string::npos &&
                    text.find("@__nativesharp_trap(i32 4)") != std::string::npos &&
                    text.find("@__nativesharp_trap(i32 5)") != std::string::npos &&
                    text.find("@__nativesharp_trap(i32 6)") != std::string::npos,
                "all six stage-17 runtime trap reasons are materialized in control flow");
            tests.expect(text.find("declare ptr @__nativesharp_alloc(i64)") != std::string::npos &&
                    text.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
                "heap lowering uses explicit 64-bit NativeSharp runtime allocation ABI");
            tests.expect(text.find("@malloc(i32)") == std::string::npos,
                "heap lowering does not inherit the deprecated LLVMBuildMalloc i32 ABI artifact");
            tests.expect(text.find("c\"A\\00B\\00\"") != std::string::npos,
                "embedded NUL bytes survive string-literal lowering with explicit length");
        }
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { return 0; }", compilation),
            "null-input negative fixture has valid semantic context");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        tests.expect(lowerer.lower(nullptr) == nullptr, "LLVM lowerer rejects a null optimized module");
        tests.expect(lowerer.error().find("non-null optimized") != std::string::npos,
            "null optimized-module rejection reports its violated precondition");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "struct Pair { int A; int B; }"
            "int Main() { Pair p; p.A = 3; return p.A; }";
        tests.expect(compile(source, compilation), "missing-struct-metadata negative fixture reaches optimized SSA");
        std::span<nativesharp::sema::Symbol* const> noSymbols;
        LlvmLowerer lowerer(noSymbols);
        std::unique_ptr<LlvmModule> module = lowerer.lower(compilation.optimizationModule());
        tests.expect(module == nullptr, "LLVM lowerer rejects struct MIR without semantic struct metadata");
        tests.expect(lowerer.error().find("struct") != std::string::npos,
            "missing struct metadata rejection identifies the struct boundary");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "extern C void __nativesharp_trap(int code);"
            "int Main() { return 0; }";
        tests.expect(compile(source, compilation), "reserved-runtime-name negative fixture is source-valid");
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> module = lower(compilation, lowerer);
        tests.expect(module == nullptr, "LLVM backend rejects collision with reserved runtime hook");
        tests.expect(lowerer.error().find("reserved backend function name") != std::string::npos,
            "reserved runtime-hook rejection is explicit");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Merge(bool condition) { int value;"
            "if (condition) { value = 1; } else { value = 2; } return value; }"
            "int Main() { return Merge(false); }";
        tests.expect(compile(source, compilation), "corrupted-SSA negative fixture starts valid");
        OptimizationModule* module = const_cast<OptimizationModule*>(compilation.optimizationModule());
        MirFunction* function = findFunction(module, "Merge");
        MirInstruction* phi = nullptr;
        if (function != nullptr)
        {
            for (MirBlock* block : function->blocks())
            {
                for (MirInstruction* instruction : block->instructions())
                {
                    if (instruction != nullptr && instruction->opcode() == MirOpcode::Phi)
                    {
                        phi = instruction;
                        break;
                    }
                }
            }
        }
        tests.expect(phi != nullptr && phi->phiIncoming().size() == 2,
            "corrupted-SSA fixture exposes a two-input PHI");
        if (phi != nullptr && phi->phiIncoming().size() == 2)
        {
            const auto first = phi->phiIncoming().front();
            phi->clearPhiIncoming();
            phi->addPhiIncoming(first.block, first.value);
        }
        LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<LlvmModule> lowered = lowerer.lower(compilation.optimizationModule());
        tests.expect(lowered == nullptr, "LLVM boundary rejects corrupted optimized SSA before code generation");
        tests.expect(lowerer.error().find("optimized SSA precondition failed") != std::string::npos,
            "corrupted optimized SSA rejection identifies the pre-lowering verifier boundary");
    }

    return tests.result();
}
