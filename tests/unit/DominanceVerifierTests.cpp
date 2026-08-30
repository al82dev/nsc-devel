#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace
{
using nativesharp::cfg::CfgFunction;
using nativesharp::cfg::CfgNode;
using nativesharp::dominance::DominanceArena;
using nativesharp::dominance::DominanceBuilder;
using nativesharp::dominance::DominanceFunction;
using nativesharp::dominance::DominanceModule;
using nativesharp::dominance::DominanceVerifier;
using nativesharp::frontend::FrontendCompilation;

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("dominance_verifier.ns"), std::string(source), false);
}

const CfgFunction* mainCfg(const FrontendCompilation& compilation)
{
    return compilation.cfgModule() == nullptr || compilation.cfgModule()->functions().empty() ? nullptr :
        compilation.cfgModule()->functions().front();
}

CfgNode* findNode(const CfgFunction* function, std::string_view prefix)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (CfgNode* node : function->nodes())
    {
        if (node != nullptr && node->block() != nullptr && node->block()->name().starts_with(prefix))
        {
            return node;
        }
    }
    return nullptr;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    FrontendCompilation compilation;
    tests.expect(compile(
        "int Main() { int x = 0; if (x == 0) { x = 1; } else { x = 2; } return x; }",
        compilation),
        "verifier base fixture compiles");
    const CfgFunction* cfgFunction = mainCfg(compilation);

    {
        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(compilation.cfgModule());
        DominanceVerifier verifier;
        tests.expect(module != nullptr && verifier.verify(module), "fresh dominance analysis verifies");
    }

    {
        DominanceArena arena;
        DominanceModule* module = arena.makeModule(compilation.cfgModule());
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects missing per-function analysis");
        tests.expect(verifier.error().find("exactly one analysis") != std::string::npos,
            "missing-function test fails for intended module-coverage invariant");
    }

    {
        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(compilation.cfgModule());
        DominanceFunction* function = module->findFunction(cfgFunction);
        module->addFunction(function);
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects duplicate dominance function registration");
        tests.expect(verifier.error().find("exactly one analysis") != std::string::npos ||
                verifier.error().find("duplicate") != std::string::npos,
            "duplicate function test fails for intended coverage invariant");
    }

    {
        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(compilation.cfgModule());
        DominanceFunction* function = module->findFunction(cfgFunction);
        CfgNode* thenNode = findNode(cfgFunction, "if.then");
        auto* treeNode = function->dominatorTree()->findNode(thenNode);
        treeNode->setImmediateDominator(nullptr);
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects corrupted immediate dominator");
        tests.expect(verifier.error().find("immediate dominator") != std::string::npos,
            "idom corruption test fails for intended invariant");
    }

    {
        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(compilation.cfgModule());
        DominanceFunction* function = module->findFunction(cfgFunction);
        CfgNode* thenNode = findNode(cfgFunction, "if.then");
        auto* treeNode = function->dominatorTree()->findNode(thenNode);
        const std::uint32_t rootPre = function->dominatorTree()->root()->preorderNumber();
        const std::uint32_t rootPost = function->dominatorTree()->root()->postorderNumber();
        treeNode->setTraversalNumbers(rootPre, rootPost);
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects duplicate/corrupt traversal interval");
        tests.expect(verifier.error().find("traversal") != std::string::npos,
            "interval corruption test fails for intended traversal invariant");
    }

    {
        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(compilation.cfgModule());
        DominanceFunction* function = module->findFunction(cfgFunction);
        CfgNode* thenNode = findNode(cfgFunction, "if.then");
        function->dominanceFrontier()->setFrontier(thenNode, {});
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects corrupted dominance frontier");
        tests.expect(verifier.error().find("dominance frontier") != std::string::npos,
            "frontier corruption test fails for intended graph-theoretic invariant");
    }

    {
        FrontendCompilation unreachableCompilation;
        tests.expect(compile("int Main() { return 1; int x = 2; x++; }", unreachableCompilation),
            "unreachable-block verifier fixture compiles");
        const CfgFunction* unreachableCfg = mainCfg(unreachableCompilation);
        CfgNode* unreachableNode = nullptr;
        for (CfgNode* node : unreachableCfg->nodes())
        {
            if (!node->reachable())
            {
                unreachableNode = node;
                break;
            }
        }

        DominanceArena arena;
        DominanceBuilder builder(arena);
        DominanceModule* module = builder.build(unreachableCompilation.cfgModule());
        DominanceFunction* function = module->findFunction(unreachableCfg);
        auto* fake = arena.makeTreeNode(unreachableNode);
        function->dominatorTree()->addNode(fake);
        DominanceVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects unreachable node inserted into dominator tree");
        tests.expect(verifier.error().find("reachable CFG nodes") != std::string::npos ||
                verifier.error().find("unreachable") != std::string::npos,
            "unreachable-tree-node test fails for intended analysis-domain invariant");
    }

    return tests.result();
}
