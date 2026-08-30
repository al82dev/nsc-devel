#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Passes/MirPass.h"
#include "nativesharp/Passes/OptimizationArena.h"
#include "nativesharp/Passes/OptimizationBuilder.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/PassContext.h"
#include "nativesharp/Passes/PassManager.h"
#include "nativesharp/Passes/PassRequirements.h"
#include "nativesharp/Passes/PassRunResult.h"
#include "nativesharp/Passes/PreservedAnalyses.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace
{
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::passes::MirPass;
using nativesharp::passes::PassContext;
using nativesharp::passes::PassRequirements;
using nativesharp::passes::PassRunResult;
using nativesharp::passes::PreservedAnalyses;

class LyingCfgPass final : public MirPass
{
public:
    std::string_view name() const noexcept override { return "lying-cfg"; }
    PassRequirements requirements() const noexcept override { return PassRequirements::cfg(); }
    PreservedAnalyses preservedAnalyses() const noexcept override
    {
        return PreservedAnalyses::cfgAndDominance();
    }

    PassRunResult run(PassContext& context, std::string& error) override
    {
        error.clear();
        for (auto* function : context.module()->functions())
        {
            if (function == nullptr || !function->defined())
            {
                continue;
            }
            for (auto* block : function->blocks())
            {
                for (MirInstruction* instruction : block->instructions())
                {
                    if (instruction != nullptr && instruction->opcode() == MirOpcode::Branch &&
                        instruction->firstTarget() != instruction->secondTarget())
                    {
                        instruction->setSecondTarget(instruction->firstTarget());
                        return PassRunResult{true, 1};
                    }
                }
            }
        }
        error = "fixture requires a two-target branch";
        return PassRunResult{};
    }
};

class AlwaysChangedPass final : public MirPass
{
public:
    std::string_view name() const noexcept override { return "always-changed"; }
    PassRequirements requirements() const noexcept override { return PassRequirements::none(); }
    PreservedAnalyses preservedAnalyses() const noexcept override { return PreservedAnalyses::all(); }
    PassRunResult run(PassContext&, std::string& error) override
    {
        error.clear();
        return PassRunResult{true, 1};
    }
};
}

int main()
{
    using nativesharp::frontend::FrontendCompilation;
    using nativesharp::passes::OptimizationArena;
    using nativesharp::passes::OptimizationBuilder;
    using nativesharp::passes::OptimizationModule;
    using nativesharp::passes::PassManager;
    using nativesharp::tests::TestContext;
    TestContext tests;

    FrontendCompilation compilation;
    const char* source =
        "int Choose(bool condition) { if (condition) { return 1; } return 2; }"
        "int Main() { return Choose(true); }";
    tests.expect(
        compilation.compileBuffer(std::filesystem::path("pass_manager.ns"), source, false),
        "pass manager fixture builds through stage 16");
    tests.expect(compilation.ssaModule() != nullptr, "source SSA remains available for independent pass pipelines");

    {
        OptimizationArena arena;
        OptimizationBuilder builder(arena);
        OptimizationModule* module = builder.build(compilation.ssaModule());
        PassManager manager(arena);
        tests.expect(!manager.run(module), "empty pass pipeline is rejected");
        tests.expect(manager.error().find("pipeline is empty") != std::string::npos,
            "empty pipeline failure is explicit");
    }

    {
        OptimizationArena arena;
        OptimizationBuilder builder(arena);
        OptimizationModule* module = builder.build(compilation.ssaModule());
        PassManager manager(arena);
        manager.addPass(new LyingCfgPass());
        tests.expect(!manager.run(module), "pass cannot silently preserve stale CFG after terminator rewrite");
        tests.expect(manager.error().find("produced invalid optimized SSA") != std::string::npos,
            "stale-analysis failure is attributed to the mutating pass");
    }

    {
        OptimizationArena arena;
        OptimizationBuilder builder(arena);
        OptimizationModule* module = builder.build(compilation.ssaModule());
        PassManager manager(arena);
        manager.setMaximumIterations(2);
        manager.addPass(new AlwaysChangedPass());
        tests.expect(!manager.run(module), "non-converging pipeline is stopped by iteration bound");
        tests.expect(manager.error().find("did not reach fixed point") != std::string::npos,
            "non-convergence failure reports fixed-point limit");
        tests.expect(module != nullptr && module->iterations() == 2,
            "non-converging pipeline records completed iteration count");
    }

    {
        OptimizationArena arena;
        OptimizationBuilder builder(arena);
        OptimizationModule* module = builder.build(compilation.ssaModule());
        PassManager manager(arena);
        manager.addDefaultPipeline();
        tests.expect(manager.run(module), "default optimization pipeline succeeds on valid SSA");
        tests.expect(
            module != nullptr && !module->records().empty(),
            "pass manager records per-pass execution history");
        tests.expect(module != nullptr && module->cfgBuildCount() >= 1 && module->dominanceBuildCount() >= 1,
            "pass manager materializes verified CFG and dominance analyses");
    }

    return tests.result();
}
