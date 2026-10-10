#include "PassTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

void optimizedDumpContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    const std::string first = dumpModule(fixture->module());
    const std::string second = dumpModule(fixture->module());
    context.expect(first == second, "optimized module dump must be byte-deterministic");
    context.expect(first.starts_with("optimized.ssa.module\n"), "optimized dump must retain its exact header");
    context.expect(first.find("function @Main") != std::string::npos, "optimized dump must include function identity");
    std::ostringstream nullOutput;
    passes::OptimizationDumper{}.dump(nullptr, nullOutput);
    context.expect(nullOutput.str().empty(), "null optimized module must emit zero bytes");
}

void instructionDumpContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    const std::string output = dumpModule(fixture->module());
    context.expect(output.find("constant") != std::string::npos, "instruction dump must expose constants by opcode");
    context.expect(output.find("%v") != std::string::npos, "instruction dump must expose typed MIR value identities");
    context.expect(output.find("return") != std::string::npos, "instruction dump must expose structural terminators");
    context.expect(output == dumpModule(fixture->module()), "instruction corpus must remain byte-exact across repetition");
}

void pipelineDumpContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, variant);
    passes::PassRecord record;
    record.iteration = 2;
    record.name = "fold";
    record.changed = true;
    record.transformations = 3;
    record.instructionsBefore = 7;
    record.instructionsAfter = 4;
    record.blocksBefore = 2;
    record.blocksAfter = 1;
    record.preservedCfg = true;
    record.preservedDominance = true;
    record.cfgBuildsAfter = 5;
    record.dominanceBuildsAfter = 6;
    fixture->module()->addRecord(record);
    fixture->module()->setIterations(2);
    fixture->module()->setCfgBuildCount(5);
    fixture->module()->setDominanceBuildCount(6);
    const std::string expected =
        "pass.pipeline\n"
        "iterations: 2\n"
        "cfg-builds: 5\n"
        "dominance-builds: 6\n"
        "iteration 2 pass fold changed=yes transforms=3 instructions=7->4 blocks=2->1 "
        "preserves=cfg,dominance cfg-builds=5 dominance-builds=6\n";
    context.expect(dumpPipeline(fixture->module()) == expected, "pipeline dump must match its complete byte contract");
    std::ostringstream nullOutput;
    passes::PassPipelineDumper{}.dump(nullptr, nullOutput);
    context.expect(nullOutput.str().empty(), "null pipeline must emit zero bytes");
}
}

void registerPassDumpModule(TestRegistry& registry)
{
    addPassContract(registry, 49, "unit", optimizedDumpContract);
    addPassContract(registry, 50, "unit", instructionDumpContract, {6, 8}, {"exact-corpus-expansion-pending"});
    addPassContract(registry, 51, "unit", pipelineDumpContract);
}
}
