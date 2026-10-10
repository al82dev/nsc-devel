#include "PassTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

class CountingPass final : public passes::MirPass
{
public:
    CountingPass(std::size_t* calls, std::size_t* destructions, passes::PassRequirements requirements,
        std::vector<passes::PassRunResult> script, bool fail = false)
        : calls_(calls), destructions_(destructions), requirements_(requirements), script_(std::move(script)), fail_(fail) {}
    ~CountingPass() override { ++*destructions_; }
    std::string_view name() const noexcept override { return "counting"; }
    passes::PassRequirements requirements() const noexcept override { return requirements_; }
    passes::PreservedAnalyses preservedAnalyses() const noexcept override { return passes::PreservedAnalyses::all(); }
    passes::PassRunResult run(passes::PassContext&, std::string& error) override
    {
        const std::size_t index = (*calls_)++;
        if (fail_)
        {
            error = "counting pass failure";
            return {};
        }
        error.clear();
        return index < script_.size() ? script_[index] : passes::PassRunResult{};
    }
private:
    std::size_t* calls_;
    std::size_t* destructions_;
    passes::PassRequirements requirements_;
    std::vector<passes::PassRunResult> script_;
    bool fail_;
};

void ownershipPipelineContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    std::size_t calls = 0;
    std::size_t destructions = 0;
    {
        passes::PassManager manager(fixture->arena());
        manager.addPass(nullptr);
        manager.addPass(new CountingPass(&calls, &destructions, passes::PassRequirements::none(), {{false, 0}}));
        context.expect(manager.run(fixture->module()), "owned mock pass pipeline must complete");
        context.expect(calls == 1 && destructions == 0, "manager must retain pass ownership through execution");
    }
    context.expect(destructions == 1, "manager destruction must destroy each owned pass exactly once");
    auto defaultFixture = makeFixture(context, variant);
    passes::PassManager defaults(defaultFixture->arena());
    defaults.addDefaultPipeline();
    context.expect(defaults.run(defaultFixture->module()), "default pipeline must execute on valid SSA");
    const std::vector<std::string> expected = {"phi-simplification", "copy-propagation", "constant-propagation",
        "dead-code-elimination", "simplify-control-flow"};
    const auto& records = defaultFixture->module()->records();
    context.expect(records.size() >= expected.size(), "default pipeline must record all five ordered passes");
    for (std::size_t index = 0; index < expected.size() && index < records.size(); ++index)
        context.expect(records[index].name == expected[index], "default pass order must remain contractual");
}

void requirementsAcquisitionContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    std::size_t calls = 0;
    std::size_t destructions = 0;
    passes::PassManager manager(fixture->arena());
    manager.addPass(new CountingPass(&calls, &destructions, passes::PassRequirements::dominance(), {{false, 0}}));
    context.expect(manager.run(fixture->module()), "dominance-requiring pass must acquire analyses before run");
    context.expect(calls == 1 && fixture->module()->cfgBuildCount() == 1 &&
        fixture->module()->dominanceBuildCount() == 1, "dominance requirement must build CFG then dominance once");
}

void fixedPointContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    std::size_t calls = 0;
    std::size_t destructions = 0;
    passes::PassManager manager(fixture->arena());
    manager.setMaximumIterations(variant == 8 ? 64 : 8);
    manager.addPass(new CountingPass(&calls, &destructions, passes::PassRequirements::none(),
        {{true, 1}, {true, 1}, {false, 0}}));
    context.expect(manager.run(fixture->module()), "scripted pipeline must stop on first unchanged iteration");
    context.expect(calls == 3 && fixture->module()->iterations() == 3,
        "fixed-point iteration count must include the closing unchanged iteration");
}

void telemetryVerificationContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    std::size_t calls = 0;
    std::size_t destructions = 0;
    passes::PassManager manager(fixture->arena());
    manager.addPass(new CountingPass(&calls, &destructions, passes::PassRequirements::cfg(), {{false, 0}}));
    context.expect(manager.run(fixture->module()), "verified no-change pass must succeed");
    const auto& records = fixture->module()->records();
    context.expect(records.size() == 1 && records[0].name == "counting" && !records[0].changed,
        "per-pass record must match the observed pass result");
    context.expect(records[0].instructionsBefore == records[0].instructionsAfter,
        "unchanged pass must retain instruction count");
    context.expect(records[0].cfgBuildsAfter == 1, "record must capture analysis counter after verification");
}

void managerErrorContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::PassManager manager(fixture->arena());
    context.expect(!manager.run(fixture->module()) && manager.error().find("pipeline is empty") != std::string::npos,
        "empty pipeline must fail with its stable diagnostic");
    std::size_t calls = 0;
    std::size_t destructions = 0;
    manager.addPass(new CountingPass(&calls, &destructions, passes::PassRequirements::none(), {{false, 0}}));
    context.expect(manager.run(fixture->module()) && manager.error().empty(),
        "manager reuse must clear the preceding error");
    if (variant == 8)
    {
        auto nonConvergent = makeFixture(context, variant);
        passes::PassManager bounded(nonConvergent->arena());
        bounded.setMaximumIterations(0);
        std::size_t repeatedCalls = 0;
        std::size_t repeatedDestructions = 0;
        bounded.addPass(new CountingPass(&repeatedCalls, &repeatedDestructions,
            passes::PassRequirements::none(), {{true, 1}}));
        context.expect(!bounded.run(nonConvergent->module()) && repeatedCalls == 1,
            "zero iteration limit must normalize to one and report non-convergence");
    }
}
}

void registerPassManagerModule(TestRegistry& registry)
{
    addPassContract(registry, 44, "component", ownershipPipelineContract);
    addPassContract(registry, 45, "component", requirementsAcquisitionContract);
    addPassContract(registry, 46, "component", fixedPointContract);
    addPassContract(registry, 47, "component", telemetryVerificationContract);
    addPassContract(registry, 48, "unit", managerErrorContract);
}
}
