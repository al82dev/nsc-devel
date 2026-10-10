#include "PassTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

mir::MirBlock* firstDefinedBlock(passes::OptimizationModule* module)
{
    if (module == nullptr || module->module() == nullptr) return nullptr;
    for (mir::MirFunction* function : module->module()->functions())
        if (function != nullptr && function->defined() && !function->blocks().empty()) return function->blocks().front();
    return nullptr;
}

sema::Type* representativeType(passes::OptimizationModule* module)
{
    if (module == nullptr || module->arena() == nullptr || module->arena()->values().empty()) return nullptr;
    return module->arena()->values().front()->type();
}

void entryModesContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::OptimizationVerifier verifier;
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    context.expect(verifier.verify(fixture->module(), analyses), "manager entry must acquire and verify both analyses");
    context.expect(verifier.error().empty(), "successful manager verification must clear diagnostics");
    context.expect(verifier.verify(static_cast<const passes::OptimizationModule*>(fixture->module())),
        "const entry must accept the cached verified analyses");
}

void structuralContract(TestContext& context, std::size_t variant)
{
    passes::OptimizationArena arena;
    auto* missing = arena.makeModule(nullptr, nullptr);
    passes::OptimizationVerifier verifier;
    context.expect(!verifier.verify(static_cast<const passes::OptimizationModule*>(missing)),
        "optimization module without MIR must be rejected");
    context.expect(!verifier.error().empty(), "structural rejection must identify its first invariant");
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    context.expect(verifier.verify(fixture->module(), analyses), "verifier reuse must accept a complete function universe");
    context.expect(verifier.error().empty(), "successful reuse must clear structural diagnostic");
}

void ordinaryUseContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    mir::MirBlock* block = firstDefinedBlock(fixture->module());
    sema::Type* type = representativeType(fixture->module());
    context.expect(block != nullptr && type != nullptr, "ordinary-use fixture must expose a typed defined block");
    if (block == nullptr || type == nullptr) return;
    auto* undefined = fixture->module()->arena()->makeValue(type);
    auto* result = fixture->module()->arena()->makeValue(type);
    auto* invalid = fixture->module()->arena()->makeInstruction(mir::MirOpcode::Copy, {});
    invalid->setResult(result);
    invalid->addOperand(undefined);
    std::vector<mir::MirInstruction*> instructions = block->instructions();
    instructions.insert(instructions.begin(), invalid);
    block->replaceInstructions(std::move(instructions));
    passes::OptimizationVerifier verifier;
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    context.expect(!verifier.verify(fixture->module(), analyses), "ordinary use without a definition must be rejected");
    context.expect(!verifier.error().empty(), "undefined ordinary use must retain a diagnostic");
}

void phiDominanceContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    mir::MirBlock* block = firstDefinedBlock(fixture->module());
    sema::Type* type = representativeType(fixture->module());
    context.expect(block != nullptr && type != nullptr, "PHI fixture must expose a typed entry block");
    if (block == nullptr || type == nullptr) return;
    auto* undefined = fixture->module()->arena()->makeValue(type);
    auto* result = fixture->module()->arena()->makeValue(type);
    auto* phi = fixture->module()->arena()->makeInstruction(mir::MirOpcode::Phi, {});
    phi->setResult(result);
    phi->addPhiIncoming(block, undefined);
    std::vector<mir::MirInstruction*> instructions = block->instructions();
    instructions.insert(instructions.begin(), phi);
    block->replaceInstructions(std::move(instructions));
    passes::OptimizationVerifier verifier;
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    context.expect(!verifier.verify(fixture->module(), analyses),
        "PHI incoming from a non-predecessor with undefined value must be rejected");
    context.expect(!verifier.error().empty(), "PHI predecessor or dominance failure must preserve first diagnostic");
}

void resetContract(TestContext& context, std::size_t variant)
{
    passes::OptimizationVerifier verifier;
    auto valid = makeFixture(context, variant);
    passes::AnalysisManager validAnalyses(*valid->module(), valid->arena());
    context.expect(verifier.verify(valid->module(), validAnalyses) && verifier.error().empty(),
        "first valid verification must succeed");
    passes::OptimizationArena invalidArena;
    auto* invalid = invalidArena.makeModule(nullptr, nullptr);
    context.expect(!verifier.verify(static_cast<const passes::OptimizationModule*>(invalid)) && !verifier.error().empty(),
        "corrupt module must set a diagnostic");
    auto second = makeFixture(context, variant);
    passes::AnalysisManager secondAnalyses(*second->module(), second->arena());
    context.expect(verifier.verify(second->module(), secondAnalyses) && verifier.error().empty(),
        "later valid call must clear prior definitions and diagnostic");
}
}

void registerPassVerifierModule(TestRegistry& registry)
{
    addPassContract(registry, 52, "component", entryModesContract);
    addPassContract(registry, 53, "component", structuralContract);
    addPassContract(registry, 54, "component", ordinaryUseContract);
    addPassContract(registry, 55, "component", phiDominanceContract);
    addPassContract(registry, 56, "unit", resetContract);
}
}
