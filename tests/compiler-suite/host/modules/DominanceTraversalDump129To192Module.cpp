#include "DominanceTestSupport.h"

#include <set>

namespace nsc::suite
{
namespace
{
using namespace dominance_support;

void traversalNumberingContract(TestContext& context, std::size_t variant)
{
    if (variant == 3)
    {
        dominance::DominanceArena arena;
        dominance::DominanceBuilder builder(arena);
        builder.numberTree(nullptr);
        GraphFixture fixture;
        FunctionGraph graph = makeSingle(fixture);
        dominance::DominatorTree* tree = arena.makeTree(graph.function);
        builder.numberTree(tree);
        context.expect(tree->root() == nullptr,
            "TF-DOM-131 null and rootless trees are stable no-op numbering inputs");
        return;
    }
    if (variant == 4)
    {
        context.expect(false,
            "TF-DOM-132 TEST-INFRASTRUCTURE: cyclic child traversal requires process isolation");
        return;
    }
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 4096) :
        (variant == 6 ? makeMultipleJoins(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-017 producer must build traversal fixture");
    if (function == nullptr)
    {
        return;
    }
    std::set<std::uint32_t> numbers;
    bool exact = true;
    for (const dominance::DominatorTreeNode* node : function->dominatorTree()->nodes())
    {
        exact = exact && node->preorderNumber() < node->postorderNumber() &&
            numbers.insert(node->preorderNumber()).second &&
            numbers.insert(node->postorderNumber()).second;
        const dominance::DominatorTreeNode* parent = node->immediateDominator();
        if (parent != nullptr)
        {
            exact = exact && parent->preorderNumber() <= node->preorderNumber() &&
                parent->postorderNumber() >= node->postorderNumber();
        }
    }
    if (variant == 5)
    {
        GraphFixture singleFixture;
        FunctionGraph single = makeSingle(singleFixture);
        dominance::DominanceFunction* singleAnalysis = buildAnalysis(
            context, singleFixture, single, "TF-DOM-133 single-node numbering fixture must build");
        exact = exact && singleAnalysis != nullptr &&
            singleAnalysis->dominatorTree()->root()->preorderNumber() == 0U &&
            singleAnalysis->dominatorTree()->root()->postorderNumber() == 1U;
    }
    if (variant == 7)
    {
        const std::string first = structuralSignature(function);
        dominance::DominanceBuilder builder(fixture.dominanceArena());
        builder.numberTree(function->dominatorTree());
        exact = exact && structuralSignature(function) == first;
    }
    context.expect(exact && numbers.size() == function->dominatorTree()->nodes().size() * 2U,
        "CF-DOM-017 iterative numbering creates unique, nested preorder/postorder intervals");
}

void frontierConstructionContract(TestContext& context, std::size_t variant)
{
    if (variant == 3)
    {
        GraphFixture fixture;
        FunctionGraph graph = makeSingle(fixture);
        dominance::DominanceArena arena;
        dominance::DominanceBuilder builder(arena);
        dominance::DominanceFunction* function = arena.makeFunction(graph.function);
        context.expect(!builder.buildFrontier(function) &&
                builder.error() == "dominance frontier requires a dominator tree",
            "TF-DOM-139 frontier construction rejects a missing tree prerequisite");
        return;
    }
    if (variant == 4)
    {
        context.expect(false,
            "TF-DOM-140 TEST-INFRASTRUCTURE: cyclic idom ascent requires process isolation");
        return;
    }
    if (variant == 6)
    {
        GraphFixture fixture;
        FunctionGraph graph = makeSingle(fixture, "SelfEdge");
        fixture.connect(graph, 0, 0);
        dominance::DominanceFunction* function = buildAnalysis(
            context, fixture, graph, "TF-DOM-142 self-edge dominance producer must build");
        if (function == nullptr)
        {
            return;
        }
        const auto& frontier = function->dominanceFrontier()->frontier(graph.nodes[0]);
        context.expect(frontier.size() == 1 && frontier.front() == graph.nodes[0],
            "TF-DOM-142 a self-edge owner has the exact singleton self dominance frontier");
        return;
    }
    GraphFixture fixture;
    FunctionGraph graph;
    if (variant == 8)
    {
        constexpr std::size_t armCount = 256;
        graph = fixture.addFunction("WideFrontier", armCount + 2U);
        const std::size_t join = armCount + 1U;
        for (std::size_t arm = 1; arm <= armCount; ++arm)
        {
            fixture.connect(graph, 0, arm);
            fixture.connect(graph, arm, join);
        }
    }
    else if (variant == 5)
    {
        graph = makeLinear(fixture, 4);
    }
    else
    {
        graph = makeMultipleJoins(fixture);
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-018 producer must build frontier-construction fixture");
    if (function == nullptr)
    {
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* owner : graph.function->reversePostOrder())
    {
        const auto& actual = function->dominanceFrontier()->frontier(owner);
        const NodeSet actualSet(actual.begin(), actual.end());
        exact = exact && actualSet == expectedFrontier(graph.function, owner, sets);
        std::size_t prior = 0;
        bool havePrior = false;
        for (const cfg::CfgNode* member : actual)
        {
            const std::size_t current = function->dominatorTree()->findNode(member)->reversePostOrderIndex();
            exact = exact && (!havePrior || prior < current);
            prior = current;
            havePrior = true;
        }
    }
    context.expect(exact, "CF-DOM-018 frontier construction is unique, complete, and RPO ordered");
}

void builderErrorContract(TestContext& context, std::size_t variant)
{
    dominance::DominanceArena arena;
    dominance::DominanceBuilder builder(arena);
    if (variant == 1 || variant == 5)
    {
        GraphFixture fixture;
        if (variant == 1)
        {
            makeSingle(fixture);
        }
        context.expect(builder.build(fixture.cfgModule()) != nullptr && builder.succeeded() &&
                builder.error().empty(),
            "CF-DOM-019 a fresh builder exposes exact success and empty error state");
        return;
    }
    context.expect(builder.build(nullptr) == nullptr && !builder.succeeded(),
        "CF-DOM-019 first invalid input establishes failure state");
    const std::string firstError = builder.error();
    if (variant == 2 || variant == 3 || variant == 4 || variant == 7)
    {
        GraphFixture invalid;
        FunctionGraph graph = makeSingle(invalid);
        graph.function->setReversePostOrder({});
        static_cast<void>(builder.build(invalid.cfgModule()));
        context.expect(builder.error() == firstError,
            "CF-DOM-019 repeated failures retain the first builder diagnostic");
        return;
    }
    GraphFixture valid;
    if (variant == 8)
    {
        makeLinear(valid, 4096);
    }
    else
    {
        makeDiamond(valid);
    }
    dominance::DominanceModule* module = builder.build(valid.cfgModule());
    context.expect(module != nullptr && !builder.succeeded() && builder.error() == firstError,
        "CF-DOM-019 builder reuse preserves its first error even when later construction succeeds");
}

void moduleDumpContract(TestContext& context, std::size_t variant)
{
    if (variant == 3)
    {
        context.expect(dump(nullptr) == "dominance.module\n<null>\n",
            "TF-DOM-155 null module dump is byte exact");
        return;
    }
    GraphFixture fixture;
    if (variant != 5)
    {
        if (variant == 8)
        {
            makeLinear(fixture, 4096, "DeepDump");
        }
        else
        {
            makeSingle(fixture, "Dump");
            if (variant == 2 || variant == 6)
            {
                makeDiamond(fixture, "Second");
            }
        }
    }
    dominance::DominanceModule* module = fixture.build();
    const std::string first = dump(module);
    if (variant == 5)
    {
        context.expect(first == "dominance.module\n",
            "TF-DOM-157 an empty module dump contains only the heading");
        return;
    }
    const std::string second = dump(module);
    context.expect(first == second && first.starts_with("dominance.module\n") &&
            first.find("function @") != std::string::npos,
        "CF-DOM-020 module dump and function order are byte deterministic");
}

void functionDumpContract(TestContext& context, std::size_t variant)
{
    if (variant == 3 || variant == 4)
    {
        GraphFixture fixture;
        FunctionGraph graph = makeSingle(fixture);
        dominance::DominanceArena arena;
        dominance::DominanceModule* module = arena.makeModule(fixture.cfgModule());
        module->addFunction(variant == 3 ? nullptr : arena.makeFunction(graph.function));
        context.expect(dump(module) == "dominance.module\nfunction <invalid>\n",
            "CF-DOM-021 invalid function tuple emits the exact invalid marker");
        return;
    }
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 4096, "Deep") :
        (variant == 6 ? makeLoop(fixture, "LoopDump") : makeSingle(fixture, "Single"));
    if (variant == 5)
    {
        fixture.addUnreachable(graph, "dead");
    }
    dominance::DominanceModule* module = fixture.build();
    const std::string actual = dump(module);
    if (variant == 1)
    {
        const std::string expected =
            "dominance.module\n"
            "function @Single\n"
            "  reachable: 1 cfg-nodes: 1 excluded-unreachable: 0\n"
            "  root: bb0\n"
            "  tree:\n"
            "    bb0\n"
            "  blocks:\n"
            "    bb0 block.0 idom=<root> depth=0 interval=[0,1] dom={bb0} df={}\n";
        context.expect(actual == expected, "TF-DOM-161 minimal function dump is byte exact");
        return;
    }
    bool exact = actual.find("idom=") != std::string::npos &&
        actual.find(" interval=[") != std::string::npos && actual.find(" dom={") != std::string::npos &&
        actual.find(" df={") != std::string::npos;
    if (variant == 5)
    {
        exact = exact && actual.find("excluded-unreachable: 1") != std::string::npos;
    }
    if (variant == 7)
    {
        exact = exact && actual == dump(module);
    }
    context.expect(exact, "CF-DOM-021 function dump exposes all dominance tuple fields exactly");
}

void treeDumpContract(TestContext& context, std::size_t variant)
{
    dominance::DominanceDumper dumper;
    if (variant == 3 || variant == 5)
    {
        std::ostringstream stream;
        dumper.dumpTree(nullptr, stream);
        context.expect(stream.str() == "    -\n",
            "CF-DOM-022 a null tree root emits the byte-exact dash marker");
        return;
    }
    if (variant == 4)
    {
        context.expect(false,
            "TF-DOM-172 TEST-INFRASTRUCTURE: cyclic tree dumping requires process isolation");
        return;
    }
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 4096) :
        (variant == 6 ? makeMultipleJoins(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-022 producer must build tree-dump fixture");
    if (function == nullptr)
    {
        return;
    }
    std::ostringstream first;
    std::ostringstream second;
    dumper.dumpTree(function->dominatorTree()->root(), first);
    dumper.dumpTree(function->dominatorTree()->root(), second);
    context.expect(first.str() == second.str() && first.str().starts_with("    bb0\n"),
        "CF-DOM-022 explicit-stack tree dump preserves child order and terminates deeply");
}

void dumpFragmentContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = makeSingle(fixture);
    std::ostringstream block;
    std::ostringstream indent;
    const cfg::CfgNode* node = variant == 3 || variant == 5 ? nullptr : graph.nodes.front();
    dominance::DominanceDumper::dumpBlockRef(node, block);
    const std::size_t depth = variant == 8 ? 4096U : (variant == 1 ? 0U : variant);
    dominance::DominanceDumper::dumpIndent(depth, indent);
    const std::string expectedBlock = node == nullptr ? "<root>" : "bb0";
    context.expect(block.str() == expectedBlock && indent.str() == std::string(depth * 2U, ' '),
        "CF-DOM-023 block references and two-space indentation are byte exact");
}

void verifierModuleContract(TestContext& context, std::size_t variant)
{
    dominance::DominanceVerifier verifier;
    if (variant == 3)
    {
        context.expect(!verifier.verify(nullptr) &&
                verifier.error() == "dominance module must reference a CFG module",
            "TF-DOM-187 null module fails with the first module-level diagnostic");
        return;
    }
    GraphFixture fixture;
    if (variant == 8)
    {
        for (std::size_t index = 0; index < 256U; ++index)
        {
            makeSingle(fixture, "Verify" + std::to_string(index));
        }
        dominance::DominanceModule* module = fixture.build();
        context.expect(module != nullptr && module->functions().size() == 256U && verifier.verify(module),
            "TF-DOM-192 verifier accepts a high-volume exact module/function correspondence");
        return;
    }
    FunctionGraph first = makeSingle(fixture, "First");
    FunctionGraph second;
    if (variant == 4 || variant == 5 || variant == 6)
    {
        second = makeSingle(fixture, "Second");
    }
    dominance::DominanceModule* module = fixture.build();
    if (variant == 4)
    {
        module->functions().front()->setDominatorTree(nullptr);
    }
    else if (variant == 5)
    {
        module->addFunction(module->functions().front());
    }
    else if (variant == 6)
    {
        module->functions().front()->setDominanceFrontier(nullptr);
    }
    if (variant == 4 || variant == 5 || variant == 6)
    {
        context.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-DOM-024 cardinality, ownership, and tuple corruption fail with one diagnostic");
        return;
    }
    if (variant == 7)
    {
        const bool firstResult = verifier.verify(module);
        const bool secondResult = verifier.verify(module);
        context.expect(firstResult && secondResult && verifier.error().empty(),
            "TF-DOM-191 repeated valid verification is deterministic and clears stale state");
        return;
    }
    context.expect(verifier.verify(module) && module->findFunction(first.function) != nullptr &&
            (second.function == nullptr || module->findFunction(second.function) != nullptr),
        "CF-DOM-024 verifier accepts exact module cardinality, ownership, and index identity");
}
}

void registerDominanceTraversalDump129To192Module(TestRegistry& registry)
{
    addDominanceContract(registry, 17, "component", traversalNumberingContract, {4});
    addDominanceContract(registry, 18, "component", frontierConstructionContract, {4});
    addDominanceContract(registry, 19, "unit", builderErrorContract);
    addDominanceContract(registry, 20, "unit", moduleDumpContract);
    addDominanceContract(registry, 21, "unit", functionDumpContract);
    addDominanceContract(registry, 22, "unit", treeDumpContract, {4});
    addDominanceContract(registry, 23, "unit", dumpFragmentContract);
    addDominanceContract(registry, 24, "component", verifierModuleContract);
}
}
