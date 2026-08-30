#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Passes/MirEffectClassifier.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/Symbol.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::passes::OptimizationModule;

const MirFunction* findFunction(const OptimizationModule* module, std::string_view name)
{
    if (module == nullptr || module->module() == nullptr)
    {
        return nullptr;
    }
    for (const MirFunction* function : module->module()->functions())
    {
        if (function != nullptr && function->symbol() != nullptr && function->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

std::size_t countOpcode(const MirFunction* function, MirOpcode opcode)
{
    std::size_t result = 0;
    if (function == nullptr)
    {
        return result;
    }
    for (const auto* block : function->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode)
            {
                ++result;
            }
        }
    }
    return result;
}
}

int main()
{
    using nativesharp::passes::MirEffectClassifier;
    using nativesharp::tests::TestContext;
    TestContext tests;

    const char* source =
        "int Trace = 0;"
        "int Touch() { Trace += 1; return 7; }"
        "int FoldAndBranch() {"
        " int a = 1; int b = 2; int sum = a + b; int dead = sum * 10;"
        " if (sum == 3) { return sum + 39; } else { return dead; }"
        "}"
        "int KeepIntegerTrap(int divisor) { int trapped = 10 / divisor; return 0; }"
        "int KeepBoundsCheck(int index) { int[4] values; int value = values[index]; return 0; }"
        "int KeepCall() { int ignored = Touch(); return 0; }"
        "int Main() { return FoldAndBranch(); }";

    FrontendCompilation compilation;
    tests.expect(
        compilation.compileBuffer(std::filesystem::path("optimization_passes.ns"), source, false),
        "optimization corpus builds through default pass pipeline");
    const OptimizationModule* optimized = compilation.optimizationModule();
    tests.expect(optimized != nullptr, "frontend exposes optimized SSA module");
    tests.expect(optimized != nullptr && optimized->iterations() == 3,
        "default pipeline reaches fixed point after multiple iterations on optimization corpus");
    tests.expect(optimized != nullptr && optimized->cfgBuildCount() >= 2,
        "control-flow rewrite invalidates and rebuilds CFG");
    tests.expect(optimized != nullptr && optimized->dominanceBuildCount() >= 2,
        "control-flow rewrite invalidates and rebuilds dominance");

    const MirFunction* folded = findFunction(optimized, "FoldAndBranch");
    tests.expect(folded != nullptr, "FoldAndBranch survives optimization");
    tests.expect(countOpcode(folded, MirOpcode::Branch) == 0,
        "constant branch is simplified to direct control flow");
    tests.expect(countOpcode(folded, MirOpcode::Multiply) == 0,
        "dead pure multiplication is eliminated");
    tests.expect(countOpcode(folded, MirOpcode::Add) == 0,
        "constant arithmetic in FoldAndBranch is folded");
    tests.expect(countOpcode(folded, MirOpcode::Return) == 1,
        "unreachable else block is removed after branch simplification");

    const MirFunction* trap = findFunction(optimized, "KeepIntegerTrap");
    tests.expect(countOpcode(trap, MirOpcode::Divide) == 1,
        "unused integer division is retained because it may trap");

    const MirFunction* bounds = findFunction(optimized, "KeepBoundsCheck");
    tests.expect(countOpcode(bounds, MirOpcode::BoundsCheck) == 1,
        "unused indexed read retains mandatory bounds check");
    tests.expect(countOpcode(bounds, MirOpcode::Load) == 1,
        "unused memory load is conservatively retained");

    const MirFunction* call = findFunction(optimized, "KeepCall");
    tests.expect(countOpcode(call, MirOpcode::Call) == 1,
        "unused call result does not allow removal of side-effecting call");

    const MirInstruction* divide = nullptr;
    if (trap != nullptr)
    {
        for (const auto* block : trap->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->opcode() == MirOpcode::Divide)
                {
                    divide = instruction;
                }
            }
        }
    }
    tests.expect(divide != nullptr && MirEffectClassifier::classify(divide).mayTrap,
        "central effect classifier marks integer division as potentially trapping");
    tests.expect(divide != nullptr && !MirEffectClassifier::removableWhenUnused(divide),
        "central effect classifier forbids DCE of potentially trapping division");

    const char* phiSource =
        "int SameConstant(bool condition) { int value;"
        " if (condition) { value = 5; } else { value = 5; } return value; }"
        "int SameValue(bool condition) { int value = 5;"
        " if (condition) { value = value; } else { value = value; } return value; }"
        "int Main() { return SameConstant(true) + SameValue(false); }";
    FrontendCompilation phiCompilation;
    tests.expect(
        phiCompilation.compileBuffer(std::filesystem::path("phi_passes.ns"), phiSource, false),
        "PHI/copy fixture builds through optimization pipeline");
    const OptimizationModule* phiOptimized = phiCompilation.optimizationModule();
    bool phiChanged = false;
    bool copyChanged = false;
    if (phiOptimized != nullptr)
    {
        for (const auto& record : phiOptimized->records())
        {
            if (record.name == "phi-simplification" && record.changed && record.transformations >= 2)
            {
                phiChanged = true;
            }
            if (record.name == "copy-propagation" && record.changed && record.transformations >= 1)
            {
                copyChanged = true;
            }
        }
    }
    tests.expect(phiChanged, "PHI simplification removes equal-value/equal-constant merge redundancy");
    tests.expect(copyChanged, "copy propagation removes copy introduced by trivial PHI simplification");
    tests.expect(countOpcode(findFunction(phiOptimized, "SameConstant"), MirOpcode::Phi) == 0,
        "equal-constant PHI is absent from final optimized SSA");
    tests.expect(countOpcode(findFunction(phiOptimized, "SameValue"), MirOpcode::Phi) == 0,
        "same-value PHI is absent from final optimized SSA");
    tests.expect(countOpcode(findFunction(phiOptimized, "SameValue"), MirOpcode::Copy) == 0,
        "copy propagation removes temporary copy from final optimized SSA");

    return tests.result();
}
