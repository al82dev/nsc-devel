#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::llvm_backend::LlvmLowerer;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::LlvmOptimizationLevel;
using nativesharp::llvm_backend::LlvmOptimizer;
using nativesharp::llvm_backend::LlvmTargetMachine;

std::unique_ptr<LlvmModule> compileAndLower(
    std::string_view source,
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    if (!compilation.compileBuffer(
            std::filesystem::path("llvm_optimization_test.ns"),
            std::string(source),
            false))
    {
        return nullptr;
    }
    if (compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }

    lowerer = std::make_unique<LlvmLowerer>(compilation.semanticAnalyzer()->globalSymbols());
    return lowerer->lower(compilation.optimizationModule());
}

std::unique_ptr<LlvmModule> makeInliningFixture(
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    return compileAndLower(
        "int Square(int x) { return x * x; } "
        "int Main() { return Square(6) + Square(2); }",
        compilation,
        lowerer);
}
}

int main()
{
    using nativesharp::llvm_backend::llvmOptimizationLevelName;
    using nativesharp::llvm_backend::llvmOptimizationPipelineName;
    using nativesharp::tests::TestContext;

    TestContext tests;

    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O0) == "O0", "O0 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O1) == "O1", "O1 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O2) == "O2", "O2 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O3) == "O3", "O3 has a stable display name");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O0) == "default<O0>",
        "O0 selects the LLVM default O0 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O1) == "default<O1>",
        "O1 selects the LLVM default O1 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O2) == "default<O2>",
        "O2 selects the LLVM default O2 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O3) == "default<O3>",
        "O3 selects the LLVM default O3 pipeline");

    constexpr std::array<LlvmOptimizationLevel, 4> levels = {
        LlvmOptimizationLevel::O0,
        LlvmOptimizationLevel::O1,
        LlvmOptimizationLevel::O2,
        LlvmOptimizationLevel::O3};

    LlvmOptimizer optimizer;
    for (const LlvmOptimizationLevel level : levels)
    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine = LlvmTargetMachine::createWindowsX64(level, targetError);
        tests.expect(targetMachine != nullptr, "each supported level creates a Windows x64 TargetMachine");
        if (targetMachine == nullptr)
        {
            continue;
        }
        tests.expect(targetMachine->optimizationLevel() == level,
            "TargetMachine retains the requested optimization level");

        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        tests.expect(module != nullptr, "optimization fixture lowers for every supported level");
        if (module == nullptr)
        {
            continue;
        }

        std::string optimizationError;
        tests.expect(optimizer.optimize(*module, *targetMachine, level, optimizationError),
            "LLVM New Pass Manager accepts every supported default pipeline");
        tests.expect(optimizationError.empty(), "successful LLVM optimization keeps its diagnostic empty");

        std::string verificationError;
        tests.expect(module->verify(verificationError), "optimized module passes LLVM verification");
    }

    {
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        tests.expect(module != nullptr, "raw comparison fixture lowers successfully");
        if (module != nullptr)
        {
            const std::string raw = module->text();
            tests.expect(raw.find("call i32 @Square(i32 6)") != std::string::npos &&
                    raw.find("call i32 @Square(i32 2)") != std::string::npos,
                "raw LLVM boundary still exposes both calls before LLVM optimization");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O0, optimizationError),
                "O0 optimization succeeds for the comparison fixture");
            const std::string text = module->text();
            tests.expect(text.find("call i32 @Square(i32 6)") != std::string::npos,
                "O0 keeps the non-forced Square call visible");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 optimization succeeds for the comparison fixture");
            const std::string text = module->text();
            tests.expect(text.find("call i32 @Square") == std::string::npos,
                "O2 removes the constant call sites from Main");
            tests.expect(text.find("ret i32 40") != std::string::npos,
                "O2 folds the inlined expression to the expected constant return");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = compileAndLower(
            "int Divide(int a, int b) { return a / b; }",
            compilation,
            lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 accepts a function containing NativeSharp dynamic trap guards");
            const std::string text = module->text();
            tests.expect(text.find("__nativesharp_trap") != std::string::npos,
                "O2 preserves a trap path when the divide precondition is dynamic");
            tests.expect(text.find("sdiv i32") != std::string::npos,
                "O2 preserves the actual signed division after its guard");
        }
    }

    {
        constexpr std::array<LlvmOptimizationLevel, 2> aggressiveLevels = {
            LlvmOptimizationLevel::O2,
            LlvmOptimizationLevel::O3};
        for (const LlvmOptimizationLevel level : aggressiveLevels)
        {
            std::string targetError;
            std::unique_ptr<LlvmTargetMachine> targetMachine =
                LlvmTargetMachine::createWindowsX64(level, targetError);
            FrontendCompilation compilation;
            std::unique_ptr<LlvmLowerer> lowerer;
            std::unique_ptr<LlvmModule> module = compileAndLower(
                "int Guarded(int value, int shift, double floating) "
                "{ return (value / shift) + (value << shift) + (int)floating; }",
                compilation,
                lowerer);
            tests.expect(module != nullptr, "semantic-guard fixture lowers successfully");
            if (targetMachine == nullptr || module == nullptr)
            {
                continue;
            }

            std::string optimizationError;
            tests.expect(optimizer.optimize(*module, *targetMachine, level, optimizationError),
                "aggressive LLVM optimization accepts multiple NativeSharp trap guards");
            const std::string text = module->text();
            tests.expect(text.find("__nativesharp_trap(i32 1)") != std::string::npos,
                "aggressive LLVM optimization preserves the division-by-zero trap");
            tests.expect(text.find("__nativesharp_trap(i32 2)") != std::string::npos,
                "aggressive LLVM optimization preserves the signed-division-overflow trap");
            tests.expect(text.find("__nativesharp_trap(i32 3)") != std::string::npos,
                "aggressive LLVM optimization preserves the invalid-shift-count trap");
            tests.expect(text.find("__nativesharp_trap(i32 5)") != std::string::npos,
                "aggressive LLVM optimization preserves the floating-to-integer-range trap");
            tests.expect(text.find("sdiv i32") != std::string::npos,
                "aggressive LLVM optimization retains the guarded signed division operation");
            tests.expect(text.find("shl i32") != std::string::npos,
                "aggressive LLVM optimization retains the guarded shift operation");
            tests.expect(text.find("fptosi") != std::string::npos,
                "aggressive LLVM optimization retains the guarded floating-to-integer conversion");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = compileAndLower(
            "int Add(int left, int right) { return left + right; }",
            compilation,
            lowerer);
        tests.expect(module != nullptr, "signed-wrap fixture lowers successfully");
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 accepts NativeSharp signed wrapping arithmetic");
            const std::string text = module->text();
            tests.expect(text.find("add i32") != std::string::npos,
                "O2 retains the signed wrapping add operation");
            tests.expect(text.find("add nsw i32") == std::string::npos,
                "O2 does not invent a global no-signed-wrap contract for dynamic signed addition");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(!optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "optimizer rejects a pass level that disagrees with its TargetMachine");
            tests.expect(optimizationError.find("does not match") != std::string::npos,
                "optimization-level mismatch reports a precise diagnostic");
            std::string verificationError;
            tests.expect(module->verify(verificationError),
                "level-mismatch rejection leaves the LLVM module valid");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            const auto invalid = static_cast<LlvmOptimizationLevel>(99);
            tests.expect(!optimizer.optimize(*module, *targetMachine, invalid, optimizationError),
                "optimizer rejects an unsupported optimization-level enum value");
            tests.expect(optimizationError.find("unsupported") != std::string::npos,
                "unsupported optimization level reports a precise diagnostic");
            std::string verificationError;
            tests.expect(module->verify(verificationError),
                "rejected optimization request leaves the input module valid");
        }
    }

    return tests.result();
}
