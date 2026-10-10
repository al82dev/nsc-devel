#include "DominanceTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace dominance_support;

void independentSetContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    if (variant == 3)
    {
        FunctionGraph empty = makeSingle(fixture);
        empty.function->setReversePostOrder({});
        context.expect(computeDominatorSets(empty.function).empty(),
            "TF-DOM-195 an empty RPO produces an empty independent dominator-set map");
        return;
    }
    if (variant == 4)
    {
        FunctionGraph broken = fixture.addFunction("Broken", 2);
        context.expect(fixture.build() == nullptr && !fixture.builder()->succeeded(),
            "TF-DOM-196 reachable nodes without processed predecessors are rejected by the producer");
        return;
    }
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeIrreducible(fixture) : makeMultipleJoins(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-025 producer must build independent-set fixture");
    if (function == nullptr)
    {
        return;
    }
    const DominatorSets first = computeDominatorSets(graph.function);
    const DominatorSets second = computeDominatorSets(graph.function);
    bool exact = first.size() == graph.function->reversePostOrder().size();
    for (const cfg::CfgNode* node : graph.function->reversePostOrder())
    {
        exact = exact && first.at(node).contains(node) && sameSet(first.at(node), second.at(node));
        for (const cfg::CfgNode* candidate : graph.function->reversePostOrder())
        {
            exact = exact && function->dominatorTree()->dominates(candidate, node) ==
                first.at(node).contains(candidate);
        }
    }
    if (variant == 5)
    {
        exact = exact && first.at(graph.function->entryNode()) == NodeSet{graph.function->entryNode()};
    }
    context.expect(exact, "CF-DOM-025 independent fixed-point sets equal the tree query relation");
}

void independentIdomContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeIrreducible(fixture) : makeTwinJoins(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-026 producer must build expected-idom fixture");
    if (function == nullptr)
    {
        return;
    }
    DominatorSets sets = computeDominatorSets(graph.function);
    if (variant == 3)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(expectedImmediateDominator(&unknown, graph.function->entryNode(), sets) == nullptr,
            "TF-DOM-203 an identity absent from the independent sets has no expected idom");
        return;
    }
    if (variant == 4)
    {
        sets.erase(graph.nodes.back());
        context.expect(expectedImmediateDominator(
                graph.nodes.back(), graph.function->entryNode(), sets) == nullptr,
            "TF-DOM-204 an incomplete independent set map cannot fabricate an idom");
        return;
    }
    bool exact = true;
    for (const cfg::CfgNode* node : graph.function->reversePostOrder())
    {
        const cfg::CfgNode* expected = expectedImmediateDominator(
            node, graph.function->entryNode(), sets);
        exact = exact && function->dominatorTree()->immediateDominator(node) == expected;
    }
    if (variant == 5)
    {
        exact = exact && expectedImmediateDominator(
            graph.function->entryNode(), graph.function->entryNode(), sets) == nullptr;
    }
    if (variant == 7)
    {
        exact = exact && expectedImmediateDominator(
            graph.nodes.back(), graph.function->entryNode(), sets) ==
            expectedImmediateDominator(graph.nodes.back(), graph.function->entryNode(), sets);
    }
    context.expect(exact, "CF-DOM-026 expected-idom oracle matches every materialized tree parent");
}

void verifierUniverseContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) : makeDiamond(fixture);
    cfg::CfgNode* unreachable = fixture.addUnreachable(graph, "dead");
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-027 producer must build verifier-universe fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        graph.function->setReversePostOrder({});
    }
    else if (variant == 4)
    {
        tree->addNode(fixture.dominanceArena().makeTreeNode(unreachable));
    }
    else if (variant == 5)
    {
        tree->addNode(tree->root());
    }
    else if (variant == 6)
    {
        tree->nodes()[1]->setReversePostOrderIndex(77U);
    }
    else if (variant == 7)
    {
        tree->setRoot(tree->nodes()[1]);
    }
    if (variant >= 3 && variant <= 7)
    {
        context.expect(!verifies(fixture.dominanceModule()),
            "CF-DOM-027 verifier rejects empty, foreign, duplicate, misindexed, or wrong-root universes");
        return;
    }
    context.expect(verifies(fixture.dominanceModule()) && tree->findNode(unreachable) == nullptr,
        "CF-DOM-027 verifier accepts exactly the reachable RPO universe and entry root");
}

void verifierIntervalContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 4096) : makeDiamond(fixture);
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-028 producer must build interval-verifier fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        tree->nodes()[1]->setTraversalNumbers(9U, 8U);
    }
    else if (variant == 4)
    {
        tree->nodes()[1]->setTraversalNumbers(
            tree->root()->preorderNumber(), tree->root()->postorderNumber());
    }
    else if (variant == 5)
    {
        tree->nodes()[1]->setTraversalNumbers(0U, 1U);
    }
    else if (variant == 6)
    {
        tree->nodes()[1]->setTraversalNumbers(20U, 21U);
    }
    if (variant >= 3 && variant <= 6)
    {
        std::string error;
        context.expect(!verifies(fixture.dominanceModule(), &error) && !error.empty(),
            "CF-DOM-028 verifier rejects reversed, duplicate, overlapping, or non-ancestral intervals");
        return;
    }
    std::string firstError;
    const bool first = verifies(fixture.dominanceModule(), &firstError);
    const bool second = verifies(fixture.dominanceModule());
    context.expect(first && second && firstError.empty(),
        "CF-DOM-028 valid traversal intervals remain deterministic and globally unique");
}

void verifierParentContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 4096) : makeDiamond(fixture);
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-029 producer must build parent-verifier fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        tree->nodes()[1]->setImmediateDominator(nullptr);
    }
    else if (variant == 4)
    {
        tree->nodes()[1]->setDepth(77U);
    }
    else if (variant == 5)
    {
        tree->root()->addChild(tree->root()->children().front());
    }
    else if (variant == 6)
    {
        tree->root()->addChild(nullptr);
    }
    if (variant >= 3 && variant <= 6)
    {
        std::string error;
        context.expect(!verifies(fixture.dominanceModule(), &error) && !error.empty(),
            "CF-DOM-029 verifier rejects missing parent, wrong depth, duplicate, or null child");
        return;
    }
    const bool first = verifies(fixture.dominanceModule());
    const bool second = variant == 7 ? verifies(fixture.dominanceModule()) : first;
    context.expect(first && second,
        "CF-DOM-029 verifier accepts the exact inverse parent/children relation and derived depths");
}

void verifierQueryContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 512) :
        (variant == 6 ? makeIrreducible(fixture) : makeTwinJoins(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-030 producer must build query-verifier fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        tree->nodes()[1]->setTraversalNumbers(50U, 51U);
    }
    else if (variant == 4)
    {
        tree->nodes()[1]->setTraversalNumbers(0U, tree->nodes()[1]->postorderNumber());
    }
    else if (variant == 5)
    {
        tree->nodes().back()->setTraversalNumbers(0U, tree->root()->postorderNumber());
    }
    if (variant >= 3 && variant <= 5)
    {
        context.expect(!verifies(fixture.dominanceModule()),
            "CF-DOM-030 verifier rejects interval-induced dominance query disagreement");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* dominator : graph.function->reversePostOrder())
    {
        for (const cfg::CfgNode* node : graph.function->reversePostOrder())
        {
            const bool expected = sets.at(node).contains(dominator);
            exact = exact && tree->dominates(dominator, node) == expected &&
                tree->strictlyDominates(dominator, node) == (expected && dominator != node);
        }
    }
    context.expect(exact && verifies(fixture.dominanceModule()),
        "CF-DOM-030 exhaustive query matrix agrees with independent dominator sets");
}

void verifierFrontierContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph;
    if (variant == 8)
    {
        constexpr std::size_t armCount = 256;
        graph = fixture.addFunction("WideFrontierVerifier", armCount + 2U);
        const std::size_t join = armCount + 1U;
        for (std::size_t arm = 1; arm <= armCount; ++arm)
        {
            fixture.connect(graph, 0, arm);
            fixture.connect(graph, arm, join);
        }
    }
    else
    {
        graph = makeTwinJoins(fixture);
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-031 producer must build frontier-verifier fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominanceFrontier* frontier = function->dominanceFrontier();
    const cfg::CfgNode* owner = graph.nodes[1];
    std::vector<const cfg::CfgNode*> values = frontier->frontier(owner);
    if (variant == 3)
    {
        values.clear();
    }
    else if (variant == 4)
    {
        values.push_back(graph.nodes.back());
    }
    else if (variant == 5 && !values.empty())
    {
        values.push_back(values.front());
    }
    else if (variant == 6 && values.size() >= 2U)
    {
        std::reverse(values.begin(), values.end());
    }
    if (variant >= 3 && variant <= 6)
    {
        frontier->setFrontier(owner, std::move(values));
        std::string error;
        context.expect(!verifies(fixture.dominanceModule(), &error) &&
                error.find("dominance frontier") != std::string::npos,
            "CF-DOM-031 verifier rejects missing, extra, duplicate, or reordered frontier members");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* candidate : graph.function->reversePostOrder())
    {
        const auto& actual = frontier->frontier(candidate);
        exact = exact && NodeSet(actual.begin(), actual.end()) ==
            expectedFrontier(graph.function, candidate, sets);
    }
    context.expect(exact && verifies(fixture.dominanceModule()),
        "CF-DOM-031 frontier definition, uniqueness, and RPO order pass the independent verifier");
}

void verifierResetContract(TestContext& context, std::size_t variant)
{
    dominance::DominanceVerifier verifier;
    GraphFixture fixture;
    if (variant == 5)
    {
        dominance::DominanceArena arena;
        dominance::DominanceModule* empty = arena.makeModule(fixture.cfgModule());
        context.expect(verifier.verify(empty) && verifier.error().empty(),
            "TF-DOM-253 an empty CFG/dominance module pair is valid and leaves no error");
        return;
    }
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) : makeDiamond(fixture);
    dominance::DominanceModule* module = fixture.build();
    if (variant == 1 || variant == 2 || variant == 7 || variant == 8)
    {
        const bool first = verifier.verify(module);
        const bool second = verifier.verify(module);
        context.expect(first && second && verifier.error().empty(),
            "CF-DOM-032 repeated valid calls reset state and preserve an empty error");
        return;
    }
    context.expect(!verifier.verify(nullptr) && !verifier.error().empty(),
        "CF-DOM-032 invalid call records its first structural diagnostic");
    const std::string firstError = verifier.error();
    if (variant == 3)
    {
        context.expect(verifier.verify(module) && verifier.error().empty(),
            "TF-DOM-251 a later valid call clears the prior verifier error");
        return;
    }
    if (variant == 4)
    {
        module->findFunction(graph.function)->setDominatorTree(nullptr);
        context.expect(!verifier.verify(module) && verifier.error() != firstError &&
                verifier.error().find("tree and dominance frontier") != std::string::npos,
            "TF-DOM-252 each call resets state and retains its own first structural violation");
        return;
    }
    module->findFunction(graph.function)->setDominanceFrontier(nullptr);
    const bool failed = !verifier.verify(module);
    const std::string tupleError = verifier.error();
    context.expect(failed && tupleError != firstError && verifier.verify(fixture.build()) &&
            verifier.error().empty(),
        "TF-DOM-254 verifier reuse exposes each call's result/error sequence without stale state");
}
}

void registerDominanceVerifier193To256Module(TestRegistry& registry)
{
    addDominanceContract(registry, 25, "component", independentSetContract);
    addDominanceContract(registry, 26, "component", independentIdomContract);
    addDominanceContract(registry, 27, "component", verifierUniverseContract);
    addDominanceContract(registry, 28, "component", verifierIntervalContract);
    addDominanceContract(registry, 29, "component", verifierParentContract);
    addDominanceContract(registry, 30, "component", verifierQueryContract);
    addDominanceContract(registry, 31, "component", verifierFrontierContract);
    addDominanceContract(registry, 32, "unit", verifierResetContract);
}
}
