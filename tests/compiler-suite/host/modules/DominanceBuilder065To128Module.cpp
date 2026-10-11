#include "DominanceTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace dominance_support;

void dominatorChainContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeLoop(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-009 producer must build dominator-chain fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(tree->dominators(&unknown).empty(),
            "TF-DOM-067 an unknown identity has an empty dominator chain");
        return;
    }
    if (variant == 4)
    {
        context.expect(false,
            "TF-DOM-068 TEST-INFRASTRUCTURE: a cyclic parent chain requires process isolation");
        return;
    }
    if (variant == 8)
    {
        const auto chain = tree->dominators(graph.nodes.back());
        context.expect(chain.size() == graph.nodes.size() &&
                chain.front() == graph.function->entryNode() && chain.back() == graph.nodes.back(),
            "TF-DOM-072 linear scale chain is complete and ordered without rebuilding every prefix");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* node : graph.function->reversePostOrder())
    {
        std::vector<const cfg::CfgNode*> expected;
        const cfg::CfgNode* cursor = node;
        while (cursor != nullptr)
        {
            expected.push_back(cursor);
            cursor = expectedImmediateDominator(cursor, graph.function->entryNode(), sets);
        }
        std::reverse(expected.begin(), expected.end());
        exact = exact && tree->dominators(node) == expected;
    }
    if (variant == 5)
    {
        exact = exact && tree->dominators(graph.function->entryNode()).size() == 1U;
    }
    if (variant == 7)
    {
        exact = exact && tree->dominators(graph.nodes.back()) == tree->dominators(graph.nodes.back());
    }
    context.expect(exact, "CF-DOM-009 dominator chains are ordered root-to-node and exact");
}

void nearestCommonDominatorContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeMultipleJoins(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-010 producer must build nearest-common-dominator fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(tree->nearestCommonDominator(&unknown, graph.nodes.front()) == nullptr,
            "TF-DOM-075 an unknown identity has no nearest common dominator");
        return;
    }
    if (variant == 4)
    {
        context.expect(false,
            "TF-DOM-076 TEST-INFRASTRUCTURE: a broken parent/depth chain requires crash isolation");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* first : graph.function->reversePostOrder())
    {
        for (const cfg::CfgNode* second : graph.function->reversePostOrder())
        {
            const cfg::CfgNode* expected = expectedCommonDominator(
                first, second, graph.function->entryNode(), sets);
            exact = exact && tree->nearestCommonDominator(first, second) == expected;
        }
    }
    if (variant == 5)
    {
        exact = exact && tree->nearestCommonDominator(graph.nodes.front(), graph.nodes.front()) ==
            graph.nodes.front();
    }
    context.expect(exact, "CF-DOM-010 nearest-common-dominator pair matrix matches the independent oracle");
}

void frontierStorageContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph;
    if (variant == 8)
    {
        constexpr std::size_t armCount = 256;
        graph = fixture.addFunction("WideStorage", armCount + 2U);
        const std::size_t join = armCount + 1U;
        for (std::size_t arm = 1; arm <= armCount; ++arm)
        {
            fixture.connect(graph, 0, arm);
            fixture.connect(graph, arm, join);
        }
    }
    else
    {
        graph = makeDiamond(fixture);
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-011 producer must build frontier fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominanceFrontier* frontier = function->dominanceFrontier();
    if (variant == 3)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(frontier->frontier(&unknown).empty() &&
                !frontier->contains(&unknown, graph.nodes.front()),
            "TF-DOM-083 an unregistered owner maps to the immutable empty frontier contract");
        return;
    }
    if (variant == 4)
    {
        cfg::CfgNode foreign(nullptr);
        frontier->setFrontier(&foreign, {graph.nodes.back()});
        context.expect(frontier->frontier(&foreign) ==
                std::vector<const cfg::CfgNode*>{graph.nodes.back()} &&
                frontier->contains(&foreign, graph.nodes.back()),
            "TF-DOM-084 storage preserves an explicitly registered foreign key; verification owns scope checks");
        return;
    }
    if (variant == 5)
    {
        frontier->setFrontier(graph.nodes[1], {graph.nodes.back(), graph.nodes.back()});
        context.expect(!verifies(fixture.dominanceModule()),
            "TF-DOM-085 duplicate frontier members are rejected");
        return;
    }
    if (variant == 6)
    {
        GraphFixture loopFixture;
        FunctionGraph loop = makeLoop(loopFixture);
        dominance::DominanceFunction* loopAnalysis = buildAnalysis(
            context, loopFixture, loop, "TF-DOM-086 loop frontier fixture must build");
        context.expect(loopAnalysis != nullptr &&
                loopAnalysis->dominanceFrontier()->contains(loop.nodes[1], loop.nodes[1]) &&
                loopAnalysis->dominanceFrontier()->contains(loop.nodes[2], loop.nodes[1]),
            "TF-DOM-086 loop header and body store the loop-header frontier");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = frontier->tree() == function->dominatorTree();
    for (const cfg::CfgNode* owner : graph.function->reversePostOrder())
    {
        const NodeSet expected = expectedFrontier(graph.function, owner, sets);
        const auto& actual = frontier->frontier(owner);
        exact = exact && NodeSet(actual.begin(), actual.end()) == expected;
        for (const cfg::CfgNode* candidate : graph.function->reversePostOrder())
        {
            exact = exact && frontier->contains(owner, candidate) == expected.contains(candidate);
        }
    }
    context.expect(exact, "CF-DOM-011 stored frontier vectors and membership match the independent set oracle");
}

