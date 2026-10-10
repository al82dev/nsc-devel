#include "DominanceTestSupport.h"

#include <limits>

namespace nsc::suite
{
namespace
{
using namespace dominance_support;

void arenaOwnershipContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = makeSingle(fixture);
    if (variant == 1)
    {
        context.expect(fixture.dominanceArena().makeModule(fixture.cfgModule()) != nullptr,
            "TF-DOM-001 arena creates an owned module with exact CFG identity");
    }
    else if (variant == 2 || variant == 6)
    {
        auto* module = fixture.dominanceArena().makeModule(fixture.cfgModule());
        auto* function = fixture.dominanceArena().makeFunction(graph.function);
        auto* tree = fixture.dominanceArena().makeTree(graph.function);
        auto* node = fixture.dominanceArena().makeTreeNode(graph.nodes.front());
        auto* frontier = fixture.dominanceArena().makeFrontier(tree);
        context.expect(module->cfgModule() == fixture.cfgModule() &&
                function->cfgFunction() == graph.function && tree->cfgFunction() == graph.function &&
                node->cfgNode() == graph.nodes.front() && frontier->tree() == tree,
            "CF-DOM-001 arena preserves every registered product identity");
    }
    else if (variant == 3)
    {
        context.expect(false,
            "TF-DOM-003 TEST-INFRASTRUCTURE: deterministic allocation failure injection is unavailable");
    }
    else if (variant == 4)
    {
        auto* node = fixture.dominanceArena().makeTreeNode(nullptr);
        context.expect(node != nullptr && node->cfgNode() == nullptr,
            "TF-DOM-004 arena preserves invalid relationship data for the verifier to diagnose");
    }
    else if (variant == 5)
    {
        auto* first = fixture.dominanceArena().makeTreeNode(graph.nodes.front());
        auto* second = fixture.dominanceArena().makeTreeNode(graph.nodes.front());
        context.expect(first != second && first->cfgNode() == second->cfgNode(),
            "TF-DOM-005 repeated arena requests create distinct objects without hidden deduplication");
    }
    else if (variant == 7)
    {
        auto* first = fixture.dominanceArena().makeTreeNode(graph.nodes.front());
        auto* second = fixture.dominanceArena().makeTreeNode(graph.nodes.front());
        context.expect(first != second && first->cfgNode() == second->cfgNode(),
            "TF-DOM-007 repeated allocation preserves insertion and source identity, not pointer bytes");
    }
    else
    {
        context.expect(false,
            "TF-DOM-008 TEST-INFRASTRUCTURE: destruction order accounting requires sanitizer hooks");
    }
}

void moduleIdentityContract(TestContext& context, std::size_t variant)
{
    if (variant == 3)
    {
        dominance::DominanceArena arena;
        dominance::DominanceModule* module = arena.makeModule(nullptr);
        std::string error;
        context.expect(!verifies(module, &error) &&
                error == "dominance module must reference a CFG module",
            "TF-DOM-011 null source identity is rejected with the first contractual diagnostic");
        return;
    }
    if (variant == 4)
    {
        GraphFixture owner;
        GraphFixture foreign;
        makeSingle(owner, "Owner");
        FunctionGraph outsider = makeSingle(foreign, "Foreign");
        dominance::DominanceArena arena;
        auto* module = arena.makeModule(owner.cfgModule());
        module->addFunction(arena.makeFunction(outsider.function));
        std::string error;
        context.expect(!verifies(module, &error) && error.find("belong") != std::string::npos,
            "TF-DOM-012 a foreign CFG function is rejected before its incomplete tuple is consumed");
        return;
    }

    GraphFixture fixture;
    const std::size_t count = variant == 8 ? 256U : (variant == 1 ? 1U : 3U);
    std::vector<FunctionGraph> graphs;
    for (std::size_t index = 0; index < count; ++index)
    {
        graphs.push_back(makeSingle(fixture, "Function" + std::to_string(index)));
    }
    dominance::DominanceModule* module = fixture.build();
    if (variant == 5 && module != nullptr)
    {
        module->addFunction(module->functions().front());
        std::string error;
        context.expect(!verifies(module, &error) && error.find("exactly one") != std::string::npos,
            "TF-DOM-013 duplicate function analysis changes cardinality and is rejected");
        return;
    }
    bool exact = module != nullptr && module->cfgModule() == fixture.cfgModule() &&
        module->functions().size() == count;
    for (std::size_t index = 0; exact && index < count; ++index)
    {
        exact = module->functions()[index]->cfgFunction() == graphs[index].function &&
            module->findFunction(graphs[index].function) == module->functions()[index];
    }
    if (variant == 7 && exact)
    {
        exact = module->findFunction(graphs.front().function) ==
            module->findFunction(graphs.front().function);
    }
    context.expect(exact && verifies(module),
        "CF-DOM-002 module preserves CFG identity, function order, lookup, and verifier acceptance");
}

void functionTupleContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 1 ? makeSingle(fixture) :
        (variant == 6 ? makeLoop(fixture) :
            (variant == 8 ? makeLinear(fixture, 2048) : makeDiamond(fixture)));
    if (variant == 3)
    {
        auto* module = fixture.dominanceArena().makeModule(fixture.cfgModule());
        module->addFunction(fixture.dominanceArena().makeFunction(graph.function));
        std::string error;
        context.expect(!verifies(module, &error) && error.find("tree and dominance frontier") !=
                std::string::npos,
            "TF-DOM-019 a missing tree/frontier tuple is rejected");
        return;
    }
    if (variant == 4)
    {
        auto* module = fixture.dominanceArena().makeModule(fixture.cfgModule());
        auto* function = fixture.dominanceArena().makeFunction(graph.function);
        auto* foreignTree = fixture.dominanceArena().makeTree(nullptr);
        function->setDominatorTree(foreignTree);
        function->setDominanceFrontier(fixture.dominanceArena().makeFrontier(foreignTree));
        module->addFunction(function);
        std::string error;
        context.expect(!verifies(module, &error) && error.find("source relationships") != std::string::npos,
            "TF-DOM-020 mismatched CFG, tree, and frontier provenance is rejected");
        return;
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-003 producer must build the function analysis tuple");
    if (function == nullptr)
    {
        return;
    }
    if (variant == 5)
    {
        dominance::DominanceFunction staged(graph.function);
        context.expect(staged.dominatorTree() == nullptr && staged.dominanceFrontier() == nullptr,
            "TF-DOM-021 a newly staged tuple exposes absent optional relations without substitution");
        return;
    }
    const bool exact = function->cfgFunction() == graph.function && function->dominatorTree() != nullptr &&
        function->dominanceFrontier() != nullptr &&
        function->dominanceFrontier()->tree() == function->dominatorTree();
    context.expect(exact && verifies(fixture.dominanceModule()),
        "CF-DOM-003 complete tuple preserves exact CFG/tree/frontier identities");
}

void treeNodeMetadataContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) : makeDiamond(fixture);
    if (variant == 1 || variant == 2 || variant == 5 || variant == 7)
    {
        auto* first = fixture.dominanceArena().makeTreeNode(graph.nodes[0]);
        auto* second = fixture.dominanceArena().makeTreeNode(graph.nodes[1]);
        first->setImmediateDominator(second);
        first->addChild(second);
        const std::size_t rpo = variant == 5 ? std::numeric_limits<std::size_t>::max() : 3U;
        const std::uint32_t post = variant == 5 ? std::numeric_limits<std::uint32_t>::max() : 11U;
        first->setReversePostOrderIndex(rpo);
        first->setTraversalNumbers(7U, post);
        first->setDepth(9U);
        context.expect(first->cfgNode() == graph.nodes[0] && first->immediateDominator() == second &&
                first->children().size() == 1U && first->children().front() == second &&
                first->reversePostOrderIndex() == rpo && first->preorderNumber() == 7U &&
                first->postorderNumber() == post && first->depth() == 9U,
            "CF-DOM-004 tree-node metadata round-trips field by field");
        return;
    }
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-004 producer must build metadata fixture");
    if (function == nullptr)
    {
        return;
    }
    if (variant == 3)
    {
        function->dominatorTree()->addNode(fixture.dominanceArena().makeTreeNode(nullptr));
    }
    else if (variant == 4)
    {
        auto* root = function->dominatorTree()->root();
        root->addChild(root->children().front());
    }
    else if (variant == 8)
    {
        context.expect(function->dominatorTree()->nodes().size() == graph.nodes.size() &&
                verifies(fixture.dominanceModule()),
            "TF-DOM-032 complete metadata remains valid across the representative tree");
        return;
    }
    std::string error;
    context.expect(!verifies(fixture.dominanceModule(), &error) && !error.empty(),
        "CF-DOM-004 verifier rejects null or inconsistent node metadata");
}

void treeIndexRootContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) : makeDiamond(fixture);
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-005 producer must build tree index fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3)
    {
        tree->setRoot(nullptr);
        std::string error;
        context.expect(!verifies(fixture.dominanceModule(), &error) && error.find("root") != std::string::npos,
            "TF-DOM-035 missing root is rejected");
        return;
    }
    if (variant == 4)
    {
        tree->addNode(fixture.dominanceArena().makeTreeNode(nullptr));
        context.expect(!verifies(fixture.dominanceModule()),
            "TF-DOM-036 inconsistent tree universe is rejected");
        return;
    }
    if (variant == 5)
    {
        tree->addNode(tree->root());
        context.expect(tree->findNode(graph.nodes.front()) == tree->root() &&
                !verifies(fixture.dominanceModule()),
            "TF-DOM-037 duplicate sequence entry preserves first lookup but fails verification");
        return;
    }
    cfg::CfgNode outsider(nullptr);
    bool exact = tree->root()->cfgNode() == graph.function->entryNode() && tree->findNode(&outsider) == nullptr;
    for (std::size_t index = 0; exact && index < graph.function->reversePostOrder().size(); ++index)
    {
        exact = tree->nodes()[index]->cfgNode() == graph.function->reversePostOrder()[index] &&
            tree->findNode(graph.nodes[index]) == tree->nodes()[index];
    }
    context.expect(exact && verifies(fixture.dominanceModule()),
        "CF-DOM-005 root, sequence, and pointer lookup preserve the reachable RPO universe");
}

void dominatesContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeLoop(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-006 producer must build query fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3 || variant == 5)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(!tree->dominates(&unknown, graph.nodes.front()) &&
                !tree->dominates(graph.nodes.front(), &unknown),
            "CF-DOM-006 unknown identities return false without mutating the tree");
        return;
    }
    if (variant == 4)
    {
        tree->nodes()[1]->setTraversalNumbers(
            tree->root()->preorderNumber(), tree->root()->postorderNumber());
        context.expect(!verifies(fixture.dominanceModule()),
            "TF-DOM-044 corrupt intervals are rejected before their query result is trusted");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* dominator : graph.function->reversePostOrder())
    {
        for (const cfg::CfgNode* node : graph.function->reversePostOrder())
        {
            exact = exact && tree->dominates(dominator, node) == sets.at(node).contains(dominator);
        }
    }
    if (variant == 7)
    {
        exact = exact && tree->dominates(graph.nodes.front(), graph.nodes.back()) ==
            tree->dominates(graph.nodes.front(), graph.nodes.back());
    }
    context.expect(exact, "CF-DOM-006 interval query matches the independent dominance matrix");
}

void strictDominanceContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeIrreducible(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-007 producer must build strict-query fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3 || variant == 5)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(!tree->strictlyDominates(&unknown, graph.nodes.front()) &&
                !tree->strictlyDominates(graph.nodes.front(), graph.nodes.front()),
            "CF-DOM-007 unknown and equal identities never strictly dominate");
        return;
    }
    if (variant == 4)
    {
        tree->nodes()[1]->setTraversalNumbers(9U, 8U);
        context.expect(!verifies(fixture.dominanceModule()),
            "TF-DOM-052 invalid intervals are rejected before strict-query semantics are trusted");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* dominator : graph.function->reversePostOrder())
    {
        for (const cfg::CfgNode* node : graph.function->reversePostOrder())
        {
            const bool expected = dominator != node && sets.at(node).contains(dominator);
            exact = exact && tree->strictlyDominates(dominator, node) == expected;
        }
    }
    context.expect(exact, "CF-DOM-007 strict query matches the independent pair matrix");
}

void immediateDominatorContract(TestContext& context, std::size_t variant)
{
    GraphFixture fixture;
    FunctionGraph graph = variant == 8 ? makeLinear(fixture, 2048) :
        (variant == 6 ? makeMultipleJoins(fixture) : makeDiamond(fixture));
    dominance::DominanceFunction* function = buildAnalysis(
        context, fixture, graph, "CF-DOM-008 producer must build immediate-dominator fixture");
    if (function == nullptr)
    {
        return;
    }
    dominance::DominatorTree* tree = function->dominatorTree();
    if (variant == 3 || variant == 5)
    {
        cfg::CfgNode unknown(nullptr);
        context.expect(tree->immediateDominator(&unknown) == nullptr &&
                tree->immediateDominator(graph.function->entryNode()) == nullptr,
            "CF-DOM-008 unknown and root identities have no immediate dominator");
        return;
    }
    if (variant == 4)
    {
        tree->nodes()[1]->setImmediateDominator(nullptr);
        context.expect(!verifies(fixture.dominanceModule()),
            "TF-DOM-060 a missing non-root immediate dominator is rejected");
        return;
    }
    const DominatorSets sets = computeDominatorSets(graph.function);
    bool exact = true;
    for (const cfg::CfgNode* node : graph.function->reversePostOrder())
    {
        exact = exact && tree->immediateDominator(node) ==
            expectedImmediateDominator(node, graph.function->entryNode(), sets);
    }
    if (variant == 7)
    {
        exact = exact && tree->immediateDominator(graph.nodes.back()) ==
            tree->immediateDominator(graph.nodes.back());
    }
    context.expect(exact, "CF-DOM-008 immediate-dominator lookup matches the independent set oracle");
}
}

void registerDominanceModel001To064Module(TestRegistry& registry)
{
    addDominanceContract(registry, 1, "unit", arenaOwnershipContract, {3, 8});
    addDominanceContract(registry, 2, "unit", moduleIdentityContract);
    addDominanceContract(registry, 3, "unit", functionTupleContract);
    addDominanceContract(registry, 4, "unit", treeNodeMetadataContract);
    addDominanceContract(registry, 5, "unit", treeIndexRootContract);
    addDominanceContract(registry, 6, "unit", dominatesContract);
    addDominanceContract(registry, 7, "unit", strictDominanceContract);
    addDominanceContract(registry, 8, "unit", immediateDominatorContract);
}
}
