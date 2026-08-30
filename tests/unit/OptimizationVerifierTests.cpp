#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Sema/Symbol.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::passes::OptimizationModule;

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

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("optimization_verifier.ns"), std::string(source), false);
}
}

int main()
{
    using nativesharp::passes::OptimizationVerifier;
    using nativesharp::tests::TestContext;
    TestContext tests;

    const char* mergeSource =
        "int Merge(bool condition) { int value;"
        " if (condition) { value = 1; } else { value = 2; }"
        " return value; } int Main() { return Merge(true); }";

    {
        FrontendCompilation compilation;
        tests.expect(compile(mergeSource, compilation), "valid merge fixture builds optimized SSA");
        OptimizationVerifier verifier;
        tests.expect(verifier.verify(compilation.optimizationModule()),
            "standalone optimization verifier accepts generated optimized SSA");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(mergeSource, compilation), "PHI corruption fixture starts valid");
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
            "merge fixture contains two-input PHI before corruption");
        if (phi != nullptr && phi->phiIncoming().size() == 2)
        {
            const auto first = phi->phiIncoming()[0];
            phi->clearPhiIncoming();
            phi->addPhiIncoming(first.block, first.value);
        }
        OptimizationVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects PHI missing a predecessor incoming value");
        tests.expect(verifier.error().find("PHI") != std::string::npos,
            "PHI corruption failure identifies PHI invariant");
    }

    {
        FrontendCompilation compilation;
        const char* branchSource =
            "int Choose(bool condition) { if (condition) { return 1; } return 2; }"
            "int Main() { return Choose(true); }";
        tests.expect(compile(branchSource, compilation), "stale CFG fixture starts valid");
        OptimizationModule* module = const_cast<OptimizationModule*>(compilation.optimizationModule());
        MirFunction* function = findFunction(module, "Choose");
        MirInstruction* branch = nullptr;
        if (function != nullptr)
        {
            for (MirBlock* block : function->blocks())
            {
                for (MirInstruction* instruction : block->instructions())
                {
                    if (instruction != nullptr && instruction->opcode() == MirOpcode::Branch)
                    {
                        branch = instruction;
                    }
                }
            }
        }
        tests.expect(branch != nullptr && branch->firstTarget() != branch->secondTarget(),
            "stale CFG fixture contains dynamic two-target branch");
        if (branch != nullptr)
        {
            branch->setSecondTarget(branch->firstTarget());
        }
        OptimizationVerifier verifier;
        tests.expect(!verifier.verify(module), "standalone verifier rejects cached CFG after terminator mutation");
        tests.expect(verifier.error().find("CFG") != std::string::npos,
            "stale CFG failure identifies cached control-flow analysis");
    }

    return tests.result();
}
