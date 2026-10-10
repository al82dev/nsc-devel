#include "CfgTestSupport.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp;
using namespace nativesharp::cfg;
using namespace cfg_support;

void moduleParityContract(TestContext& c, std::size_t variant)
{
    if (variant == 3)
    {
        CfgVerifier verifier;
        c.expect(!verifier.verify(nullptr) &&
                verifier.error() == "CFG module must reference its source MIR module",
            "CF-CFG-019 null CFG module reports the exact first module diagnostic");
        return;
    }
    auto fixture = makeLinearGraph();
    CfgModule* module = fixture->cfgModule();
    if (variant == 4)
    {
        module = fixture->makeManualModule();
    }
    else if (variant == 5)
    {
        module->addFunction(module->functions().front());
    }
    else if (variant == 6)
    {
        fixture->function()->setDefined(false);
    }
    CfgVerifier verifier;
    if (variant >= 4 && variant <= 6)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-019 missing definitions, prototype CFGs, and count mismatches are rejected");
        return;
    }
    const bool first = verifier.verify(module);
    const bool second = verifier.verify(module);
    c.expect(first && second && verifier.error().empty(),
        "CF-CFG-019 every MIR definition has exactly one CFG and prototypes have none");
}

void functionIdentityContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeLinearGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    if (variant == 3)
    {
        CfgModule* manual = fixture->makeManualModule();
        manual->addFunction(nullptr);
        module = manual;
    }
    else if (variant == 4)
    {
        fixture->function()->setDefined(false);
    }
    else if (variant == 5)
    {
        function->addNode(function->nodes().front());
    }
    else if (variant == 6)
    {
        std::vector<mir::MirBlock*> reversed = fixture->function()->blocks();
        std::reverse(reversed.begin(), reversed.end());
        fixture->function()->replaceBlocks(std::move(reversed));
    }
    CfgVerifier verifier;
    if (variant >= 3 && variant <= 6)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-020 null, undefined, duplicate, and reordered function topology is rejected");
        return;
    }
    c.expect(verifier.verify(module) && function->entryNode() == function->nodes().front(),
        "CF-CFG-020 function identity, node order, uniqueness, and entry are exact");
}

void nodeRegistrationContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeLinearGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    CfgNode* entry = function->nodes().front();
    CfgNode* exit = function->nodes().back();
    CfgEdge* edge = function->edges().front();
    if (variant == 3) exit->addIncoming(nullptr);
    else if (variant == 4) exit->addOutgoing(edge);
    else if (variant == 5) entry->addOutgoing(edge);
    else if (variant == 6)
    {
        CfgEdge* foreign = fixture->makeEdge(entry, exit, CfgEdgeKind::Jump);
        entry->addOutgoing(foreign);
    }
    else if (variant == 8)
        for (std::size_t index = 0; index < 4096; ++index) entry->addOutgoing(edge);
    CfgVerifier verifier;
    if ((variant >= 3 && variant <= 6) || variant == 8)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-021 null, reversed, duplicate, foreign, and scaled corrupt registrations fail");
        return;
    }
    c.expect(verifier.verify(module),
        "CF-CFG-021 every local edge is known, unique, correctly oriented, and mapped back");
}

void edgeRegistrationContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeLinearGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    CfgNode* entry = function->nodes().front();
    CfgNode* exit = function->nodes().back();
    CfgEdge* edge = function->edges().front();
    if (variant == 3)
    {
        CfgEdge* invalid = fixture->makeEdge(nullptr, exit, CfgEdgeKind::Jump);
        function->addEdge(invalid);
    }
    else if (variant == 4)
    {
        auto* foreignBlock = fixture->mirArena().makeBlock("foreign");
        CfgNode* foreign = fixture->makeNode(foreignBlock);
        CfgEdge* invalid = fixture->makeEdge(entry, foreign, CfgEdgeKind::Jump);
        function->addEdge(invalid);
        entry->addOutgoing(invalid);
        foreign->addIncoming(invalid);
    }
    else if (variant == 5)
    {
        exit->addIncoming(edge);
    }
    else if (variant == 6)
    {
        entry->addOutgoing(edge);
    }
    CfgVerifier verifier;
    if (variant >= 3 && variant <= 6)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-022 null, foreign, missing, or repeated endpoint registration is rejected");
        return;
    }
    c.expect(verifier.verify(module),
        "CF-CFG-022 both endpoints belong to one function and register the edge exactly once");
}

