#include "PassTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

void moduleIdentity(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    auto* module = fixture->module();
    context.expect(module->sourceSsa() == fixture->compilation().ssaModule(), "source SSA identity must be retained");
    context.expect(module->module() != nullptr && module->arena() == &fixture->arena().mirArena(),
        "mutable MIR and owning arena identities must be exact");
}

void attachmentContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    auto* module = fixture->module();
    context.expect(module->cfgModule() == nullptr && module->dominanceModule() == nullptr,
        "analysis attachments must start empty");
    passes::AnalysisManager manager(*module, fixture->arena());
    const auto* cfg = manager.ensureCfg();
    context.expect(cfg != nullptr && module->cfgModule() == cfg, "verified CFG must be published atomically");
    if (variant != 3)
    {
        const auto* dominance = manager.ensureDominance();
        context.expect(dominance != nullptr && module->dominanceModule() == dominance,
            "verified dominance must be published atomically");
    }
}

void telemetryContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::PassRecord first;
    first.iteration = variant;
    first.name = "first";
    first.changed = true;
    passes::PassRecord second;
    second.iteration = variant + 1;
    second.name = "second";
    fixture->module()->addRecord(first);
    fixture->module()->addRecord(second);
    fixture->module()->setIterations(variant);
    fixture->module()->setCfgBuildCount(variant + 2);
    fixture->module()->setDominanceBuildCount(variant + 3);
    const auto& records = fixture->module()->records();
    context.expect(records.size() == 2 && records[0].name == "first" && records[1].name == "second",
        "telemetry must retain insertion order");
    context.expect(fixture->module()->iterations() == variant && fixture->module()->cfgBuildCount() == variant + 2,
        "telemetry counters must round-trip exactly");
}

void cfgCacheContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager manager(*fixture->module(), fixture->arena());
    const auto* first = manager.ensureCfg();
    const auto* second = manager.ensureCfg();
    context.expect(first != nullptr && first == second, "CFG analysis must be cached by identity");
    context.expect(manager.cfgBuildCount() == 1, "cached CFG must build exactly once");
    context.expect(manager.error().empty(), "successful CFG acquisition must retain no diagnostic");
}

void dominanceCacheContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager manager(*fixture->module(), fixture->arena());
    const auto* first = manager.ensureDominance();
    const auto* second = manager.ensureDominance();
    context.expect(first != nullptr && first == second, "dominance analysis must be cached by identity");
    context.expect(manager.cfgBuildCount() == 1 && manager.dominanceBuildCount() == 1,
        "dominance must acquire CFG before one dominance build");
}

void invalidationContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager manager(*fixture->module(), fixture->arena());
    const auto* firstCfg = manager.ensureCfg();
    const auto* firstDom = manager.ensureDominance();
    if (variant == 1 || variant == 7)
        manager.invalidate(passes::PreservedAnalyses::all());
    else if (variant == 2 || variant == 6)
        manager.invalidate(passes::PreservedAnalyses::cfgAndDominance());
    else
        manager.invalidate(passes::PreservedAnalyses::none());
    const auto* secondCfg = manager.ensureCfg();
    const auto* secondDom = manager.ensureDominance();
    const bool preserved = variant == 1 || variant == 2 || variant == 6 || variant == 7;
    context.expect((firstCfg == secondCfg) == preserved, "CFG invalidation transition must match lattice");
    context.expect((firstDom == secondDom) == preserved, "dominance invalidation transition must match lattice");
}

void counterErrorContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::AnalysisManager manager(*fixture->module(), fixture->arena());
    context.expect(manager.cfgBuildCount() == 0 && manager.dominanceBuildCount() == 0,
        "analysis counters must begin at zero");
    context.expect(manager.ensureCfg() != nullptr, "valid CFG analysis must build");
    context.expect(manager.cfgBuildCount() == 1 && manager.error().empty(),
        "successful analysis must increment only its exact counter");
}

