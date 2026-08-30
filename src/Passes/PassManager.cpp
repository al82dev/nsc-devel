#include "nativesharp/Passes/PassManager.h"

#include "nativesharp/Passes/AnalysisManager.h"
#include "nativesharp/Passes/ConstantPropagationPass.h"
#include "nativesharp/Passes/CopyPropagationPass.h"
#include "nativesharp/Passes/DeadCodeEliminationPass.h"
#include "nativesharp/Passes/MirPass.h"
#include "nativesharp/Passes/MirRewriter.h"
#include "nativesharp/Passes/OptimizationArena.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Passes/PassContext.h"
#include "nativesharp/Passes/PhiSimplificationPass.h"
#include "nativesharp/Passes/SimplifyControlFlowPass.h"

#include <utility>

namespace nativesharp::passes
{
PassManager::PassManager(OptimizationArena& arena) : arena_(arena) {}

PassManager::~PassManager()
{
    for (MirPass* pass : passes_)
    {
        delete pass;
    }
}

void PassManager::addPass(MirPass* pass)
{
    if (pass != nullptr)
    {
        passes_.push_back(pass);
    }
}

void PassManager::addDefaultPipeline()
{
    addPass(new PhiSimplificationPass());
    addPass(new CopyPropagationPass());
    addPass(new ConstantPropagationPass());
    addPass(new DeadCodeEliminationPass());
    addPass(new SimplifyControlFlowPass());
}

void PassManager::setMaximumIterations(std::size_t value) noexcept
{
    maximumIterations_ = value == 0 ? 1 : value;
}

bool PassManager::run(OptimizationModule* module)
{
    error_.clear();
    if (module == nullptr || module->module() == nullptr ||
        module->arena() == nullptr)
    {
        return fail("pass manager requires a mutable optimization module");
    }
    if (passes_.empty())
    {
        return fail("pass manager pipeline is empty");
    }

    AnalysisManager analyses(*module, arena_);
    OptimizationVerifier verifier;
    if (!verifier.verify(module, analyses))
    {
        return fail("initial optimized SSA verification failed: " +
            verifier.error());
    }

    bool converged = false;
    std::size_t completedIterations = 0;
    for (std::size_t iteration = 1;
         iteration <= maximumIterations_;
         ++iteration)
    {
        bool iterationChanged = false;

        for (MirPass* pass : passes_)
        {
            if (pass == nullptr)
            {
                return fail("pass manager pipeline contains null pass");
            }

            const PassRequirements requirements = pass->requirements();
            if (requirements.needsDominance())
            {
                if (analyses.ensureDominance() == nullptr)
                {
                    return fail("pass '" + std::string(pass->name()) +
                        "' cannot obtain dominance: " + analyses.error());
                }
            }
            else if (requirements.needsCfg())
            {
                if (analyses.ensureCfg() == nullptr)
                {
                    return fail("pass '" + std::string(pass->name()) +
                        "' cannot obtain CFG: " + analyses.error());
                }
            }

            PassRecord record;
            record.iteration = iteration;
            record.name = std::string(pass->name());
            record.instructionsBefore =
                MirRewriter::instructionCount(module->module());
            record.blocksBefore =
                MirRewriter::blockCount(module->module());

            PassContext context(
                module->module(),
                *module->arena(),
                analyses);
            std::string passError;
            const PassRunResult result =
                pass->run(context, passError);

            if (!passError.empty())
            {
                return fail("pass '" + std::string(pass->name()) +
                    "' failed: " + passError);
            }

            record.changed = result.changed;
            record.transformations = result.transformations;
            record.instructionsAfter =
                MirRewriter::instructionCount(module->module());
            record.blocksAfter =
                MirRewriter::blockCount(module->module());

            const PreservedAnalyses preserved =
                pass->preservedAnalyses();
            record.preservedCfg =
                preserved.preserves(AnalysisKind::Cfg);
            record.preservedDominance =
                preserved.preserves(AnalysisKind::Dominance);

            if (result.changed)
            {
                iterationChanged = true;
                analyses.invalidate(preserved);

                if (!verifier.verify(module, analyses))
                {
                    return fail("pass '" + std::string(pass->name()) +
                        "' produced invalid optimized SSA: " +
                        verifier.error());
                }
            }

            record.cfgBuildsAfter = analyses.cfgBuildCount();
            record.dominanceBuildsAfter =
                analyses.dominanceBuildCount();
            module->addRecord(std::move(record));
        }

        completedIterations = iteration;
        if (!iterationChanged)
        {
            converged = true;
            break;
        }
    }

    module->setIterations(completedIterations);
    module->setCfgBuildCount(analyses.cfgBuildCount());
    module->setDominanceBuildCount(
        analyses.dominanceBuildCount());

    if (!converged)
    {
        return fail(
            "pass pipeline did not reach fixed point within maximum iterations");
    }

    if (!verifier.verify(module, analyses))
    {
        return fail("final optimized SSA verification failed: " +
            verifier.error());
    }

    module->setCfgBuildCount(analyses.cfgBuildCount());
    module->setDominanceBuildCount(
        analyses.dominanceBuildCount());
    return true;
}

const std::string& PassManager::error() const noexcept { return error_; }

bool PassManager::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
}
