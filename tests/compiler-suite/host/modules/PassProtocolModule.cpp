#include "PassTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

void analysisKindContract(TestContext& context, std::size_t variant)
{
    const auto all = passes::PreservedAnalyses::all();
    context.expect(all.preserves(passes::AnalysisKind::Cfg), "all must preserve CFG");
    context.expect(all.preserves(passes::AnalysisKind::Dominance), "all must preserve dominance");
    if (variant >= 3)
        context.expect(!all.preserves(static_cast<passes::AnalysisKind>(255)), "unknown analysis kind must not be preserved");
}

void requirementsContract(TestContext& context, std::size_t variant)
{
    const auto none = passes::PassRequirements::none();
    const auto cfg = passes::PassRequirements::cfg();
    const auto dominance = passes::PassRequirements::dominance();
    context.expect(!none.needsCfg() && !none.needsDominance(), "none requirement must be empty");
    context.expect(cfg.needsCfg() && !cfg.needsDominance(), "CFG requirement must request only CFG");
    context.expect(dominance.needsCfg() && dominance.needsDominance(), "dominance must imply CFG");
    if (variant == 8)
        for (std::size_t index = 0; index < 4096; ++index)
            context.expect(passes::PassRequirements::dominance().needsCfg(), "requirement factories must be deterministic");
}

void preservationContract(TestContext& context, std::size_t variant)
{
    const auto none = passes::PreservedAnalyses::none();
    const auto both = passes::PreservedAnalyses::cfgAndDominance();
    context.expect(!none.preserves(passes::AnalysisKind::Cfg), "none must invalidate CFG");
    context.expect(!none.preserves(passes::AnalysisKind::Dominance), "none must invalidate dominance");
    context.expect(both.preserves(passes::AnalysisKind::Cfg), "combined preservation must retain CFG");
    context.expect(both.preserves(passes::AnalysisKind::Dominance), "combined preservation must retain dominance");
    if (variant >= 3)
        context.expect(!both.preserves(static_cast<passes::AnalysisKind>(255)), "unknown analysis must not be retained");
}

void resultContract(TestContext& context, std::size_t variant)
{
    const passes::PassRunResult empty{};
    const passes::PassRunResult changed{true, variant * 17};
    context.expect(!empty.changed && empty.transformations == 0, "default pass result must be empty");
    context.expect(changed.changed && changed.transformations == variant * 17, "pass result fields must be exact");
}

void recordContract(TestContext& context, std::size_t variant)
{
    passes::PassRecord record;
    record.iteration = variant;
    record.name = "record-" + std::to_string(variant);
    record.changed = variant % 2 == 0;
    record.transformations = variant * 3;
    record.instructionsBefore = 100;
    record.instructionsAfter = 100 - variant;
    record.blocksBefore = 8;
    record.blocksAfter = 7;
    record.preservedCfg = true;
    record.cfgBuildsAfter = variant;
    context.expect(record.iteration == variant && record.name == "record-" + std::to_string(variant),
        "record identity fields must be exact");
    context.expect(record.instructionsBefore - record.instructionsAfter == variant,
        "record instruction delta must remain observable");
    context.expect(record.preservedCfg && record.cfgBuildsAfter == variant,
        "record analysis fields must be exact");
}

void abstractionContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    passes::PassContext passContext(fixture->module()->module(), *fixture->module()->arena(), analyses);
    ScriptedPass pass("scripted", passes::PassRequirements::none(), passes::PreservedAnalyses::all(),
        {{variant != 3, variant == 3 ? 0U : variant}}, variant == 4);
    std::string error;
    const auto result = pass.run(passContext, error);
    context.expect(pass.name() == "scripted" && pass.calls() == 1, "pass virtual protocol must dispatch exactly once");
    context.expect((variant == 4) == !error.empty(), "pass error channel must be independent from result fields");
    if (variant != 4)
        context.expect(result.transformations == (variant == 3 ? 0U : variant), "pass result must cross abstraction unchanged");
}

void contextContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    passes::PassContext tuple(fixture->module()->module(), *fixture->module()->arena(), analyses);
    context.expect(tuple.module() == fixture->module()->module(), "context must preserve module identity");
    context.expect(&tuple.arena() == fixture->module()->arena(), "context must preserve MIR arena identity");
    context.expect(&tuple.analyses() == &analyses, "context must preserve analysis-manager identity");
}

void arenaContract(TestContext& context, std::size_t variant)
{
    passes::OptimizationArena arena;
    context.expect(&arena.mirArena() == &arena.mirArena(), "MIR arena accessor must be stable");
    context.expect(&arena.cfgArena() == &arena.cfgArena(), "CFG arena accessor must be stable");
    context.expect(&arena.dominanceArena() == &arena.dominanceArena(), "dominance arena accessor must be stable");
    if (variant == 2 || variant == 6)
        context.expect(arena.makeModule(nullptr, arena.mirArena().makeModule()) != nullptr,
            "optimization arena must own each created module");
    if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            context.expect(arena.makeModule(nullptr, arena.mirArena().makeModule()) != nullptr,
                "arena must retain high-volume allocation identity");
}
}

void registerPassProtocolModule(TestRegistry& registry)
{
    addPassContract(registry, 1, "unit", analysisKindContract);
    addPassContract(registry, 2, "unit", requirementsContract);
    addPassContract(registry, 3, "unit", preservationContract);
    addPassContract(registry, 4, "unit", resultContract);
    addPassContract(registry, 5, "unit", recordContract);
    addPassContract(registry, 6, "unit", abstractionContract);
    addPassContract(registry, 7, "unit", contextContract);
    addPassContract(registry, 8, "component", arenaContract, {3, 4, 7, 8}, {"lifetime-instrumentation"});
}
}