void builderEntryContract(TestContext& context, std::size_t variant)
{
    passes::OptimizationArena arena;
    passes::OptimizationBuilder builder(arena);
    context.expect(builder.build(nullptr) == nullptr, "null SSA source must be rejected");
    context.expect(!builder.error().empty(), "null SSA rejection must provide a diagnostic");
    auto fixture = std::make_unique<PassFixture>();
    context.expect(fixture->compile(variant), "valid builder fixture must compile");
    context.expect(builder.build(fixture->compilation().ssaModule()) != nullptr,
        "builder reuse must clear the previous error and clone valid SSA");
    context.expect(builder.error().empty(), "successful builder reuse must clear diagnostic state");
}

void cloningIdentityContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    const auto* source = fixture->compilation().ssaModule()->module();
    const auto* clone = fixture->module()->module();
    context.expect(source != clone, "optimized MIR module must be pointer-independent from SSA MIR");
    context.expect(source->globals().size() == clone->globals().size(), "global order and cardinality must be cloned");
    context.expect(source->functions().size() == clone->functions().size(), "function order and cardinality must be cloned");
}

void functionCloneContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    const auto* source = fixture->compilation().ssaModule()->module();
    const auto* clone = fixture->module()->module();
    context.expect(!source->functions().empty() && source->functions().size() == clone->functions().size(),
        "function universe must clone completely");
    for (std::size_t index = 0; index < source->functions().size(); ++index)
    {
        const auto* left = source->functions()[index];
        const auto* right = clone->functions()[index];
        context.expect(left != right && left->symbol() == right->symbol(), "function clone must preserve symbol, not identity");
        context.expect(left->blocks().size() == right->blocks().size(), "block order must be isomorphic");
    }
}

void instructionCloneContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    const auto* source = fixture->compilation().ssaModule()->module();
    const auto* clone = fixture->module()->module();
    std::size_t compared = 0;
    for (std::size_t f = 0; f < source->functions().size(); ++f)
    {
        const auto* leftFunction = source->functions()[f];
        const auto* rightFunction = clone->functions()[f];
        for (std::size_t b = 0; b < leftFunction->blocks().size(); ++b)
        {
            const auto* leftBlock = leftFunction->blocks()[b];
            const auto* rightBlock = rightFunction->blocks()[b];
            context.expect(leftBlock != rightBlock && leftBlock->instructions().size() == rightBlock->instructions().size(),
                "instruction container must clone independently and preserve cardinality");
            for (std::size_t i = 0; i < leftBlock->instructions().size(); ++i)
            {
                const auto* left = leftBlock->instructions()[i];
                const auto* right = rightBlock->instructions()[i];
                context.expect(left != right && left->opcode() == right->opcode(),
                    "instruction clone must preserve opcode without retaining identity");
                ++compared;
            }
        }
    }
    context.expect(compared != 0, "fixture must exercise at least one instruction clone");
}

void builderLifecycleContract(TestContext& context, std::size_t variant)
{
    passes::OptimizationArena arena;
    passes::OptimizationBuilder builder(arena);
    context.expect(builder.build(nullptr) == nullptr && !builder.error().empty(),
        "builder failure must latch a useful diagnostic");
    frontend::FrontendCompilation compilation;
    context.expect(compilation.compileBuffer("builder-reuse.ns", "int Main() { return 0; }", false),
        "reuse fixture must reach SSA");
    context.expect(builder.build(compilation.ssaModule()) != nullptr && builder.error().empty(),
        "next build must reset maps and error state");
    if (variant == 8)
        for (std::size_t index = 0; index < 128; ++index)
            context.expect(builder.build(compilation.ssaModule()) != nullptr, "builder repeated reuse must remain deterministic");
}
}

void registerPassModelAnalysisModule(TestRegistry& registry)
{
    addPassContract(registry, 9, "unit", moduleIdentity);
    addPassContract(registry, 10, "component", attachmentContract);
    addPassContract(registry, 11, "unit", telemetryContract);
    addPassContract(registry, 12, "component", cfgCacheContract);
    addPassContract(registry, 13, "component", dominanceCacheContract);
    addPassContract(registry, 14, "unit", invalidationContract);
    addPassContract(registry, 15, "unit", counterErrorContract);
    addPassContract(registry, 16, "component", builderEntryContract);
    addPassContract(registry, 17, "component", cloningIdentityContract);
    addPassContract(registry, 18, "component", functionCloneContract);
    addPassContract(registry, 19, "component", instructionCloneContract);
    addPassContract(registry, 20, "unit", builderLifecycleContract);
}
}