void terminatorContract(TestContext& c, std::size_t variant)
{
    auto fixture = variant == 6 ? makeBranchGraph() : makeLinearGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    mir::MirBlock* entryBlock = fixture->function()->blocks().front();
    if (variant == 3) entryBlock->replaceInstructions({});
    else if (variant == 4) entryBlock->replaceInstructions({nullptr});
    else if (variant == 5)
        entryBlock->replaceInstructions({fixture->mirArena().makeInstruction(mir::MirOpcode::Add, {})});
    else if (variant == 7)
        function->nodes().back()->addOutgoing(function->edges().front());
    else if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            function->nodes().front()->addOutgoing(function->edges().front());
    CfgVerifier verifier;
    if ((variant >= 3 && variant <= 5) || variant >= 7)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-023 malformed terminators and wrong edge cardinality are rejected");
        return;
    }
    c.expect(verifier.verify(module),
        "CF-CFG-023 jump, branch, return, and unreachable correspondence is exact");
}

void traversalMetadataContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeBranchGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    if (variant == 3) function->setReversePostOrder({});
    else if (variant == 4)
    {
        std::vector<CfgNode*> reversed = function->reversePostOrder();
        std::reverse(reversed.begin(), reversed.end());
        function->setReversePostOrder(std::move(reversed));
    }
    else if (variant == 5) function->nodes().front()->setReachable(false);
    else if (variant == 6) function->nodes().back()->setReachable(false);
    else if (variant == 8)
        function->setReversePostOrder(std::vector<CfgNode*>(4096, function->entryNode()));
    CfgVerifier verifier;
    if ((variant >= 3 && variant <= 6) || variant == 8)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-024 stale order, omission, repetition, and flag mismatch are rejected");
        return;
    }
    c.expect(verifier.verify(module) &&
            function->reversePostOrder() == CfgTraversal::reversePostOrder(function->entryNode()),
        "CF-CFG-024 stored RPO and reachability flags equal iterative recomputation");
}

void exitCollectionsContract(TestContext& c, std::size_t variant)
{
    auto fixture = makeBranchGraph();
    CfgModule* module = fixture->cfgModule();
    CfgFunction* function = module->functions().front();
    if (variant == 3) function->addExitNode(function->entryNode());
    else if (variant == 4) function->addReachableExitNode(function->entryNode());
    else if (variant == 5) function->addExitNode(function->exitNodes().front());
    else if (variant == 6) function->addReachableExitNode(function->reachableExitNodes().front());
    else if (variant == 8)
        for (std::size_t index = 0; index < 4096; ++index)
            function->addExitNode(function->entryNode());
    CfgVerifier verifier;
    if ((variant >= 3 && variant <= 6) || variant == 8)
    {
        c.expect(!verifier.verify(module) && !verifier.error().empty(),
            "CF-CFG-025 missing, extra, repeated, reordered, and wrongly reachable exits fail");
        return;
    }
    c.expect(verifier.verify(module) && function->exitNodes() == function->reachableExitNodes(),
        "CF-CFG-025 exit sequences equal nodes without outgoing edges in node order");
}

void verifierResetContract(TestContext& c, std::size_t variant)
{
    auto valid = makeLinearGraph();
    CfgVerifier verifier;
    if (variant == 1)
    {
        c.expect(verifier.verify(valid->cfgModule()) && verifier.error().empty(),
            "TF-CFG-201 fresh verifier accepts a valid module with no error");
        return;
    }
    verifier.verify(nullptr);
    const std::string invalidError = verifier.error();
    const bool recovered = verifier.verify(valid->cfgModule());
    if (variant >= 4)
    {
        valid->cfgModule()->functions().front()->setReversePostOrder({});
        verifier.verify(valid->cfgModule());
    }
    c.expect(!invalidError.empty() && recovered &&
            (variant < 4 ? verifier.error().empty() : !verifier.error().empty()),
        "CF-CFG-026 each verify call resets prior state and retains only its current first failure");
}
}

void registerCfgVerifierModule(TestRegistry& registry)
{
    addCfgContract(registry, 19, "component", moduleParityContract);
    addCfgContract(registry, 20, "component", functionIdentityContract);
    addCfgContract(registry, 21, "component", nodeRegistrationContract);
    addCfgContract(registry, 22, "component", edgeRegistrationContract);
    addCfgContract(registry, 23, "component", terminatorContract);
    addCfgContract(registry, 24, "component", traversalMetadataContract);
    addCfgContract(registry, 25, "component", exitCollectionsContract);
    addCfgContract(registry, 26, "unit", verifierResetContract);
}
}
