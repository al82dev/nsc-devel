#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceDumper.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Sema/TypeKind.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/Sema/Symbol.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::cfg::CfgFunction;
using nativesharp::cfg::CfgNode;
using nativesharp::dominance::DominanceFunction;
using nativesharp::dominance::DominanceModule;
using nativesharp::dominance::DominatorTree;
using nativesharp::frontend::FrontendCompilation;

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("dominance_test.ns"), std::string(source), false);
}

const CfgFunction* findCfgFunction(const FrontendCompilation& compilation, std::string_view name)
{
    if (compilation.cfgModule() == nullptr)
    {
        return nullptr;
    }
    for (const CfgFunction* function : compilation.cfgModule()->functions())
    {
        if (function != nullptr && function->mirFunction() != nullptr &&
            function->mirFunction()->symbol() != nullptr &&
            function->mirFunction()->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

const DominanceFunction* findDominanceFunction(
    const FrontendCompilation& compilation,
    std::string_view name)
{
    const CfgFunction* cfgFunction = findCfgFunction(compilation, name);
    return compilation.dominanceModule() == nullptr ? nullptr :
        compilation.dominanceModule()->findFunction(cfgFunction);
}

const CfgNode* findNode(const CfgFunction* function, std::string_view prefix)
{
    if (function == nullptr)
    {
        return nullptr;
    }
    for (const CfgNode* node : function->nodes())
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

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { return 42; }", compilation),
            "single-block function builds through dominance analysis");
        const DominanceFunction* function = findDominanceFunction(compilation, "Main");
        tests.expect(function != nullptr, "defined CFG function receives dominance analysis");
        const DominatorTree* tree = function == nullptr ? nullptr : function->dominatorTree();
        tests.expect(tree != nullptr && tree->nodes().size() == 1, "single-block function has one tree node");
        tests.expect(tree != nullptr && tree->root() != nullptr &&
                tree->root()->immediateDominator() == nullptr,
            "entry is the root and has no immediate dominator");
        tests.expect(tree != nullptr && tree->dominates(tree->root()->cfgNode(), tree->root()->cfgNode()),
            "dominance is reflexive for reachable nodes");
        tests.expect(function != nullptr && function->dominanceFrontier() != nullptr &&
                function->dominanceFrontier()->frontier(tree->root()->cfgNode()).empty(),
            "single return block has empty dominance frontier");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int x = 0; if (x == 0) { x = 1; } else { x = 2; } return x; }";
        tests.expect(compile(source, compilation), "diamond control flow builds dominance information");
        const CfgFunction* cfgFunction = findCfgFunction(compilation, "Main");
        const DominanceFunction* function = findDominanceFunction(compilation, "Main");
        const DominatorTree* tree = function == nullptr ? nullptr : function->dominatorTree();
        const CfgNode* entry = findNode(cfgFunction, "entry");
        const CfgNode* thenNode = findNode(cfgFunction, "if.then");
        const CfgNode* elseNode = findNode(cfgFunction, "if.else");
        const CfgNode* merge = findNode(cfgFunction, "if.end");
        tests.expect(tree != nullptr && tree->immediateDominator(thenNode) == entry,
            "diamond then block is immediately dominated by entry");
        tests.expect(tree != nullptr && tree->immediateDominator(elseNode) == entry,
            "diamond else block is immediately dominated by entry");
        tests.expect(tree != nullptr && tree->immediateDominator(merge) == entry,
            "diamond merge immediate dominator is entry");
        tests.expect(tree != nullptr && tree->dominates(entry, merge), "entry dominates diamond merge");
        tests.expect(tree != nullptr && !tree->dominates(thenNode, merge),
            "one arm does not dominate the merge reached by both arms");
        tests.expect(tree != nullptr && tree->nearestCommonDominator(thenNode, elseNode) == entry,
            "two diamond arms have entry as nearest common dominator");
        tests.expect(function != nullptr && function->dominanceFrontier()->contains(thenNode, merge),
            "then block has merge in its dominance frontier");
        tests.expect(function != nullptr && function->dominanceFrontier()->contains(elseNode, merge),
            "else block has merge in its dominance frontier");
        const std::vector<const CfgNode*> mergeDominators = tree->dominators(merge);
        tests.expect(mergeDominators.size() == 2 && mergeDominators[0] == entry &&
                mergeDominators[1] == merge,
            "dominator chain is materialized root-to-node");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int Main() { int i = 0; while (i < 3) { i++; } return i; }";
        tests.expect(compile(source, compilation), "loop source builds dominance information");
        const CfgFunction* cfgFunction = findCfgFunction(compilation, "Main");
        const DominanceFunction* function = findDominanceFunction(compilation, "Main");
        const DominatorTree* tree = function == nullptr ? nullptr : function->dominatorTree();
        const CfgNode* entry = findNode(cfgFunction, "entry");
        const CfgNode* condition = findNode(cfgFunction, "while.cond");
        const CfgNode* body = findNode(cfgFunction, "while.body");
        const CfgNode* exit = findNode(cfgFunction, "while.end");
        tests.expect(tree != nullptr && tree->immediateDominator(condition) == entry,
            "while condition is immediately dominated by preheader entry");
        tests.expect(tree != nullptr && tree->immediateDominator(body) == condition,
            "while body is immediately dominated by condition");
        tests.expect(tree != nullptr && tree->immediateDominator(exit) == condition,
            "while exit is immediately dominated by condition");
        tests.expect(function != nullptr && function->dominanceFrontier()->contains(condition, condition),
            "loop header may belong to its own dominance frontier");
        tests.expect(function != nullptr && function->dominanceFrontier()->contains(body, condition),
            "loop body frontier contains loop header because of back edge");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { return 1; int x = 2; x++; }", compilation),
            "unreachable post-return block remains in CFG");
        const CfgFunction* cfgFunction = findCfgFunction(compilation, "Main");
        const DominanceFunction* function = findDominanceFunction(compilation, "Main");
        const DominatorTree* tree = function == nullptr ? nullptr : function->dominatorTree();
        bool foundExcluded = false;
        for (const CfgNode* node : cfgFunction->nodes())
        {
            if (!node->reachable())
            {
                foundExcluded = true;
                tests.expect(tree->findNode(node) == nullptr,
                    "unreachable CFG block is excluded from dominator tree");
                tests.expect(!tree->dominates(cfgFunction->entryNode(), node),
                    "dominance query returns false outside reachable analysis domain");
                tests.expect(function->dominanceFrontier()->frontier(node).empty(),
                    "unreachable block has no stored dominance frontier");
            }
        }
        tests.expect(foundExcluded, "test source contains an unreachable CFG block");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int F(int x); int Main() { return 0; }", compilation),
            "prototype plus definition builds through dominance stage");
        tests.expect(compilation.dominanceModule() != nullptr &&
                compilation.dominanceModule()->functions().size() == compilation.cfgModule()->functions().size(),
            "dominance module mirrors CFG functions rather than MIR prototypes");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile(
            "int Main() { int x = 0; if (x == 0) { x = 1; } else { x = 2; } return x; }",
            compilation),
            "dominance dump fixture compiles");
        std::ostringstream output;
        nativesharp::dominance::DominanceDumper dumper;
        dumper.dump(compilation.dominanceModule(), output);
        tests.expect(output.str().find("dominance.module") != std::string::npos,
            "dominance dumper emits module heading");
        tests.expect(output.str().find("idom=") != std::string::npos &&
                output.str().find("df={") != std::string::npos,
            "dominance dump exposes immediate dominators and frontiers");
        tests.expect(output.str().find("tree:") != std::string::npos,
            "dominance dump renders dominator-tree hierarchy");
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int i = 0; while (i < 2) { i++; } return i; }", compilation),
            "dominance verifier fixture compiles");
        nativesharp::dominance::DominanceVerifier verifier;
        tests.expect(verifier.verify(compilation.dominanceModule()),
            "independent fixed-point verifier accepts builder output");
    }


    {
        nativesharp::sema::TypeContext types;
        nativesharp::sema::Type* functionType =
            types.functionType(types.builtin(nativesharp::sema::TypeKind::Void), {});
        nativesharp::sema::Symbol symbol(
            nativesharp::sema::SymbolKind::Function, "Irreducible", functionType, nullptr);
        nativesharp::mir::MirArena mirArena;
        nativesharp::mir::MirModule* mirModule = mirArena.makeModule();
        nativesharp::mir::MirFunction* mirFunction = mirArena.makeFunction(&symbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);

        nativesharp::mir::MirBlock* entry = mirArena.makeBlock("entry");
        nativesharp::mir::MirBlock* a = mirArena.makeBlock("a");
        nativesharp::mir::MirBlock* b = mirArena.makeBlock("b");
        nativesharp::mir::MirBlock* exit = mirArena.makeBlock("exit");
        mirFunction->addBlock(entry);
        mirFunction->addBlock(a);
        mirFunction->addBlock(b);
        mirFunction->addBlock(exit);

        auto* entryBranch = mirArena.makeInstruction(
            nativesharp::mir::MirOpcode::Branch, nativesharp::SourceRange());
        entryBranch->setFirstTarget(a);
        entryBranch->setSecondTarget(b);
        entry->addInstruction(entryBranch);
        auto* aJump = mirArena.makeInstruction(
            nativesharp::mir::MirOpcode::Jump, nativesharp::SourceRange());
        aJump->setFirstTarget(b);
        a->addInstruction(aJump);
        auto* bBranch = mirArena.makeInstruction(
            nativesharp::mir::MirOpcode::Branch, nativesharp::SourceRange());
        bBranch->setFirstTarget(a);
        bBranch->setSecondTarget(exit);
        b->addInstruction(bBranch);
        exit->addInstruction(mirArena.makeInstruction(
            nativesharp::mir::MirOpcode::Return, nativesharp::SourceRange()));

        nativesharp::cfg::CfgArena cfgArena;
        nativesharp::cfg::CfgBuilder cfgBuilder(cfgArena);
        nativesharp::cfg::CfgModule* cfgModule = cfgBuilder.build(mirModule);
        tests.expect(cfgModule != nullptr, "irreducible CFG fixture is constructed from MIR terminators");

        nativesharp::dominance::DominanceArena dominanceArena;
        nativesharp::dominance::DominanceBuilder dominanceBuilder(dominanceArena);
        DominanceModule* module = dominanceBuilder.build(cfgModule);
        tests.expect(module != nullptr, "dominance algorithm accepts irreducible reachable CFG");
        nativesharp::dominance::DominanceVerifier verifier;
        tests.expect(module != nullptr && verifier.verify(module),
            "fixed-point verifier confirms dominance for irreducible CFG");

        const CfgFunction* graph = cfgModule->functions().front();
        const CfgNode* entryNode = graph->findNode(entry);
        const CfgNode* aNode = graph->findNode(a);
        const CfgNode* bNode = graph->findNode(b);
        const CfgNode* exitNode = graph->findNode(exit);
        const DominanceFunction* function = module->findFunction(graph);
        const DominatorTree* tree = function->dominatorTree();
        tests.expect(tree->immediateDominator(aNode) == entryNode,
            "first irreducible SCC entry is immediately dominated by function entry");
        tests.expect(tree->immediateDominator(bNode) == entryNode,
            "second irreducible SCC entry is immediately dominated by function entry");
        tests.expect(tree->immediateDominator(exitNode) == bNode,
            "irreducible region exit is immediately dominated by its mandatory predecessor");
        tests.expect(function->dominanceFrontier()->contains(aNode, bNode),
            "first irreducible SCC node has the other entry in its frontier");
        tests.expect(function->dominanceFrontier()->contains(bNode, aNode),
            "second irreducible SCC node has the first entry in its frontier");
    }

    return tests.result();
}
