#include "CfgTestSupport.h"

#include <array>
#include <string>
#include <unordered_set>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::cfg;
using namespace cfg_support;

void arenaContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    if (variant == 1)
    {
        c.expect(fixture.makeManualModule() != nullptr,
            "TF-CFG-001 arena creates and owns a minimal CFG module");
    }
    else if (variant == 2 || variant == 6)
    {
        auto* module = fixture.makeManualModule();
        auto* function = fixture.makeManualFunction();
        auto* block = fixture.addReturnBlock("entry");
        auto* node = fixture.makeNode(block);
        auto* edge = fixture.makeEdge(node, node, CfgEdgeKind::Jump);
        c.expect(module != nullptr && function != nullptr && node != nullptr && edge != nullptr,
            "CF-CFG-001 arena creates every registered CFG object kind");
    }
    else if (variant == 3)
    {
        c.expect(false,
            "TF-CFG-003 TEST-INFRASTRUCTURE: deterministic allocation failure injection is unavailable");
    }
    else if (variant == 4)
    {
        CfgEdge* edge = fixture.makeEdge(nullptr, nullptr, CfgEdgeKind::Jump);
        c.expect(false && edge != nullptr,
            "TF-CFG-004 PLAN-CONTRADICTION: CfgArena is an owner, not a validator; invalid endpoints are rejected by CfgVerifier");
    }
    else if (variant == 5)
    {
        auto* block = fixture.addReturnBlock("same");
        CfgNode* first = fixture.makeNode(block);
        CfgNode* second = fixture.makeNode(block);
        c.expect(first != second,
            "TF-CFG-005 repeated arena requests create distinct owned objects without hidden deduplication");
    }
    else if (variant == 7)
    {
        c.expect(false,
            "TF-CFG-007 PLAN-CONTRADICTION: byte identity cannot include allocator-dependent pointer values");
    }
    else
    {
        c.expect(false,
            "TF-CFG-008 TEST-INFRASTRUCTURE: lifetime and high-volume destruction require sanitizer instrumentation");
    }
}

void edgeKindContract(TestContext& c, std::size_t variant)
{
    const std::array<std::pair<CfgEdgeKind, std::string_view>, 3> rows = {{
        {CfgEdgeKind::Jump, "jump"}, {CfgEdgeKind::BranchTrue, "true"},
        {CfgEdgeKind::BranchFalse, "false"}}};
    bool exact = cfgEdgeKindName(static_cast<CfgEdgeKind>(99)) == "unknown";
    for (const auto& [kind, name] : rows) exact = exact && cfgEdgeKindName(kind) == name;
    if (variant == 5) exact = exact && cfgEdgeKindName(CfgEdgeKind::Jump) == cfgEdgeKindName(CfgEdgeKind::Jump);
    if (variant == 7)
        for (std::size_t repeat = 0; repeat < 32; ++repeat)
            exact = exact && cfgEdgeKindName(CfgEdgeKind::BranchTrue) == "true";
    if (variant == 8)
        for (int value = -1024; value <= 1024; ++value)
            exact = exact && !cfgEdgeKindName(static_cast<CfgEdgeKind>(value)).empty();
    c.expect(exact, "CF-CFG-002 exhaustive edge-kind spelling and fallback remain deterministic");
}

void edgeIdentityContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    auto* firstBlock = fixture.addReturnBlock("first");
    auto* secondBlock = fixture.addReturnBlock("second");
    CfgNode* first = fixture.makeNode(firstBlock);
    CfgNode* second = fixture.makeNode(secondBlock);
    const CfgEdgeKind kind = variant == 6 ? CfgEdgeKind::BranchFalse : CfgEdgeKind::Jump;
    CfgEdge* edge = fixture.makeEdge(variant == 3 ? nullptr : first,
        variant == 4 ? nullptr : second, kind);
    c.expect(edge->source() == (variant == 3 ? nullptr : first) &&
            edge->target() == (variant == 4 ? nullptr : second) && edge->kind() == kind,
        "CF-CFG-003 edge accessors preserve exact endpoint and kind identity");
}

void nodeAdjacencyContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeBranchGraph();
    CfgFunction* function = fixture->cfgModule()->functions().front();
    CfgNode* entry = function->nodes().front();
    if (variant == 3) entry->addOutgoing(nullptr);
    if (variant == 4) entry->addIncoming(function->edges().front());
    if (variant == 5) entry->addOutgoing(function->edges().front());
    if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            entry->addOutgoing(function->edges()[index % function->edges().size()]);

    if (variant == 1 || variant == 2 || variant == 6 || variant == 7)
    {
        const auto successors = entry->successors();
        c.expect(entry->incomingEdges().empty() && entry->outgoingEdges().size() == 2 &&
                successors.size() == 2 && successors[0] != successors[1],
            "CF-CFG-004 edge lists and unique successor projection preserve first-edge order");
    }
    else
    {
        CfgVerifier verifier;
        c.expect(!verifier.verify(fixture->cfgModule()) && !verifier.error().empty(),
            "CF-CFG-004 verifier rejects null, reversed, duplicate, and oversized corrupt registrations");
    }
}

void reachabilityContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    CfgNode* node = fixture.makeNode(variant == 3 ? nullptr : fixture.addReturnBlock("entry"));
    const bool expected = variant == 2 || variant == 4 || variant == 6 || variant == 8;
    node->setReachable(expected);
    if (variant == 5)
    {
        node->setReachable(true);
        node->setReachable(false);
    }
    c.expect(node->reachable() == (variant == 5 ? false : expected),
        "CF-CFG-005 reachability changes only through the explicit setter and preserves its last value");
}

void functionMappingContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    CfgModule* module = fixture.makeManualModule();
    CfgFunction* function = fixture.makeManualFunction();
    module->addFunction(function);
    auto* firstBlock = fixture.addReturnBlock("first");
    auto* secondBlock = fixture.addReturnBlock("second");
    CfgNode* first = fixture.makeNode(firstBlock);
    CfgNode* second = fixture.makeNode(secondBlock);
    if (variant != 3) function->addNode(first);
    if (variant == 2 || variant == 6 || variant == 7 || variant == 8) function->addNode(second);
    if (variant == 4 || variant == 5) function->addNode(first);
    if (variant == 3 || variant == 4 || variant == 5)
    {
        CfgVerifier verifier;
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-006 missing and duplicate node mappings fail verification");
        return;
    }
    c.expect(function->mirFunction() == fixture.function() &&
            function->entryNode() == (function->nodes().empty() ? nullptr : function->nodes().front()) &&
            function->findNode(firstBlock) == (variant == 3 ? nullptr : first),
        "CF-CFG-006 function node order, entry, and block lookup retain pointer identity");
}

void functionCollectionsContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeBranchGraph();
    CfgFunction* function = fixture->cfgModule()->functions().front();
    if (variant == 3) function->setReversePostOrder({});
    if (variant == 4) function->addExitNode(function->entryNode());
    if (variant == 5) function->addReachableExitNode(function->reachableExitNodes().front());
    if (variant == 8)
    {
        std::vector<CfgNode*> scale(2048, function->entryNode());
        function->setReversePostOrder(std::move(scale));
    }
    if (variant == 3 || variant == 4 || variant == 5 || variant == 8)
    {
        CfgVerifier verifier;
        c.expect(!verifier.verify(fixture->cfgModule()) && !verifier.error().empty(),
            "CF-CFG-007 stale, extra, duplicate, and scaled-corrupt collections fail verification");
        return;
    }
    c.expect(function->edges().size() == 2 && !function->nodes().empty() &&
            !function->exitNodes().empty() && !function->reachableExitNodes().empty(),
        "CF-CFG-007 function analysis collections preserve builder insertion order");
}

void moduleMappingContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    CfgModule* module = fixture.makeManualModule();
    CfgFunction* first = fixture.makeManualFunction();
    if (variant != 3) module->addFunction(first);
    if (variant == 4) module->addFunction(nullptr);
    if (variant == 2 || variant == 5 || variant == 6 || variant == 8)
        module->addFunction(fixture.makeManualFunction());
    if (variant == 3 || variant == 4 || variant == 5)
    {
        CfgVerifier verifier;
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-008 missing, null, duplicate, and count-mismatched function mappings fail verification");
        return;
    }
    c.expect(module->mirModule() == fixture.module() &&
            module->findFunction(fixture.function()) == (variant == 3 ? nullptr : first) &&
            module->functions().size() == (variant == 3 ? 0U :
                (variant == 2 || variant == 5 || variant == 6 || variant == 8 ? 2U : 1U)),
        "CF-CFG-008 module function order and first identity lookup remain exact");
}

void traversalContract(TestContext& c, std::size_t variant)
{
    if (variant == 3)
    {
        c.expect(CfgTraversal::reversePostOrder(nullptr).empty(),
            "TF-CFG-067 null traversal entry returns an empty sequence");
        return;
    }
    auto fixture = variant == 1 ? makeLinearGraph() : makeBranchGraph();
    CfgFunction* function = fixture->cfgModule()->functions().front();
    if (variant == 4) function->entryNode()->addOutgoing(nullptr);
    if (variant == 5) function->entryNode()->addOutgoing(function->edges().front());
    const auto first = CfgTraversal::reversePostOrder(function->entryNode());
    const auto second = CfgTraversal::reversePostOrder(function->entryNode());
    const std::unordered_set<CfgNode*> unique(first.begin(), first.end());
    c.expect(first == second && first.front() == function->entryNode() && unique.size() == first.size(),
        "CF-CFG-009 iterative RPO is deterministic and visits each reachable node once");
}
}

void registerCfgModelTraversalModule(TestRegistry& registry)
{
    addCfgContract(registry, 1, "component", arenaContract, {3, 4, 7, 8});
    addCfgContract(registry, 2, "unit", edgeKindContract);
    addCfgContract(registry, 3, "unit", edgeIdentityContract);
    addCfgContract(registry, 4, "unit", nodeAdjacencyContract);
    addCfgContract(registry, 5, "unit", reachabilityContract);
    addCfgContract(registry, 6, "component", functionMappingContract);
    addCfgContract(registry, 7, "component", functionCollectionsContract);
    addCfgContract(registry, 8, "component", moduleMappingContract);
    addCfgContract(registry, 9, "unit", traversalContract);
}
}
