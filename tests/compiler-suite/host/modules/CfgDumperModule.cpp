#include "CfgTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::cfg;
using namespace cfg_support;

void moduleDumpContract(TestContext& c, std::size_t variant)
{
    if (variant == 3 || variant == 4)
    {
        c.expect(dump(nullptr) == "cfg.module\n<null>\n",
            "CF-CFG-016 null CFG module has the exact explicit dump marker");
        return;
    }
    GraphFixture fixture;
    CfgModule* module = fixture.makeManualModule();
    if (variant != 1 && variant != 5) module->addFunction(nullptr);
    const std::string first = dump(module);
    const std::string second = dump(module);
    const std::string expected = variant == 1 || variant == 5
        ? "cfg.module\n" : "cfg.module\nfunction <invalid>\n";
    c.expect(first == expected && second == first,
        "CF-CFG-016 module header, function sequence, invalid marker, and repetition are byte exact");
}

void functionDumpContract(TestContext& c, std::size_t variant)
{
    if (variant == 3 || variant == 4)
    {
        GraphFixture fixture;
        CfgModule* module = fixture.makeManualModule();
        module->addFunction(nullptr);
        c.expect(dump(module) == "cfg.module\nfunction <invalid>\n",
            "CF-CFG-017 invalid function has the exact explicit marker");
        return;
    }
    auto fixture = variant == 1 ? makeLinearGraph() : makeBranchGraph();
    const std::string text = dump(fixture->cfgModule());
    CfgFunction* function = fixture->cfgModule()->functions().front();
    const std::string counts = "  nodes: " + std::to_string(function->nodes().size()) +
        " edges: " + std::to_string(function->edges().size()) +
        " reachable: " + std::to_string(function->reversePostOrder().size());
    c.expect(text.starts_with("cfg.module\nfunction @Main\n") &&
            text.find(counts) != std::string::npos && text.find("  entry: bb") != std::string::npos &&
            text.find("  rpo:") != std::string::npos && text.find("  exits:") != std::string::npos &&
            text.find("  reachable-exits:") != std::string::npos,
        "CF-CFG-017 function summary and ordered sets use the exact stored graph state");
}

void nodeDumpContract(TestContext& c, std::size_t variant)
{
    GraphFixture fixture;
    CfgModule* module = fixture.makeManualModule();
    CfgFunction* function = fixture.makeManualFunction();
    module->addFunction(function);
    if (variant == 3)
    {
        function->addNode(nullptr);
        c.expect(dump(module).find("<invalid-node>\n") != std::string::npos,
            "TF-CFG-139 invalid node has an explicit byte-exact marker");
        return;
    }
    auto* exitBlock = fixture.addReturnBlock("exit");
    auto* entryBlock = fixture.addJumpBlock("entry", exitBlock);
    fixture.function()->replaceBlocks({entryBlock, exitBlock});
    CfgNode* entry = fixture.makeNode(entryBlock);
    CfgNode* exit = fixture.makeNode(exitBlock);
    function->addNode(entry);
    function->addNode(exit);
    CfgEdge* edge = fixture.makeEdge(entry, exit, CfgEdgeKind::Jump);
    function->addEdge(edge);
    entry->addOutgoing(variant == 4 ? nullptr : edge);
    exit->addIncoming(edge);
    entry->setReachable(true);
    exit->setReachable(true);
    function->setReversePostOrder({entry, exit});
    function->addExitNode(exit);
    function->addReachableExitNode(exit);
    const std::string text = dump(module);
    if (variant == 4)
        c.expect(text.find("  succ: <null-edge>\n") != std::string::npos,
            "TF-CFG-140 null edge has an explicit byte-exact marker");
    else
        c.expect(text.find(" entry [reachable]\n") != std::string::npos &&
                text.find("  pred: -\n") != std::string::npos &&
                text.find("  succ: jump->bb") != std::string::npos &&
                text.find(" exit [reachable]\n") != std::string::npos,
            "CF-CFG-018 node adjacency and edge formatting preserve exact order and state");
}
}

void registerCfgDumperModule(TestRegistry& registry)
{
    addCfgContract(registry, 16, "unit", moduleDumpContract);
    addCfgContract(registry, 17, "unit", functionDumpContract);
    addCfgContract(registry, 18, "unit", nodeDumpContract);
}
}