void moduleBuildContract(TestContext& context, std::size_t variant)
{
    if (variant == 3)
    {
        dominance::DominanceArena arena;
        dominance::DominanceBuilder builder(arena);
        context.expect(builder.build(nullptr) == nullptr && !builder.succeeded() &&
                builder.error() == "dominance builder requires a CFG module",
            "TF-DOM-091 null CFG input fails with the contractual first error");
        return;
    }
    GraphFixture fixture;
    std::vector<FunctionGraph> graphs;
    const std::size_t count = variant == 8 ? 256U : (variant == 5 ? 0U : 3U);
    for (std::size_t index = 0; index < count; ++index)
    {
        graphs.push_back(index % 2U == 0U ?
            makeSingle(fixture, "Build" + std::to_string(index)) :
            makeDiamond(fixture, "Build" + std::to_string(index)));
    }
    if (variant == 4)
    {
        graphs.front().function->setReversePostOrder({});
    }
    dominance::DominanceModule* module = fixture.build();
    if (variant == 4)
    {
        context.expect(module == nullptr && !fixture.builder()->succeeded() &&
                fixture.builder()->error().find("reachable CFG entry") != std::string::npos,
            "TF-DOM-092 a function without reachable RPO is rejected atomically");
        return;
    }
    bool exact = module != nullptr && fixture.builder()->succeeded() &&
        module->functions().size() == graphs.size();
    for (std::size_t index = 0; exact && index < graphs.size(); ++index)
    {
        exact = module->functions()[index]->cfgFunction() == graphs[index].function;
    }
    context.expect(exact && verifies(module),
        "CF-DOM-012 module build preserves source-function order and exact success state");
}

void functionPipelineContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeIrreducible(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-013 per-function pipeline must produce an analysis");
    if (function == nullptr)
    {
        return;
    }
    if (variant == 3)
    {
        function->setDominatorTree(nullptr);
    }
    else if (variant == 4)
    {
        function->setDominanceFrontier(nullptr);
    }
    else if (variant == 5)
    {
        context.expect(function->dominatorTree()->nodes().size() == graph.nodes.size() &&
                function->dominanceFrontier()->tree() == function->dominatorTree(),
            "TF-DOM-101 single complete tuple contains every pipeline output");
        return;
    }
    if (variant == 3 || variant == 4)
    {
        context.expect(!verifies(fixture.dominanceModule()),
            "CF-DOM-013 verifier rejects an incomplete per-function pipeline tuple");
        return;
    }
    context.expect(verifies(fixture.dominanceModule()) &&
            function->dominatorTree()->root()->cfgNode() == graph.function->entryNode(),
        "CF-DOM-013 idoms, materialization, numbering, and frontier complete as one pipeline");
}

void fixedPointContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeIrreducible(fixture) : makeMultipleJoins(fixture));
    if (variant == 3)
    {
        graph.nodes[1]->setReachable(false);
    }
    else if (variant == 4)
    {
        graph.function->setReversePostOrder({graph.nodes[0], graph.nodes[0]});
    }
    dominance::DominanceModule* module = fixture.build();
    if (variant == 3 || variant == 4)
    {
        context.expect(module == nullptr && !fixture.builder()->succeeded(),
            "CF-DOM-014 invalid reachable/RPO relations fail before a partial result is returned");
        return;
    }
    dominance::DominanceFunction* function = analysis(fixture, graph);
    if (function == nullptr)
    {
        context.expect(false, "CF-DOM-014 product builder did not produce the fixed-point fixture");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* node : graph.function->reversePostOrder())
    {
        exact = exact && function->dominatorTree()->immediateDominator(node) ==
            expectedImmediateDominator(node, graph.function->entryNode(), sets);
    }
    if (variant == 5)
    {
        exact = exact && sets.at(graph.function->entryNode()) == NodeSet{graph.function->entryNode()};
    }
    context.expect(exact, "CF-DOM-014 CHK fixed point matches independently intersected dominator sets");
}

void intersectionContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    const std::size_t count = variant == 8 ? 2048U : 5U;
    FunctionGraph graph = makeLinear(fixture, count);
    dominance::DominanceArena arena;
    dominance::DominanceBuilder builder(arena);
    std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*> idoms;
    std::unordered_map<const cfg::CfgNode*, std::size_t> indices;
    idoms[graph.nodes.front()] = graph.nodes.front();
    indices[graph.nodes.front()] = 0;
    for (std::size_t index = 1; index < count; ++index)
    {
        idoms[graph.nodes[index]] = graph.nodes[index - 1];
        indices[graph.nodes[index]] = index;
    }
    if (variant == 3)
    {
        indices.erase(graph.nodes.back());
    }
    else if (variant == 4)
    {
        idoms.erase(graph.nodes.back());
    }
    const cfg::CfgNode* first = variant == 5 ? graph.nodes.front() : graph.nodes.back();
    const cfg::CfgNode* second = variant == 5 ? graph.nodes.front() : graph.nodes[count / 2U];
    const cfg::CfgNode* result = builder.intersect(first, second, idoms, indices);
    if (variant == 3 || variant == 4)
    {
        context.expect(result == nullptr,
            "CF-DOM-015 missing RPO/index ancestry returns null without fabricating an intersection");
        return;
    }
    context.expect(result == second,
        "CF-DOM-015 RPO intersection advances explicit idom chains to the exact common node");
}

void treeMaterializationContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeMultipleJoins(fixture) : makeDiamond(fixture));
    if (variant == 3 || variant == 4)
    {
        dominance::DominanceArena arena;
        dominance::DominanceBuilder builder(arena);
        dominance::DominatorTree* tree = arena.makeTree(graph.function);
        std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*> idoms;
        idoms[graph.nodes.front()] = graph.nodes.front();
        for (std::size_t index = 1; index < graph.nodes.size(); ++index)
        {
            idoms[graph.nodes[index]] = variant == 4 && index == 1U ?
                graph.nodes[index] : graph.nodes.front();
        }
        if (variant == 3)
        {
            idoms.erase(graph.nodes[1]);
        }
        context.expect(!builder.materializeTree(graph.function, tree, idoms) && !builder.error().empty(),
            "CF-DOM-016 missing or self immediate dominator prevents partial tree materialization");
        return;
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-016 producer must materialize the dominator tree");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    bool exact = tree->nodes().size() == graph.function->reversePostOrder().size() &&
        tree->root()->cfgNode() == graph.function->entryNode();
    for (std::size_t index = 0; exact && index < tree->nodes().size(); ++index)
    {
        auto* node = tree->nodes()[index];
        exact = node->cfgNode() == graph.function->reversePostOrder()[index] &&
            node->reversePostOrderIndex() == index &&
            (index == 0U || node->depth() == node->immediateDominator()->depth() + 1U);
    }
    context.expect(exact && verifies(fixture.dominanceModule()),
        "CF-DOM-016 materialized tree preserves sequence, parent, child, and depth relations");
}
}

void registerDominanceBuilder065To128Module(TestRegistry& registry)
{
    addDominanceContract(registry, 9, "unit", dominatorChainContract, {4});
    addDominanceContract(registry, 10, "unit", nearestCommonDominatorContract, {4});
    addDominanceContract(registry, 11, "unit", frontierStorageContract);
    addDominanceContract(registry, 12, "component", moduleBuildContract);
    addDominanceContract(registry, 13, "component", functionPipelineContract);
    addDominanceContract(registry, 14, "component", fixedPointContract);
    addDominanceContract(registry, 15, "unit", intersectionContract);
    addDominanceContract(registry, 16, "component", treeMaterializationContract);
}
}
