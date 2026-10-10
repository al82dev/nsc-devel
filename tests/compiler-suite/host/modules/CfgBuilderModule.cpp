#include "CfgTestSupport.h"

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

void builderSelectionContract(TestContext& c, std::size_t variant)
{
    if (variant == 3)
    {
        CfgArena arena;
        CfgBuilder builder(arena);
        c.expect(builder.build(nullptr) == nullptr && !builder.succeeded() &&
                builder.error() == "CFG builder requires a MIR module",
            "CF-CFG-010 null module fails with the exact first diagnostic");
        return;
    }
    GraphFixture fixture;
    if (variant == 4) fixture.module()->addFunction(nullptr);
    if (variant == 5) fixture.function()->setDefined(false);
    if (variant != 5) fixture.addReturnBlock("entry");
    CfgModule* module = fixture.build();
    if (variant == 4)
    {
        c.expect(false && module != nullptr,
            "TF-CFG-076 PRODUCT-DEFECT: null MIR function entries are silently ignored instead of rejected");
        return;
    }
    const std::size_t expected = variant == 5 ? 0U : 1U;
    c.expect(module != nullptr && module->functions().size() == expected,
        "CF-CFG-010 builder selects defined functions only and preserves module order");
}

void builderFunctionContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    if (variant == 3)
    {
        fixture.function()->addBlock(nullptr);
    }
    else if (variant != 4)
    {
        fixture.addReturnBlock("first");
        if (variant != 1) fixture.addUnreachableBlock("second");
    }
    CfgModule* module = fixture.build();
    if (variant == 3 || variant == 4)
    {
        c.expect(module == nullptr && !fixture.builder()->succeeded() && !fixture.builder()->error().empty(),
            "CF-CFG-011 invalid null-block or absent-entry function fails before publication completes");
        return;
    }
    CfgFunction* function = module->functions().front();
    c.expect(function->nodes().size() == fixture.function()->blocks().size() &&
            function->entryNode()->block() == fixture.function()->entryBlock() &&
            function->findNode(fixture.function()->blocks().front()) == function->nodes().front(),
        "CF-CFG-011 builder creates one ordered node per MIR block and the first is entry");
}

void builderTerminatorContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    if (variant == 1) fixture.addReturnBlock("return");
    else if (variant == 2) fixture.addUnreachableBlock("unreachable");
    else if (variant == 3)
    {
        auto* block = fixture.mirArena().makeBlock("empty");
        fixture.function()->addBlock(block);
    }
    else if (variant == 4)
    {
        auto* block = fixture.mirArena().makeBlock("null");
        block->addInstruction(nullptr);
        fixture.function()->addBlock(block);
    }
    else if (variant == 5)
    {
        auto* block = fixture.mirArena().makeBlock("ordinary");
        block->addInstruction(fixture.mirArena().makeInstruction(mir::MirOpcode::Add, {}));
        fixture.function()->addBlock(block);
    }
    else if (variant == 6)
    {
        auto* exit = fixture.addReturnBlock("exit");
        auto* entry = fixture.addJumpBlock("entry", exit);
        fixture.function()->replaceBlocks({entry, exit});
    }
    else
    {
        auto* left = fixture.addReturnBlock("left");
        auto* right = fixture.addReturnBlock("right");
        auto* entry = fixture.addBranchBlock("entry", left, right);
        fixture.function()->replaceBlocks({entry, left, right});
    }
    CfgModule* module = fixture.build();
    if (variant >= 3 && variant <= 5)
    {
        c.expect(module == nullptr && !fixture.builder()->error().empty(),
            "CF-CFG-012 empty, null, and non-control terminators fail with a stable first error");
        return;
    }
    CfgFunction* function = module->functions().front();
    const std::size_t expectedEdges = variant == 6 ? 1U : variant >= 7 ? 2U : 0U;
    c.expect(function->edges().size() == expectedEdges,
        "CF-CFG-012 return/unreachable/jump/branch map to exact outgoing edge cardinality");
}

void builderEdgeContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    auto* exit = fixture.addReturnBlock("exit");
    mir::MirBlock* entry = nullptr;
    if (variant == 3) entry = fixture.addJumpBlock("entry", nullptr);
    else if (variant == 4)
    {
        auto* foreign = fixture.mirArena().makeBlock("foreign");
        foreign->addInstruction(fixture.mirArena().makeInstruction(mir::MirOpcode::Return, {}));
        entry = fixture.addJumpBlock("entry", foreign);
    }
    else entry = fixture.addJumpBlock("entry", exit);
    fixture.function()->replaceBlocks({entry, exit});
    CfgModule* module = fixture.build();
    if (variant == 3 || variant == 4)
    {
        c.expect(module == nullptr && !fixture.builder()->error().empty(),
            "CF-CFG-013 null or foreign targets fail before edge publication");
        return;
    }
    CfgFunction* function = module->functions().front();
    CfgEdge* edge = function->edges().front();
    c.expect(function->edges().size() == 1 && function->nodes()[0]->outgoingEdges()[0] == edge &&
            function->nodes()[1]->incomingEdges()[0] == edge,
        "CF-CFG-013 one edge is registered once globally and at both endpoints");
}

void builderTraversalContract(TestContext& c, std::size_t variant)
{
    auto fixture = variant == 1 ? makeLinearGraph() : makeBranchGraph();
    CfgFunction* function = fixture->cfgModule()->functions().front();
    if (variant == 3) function->nodes().front()->setReachable(false);
    if (variant == 4) function->setReversePostOrder({});
    if (variant == 5) function->addExitNode(function->entryNode());
    CfgVerifier verifier;
    if (variant >= 3 && variant <= 5)
    {
        c.expect(!verifier.verify(fixture->cfgModule()) && !verifier.error().empty(),
            "CF-CFG-014 verifier rejects stale traversal flags, RPO, and exits");
        return;
    }
    c.expect(verifier.verify(fixture->cfgModule()) &&
            function->reversePostOrder() == CfgTraversal::reversePostOrder(function->entryNode()) &&
            !function->exitNodes().empty() && function->exitNodes() == function->reachableExitNodes(),
        "CF-CFG-014 builder traversal, reachability, and exit collections agree exactly");
}

void builderErrorLatchContract(TestContext& c, std::size_t variant)
{
    CfgArena arena;
    CfgBuilder builder(arena);
    if (variant == 1)
    {
        GraphFixture valid;
        valid.addReturnBlock("entry");
        c.expect(valid.build() != nullptr && valid.builder()->succeeded() && valid.builder()->error().empty(),
            "TF-CFG-113 successful builder state has no error");
        return;
    }
    builder.build(nullptr);
    const std::string first = builder.error();
    if (variant >= 2) builder.build(nullptr);
    c.expect(!builder.succeeded() && builder.error() == first &&
            first == "CFG builder requires a MIR module",
        "CF-CFG-015 builder preserves the first error across later failures and reuse");
}
}

void registerCfgBuilderModule(TestRegistry& registry)
{
    addCfgContract(registry, 10, "component", builderSelectionContract, {}, {4});
    addCfgContract(registry, 11, "component", builderFunctionContract);
    addCfgContract(registry, 12, "component", builderTerminatorContract);
    addCfgContract(registry, 13, "component", builderEdgeContract);
    addCfgContract(registry, 14, "component", builderTraversalContract);
    addCfgContract(registry, 15, "unit", builderErrorLatchContract);
}
}
