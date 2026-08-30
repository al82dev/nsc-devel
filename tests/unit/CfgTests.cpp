#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgDumper.h"
#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgEdgeKind.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgVerifier.h"
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
#include "../support/TestContext.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using nativesharp::cfg::CfgEdge;
using nativesharp::cfg::CfgEdgeKind;
using nativesharp::cfg::CfgFunction;
using nativesharp::cfg::CfgModule;
using nativesharp::cfg::CfgNode;
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;

namespace
{
bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("cfg_test.ns"), std::string(source), false);
}

const CfgFunction* findFunction(const CfgModule* module, std::string_view name)
{
    if (module == nullptr)
    {
        return nullptr;
    }
    for (const CfgFunction* function : module->functions())
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

bool hasSuccessor(const CfgNode* source, const CfgNode* target)
{
    if (source == nullptr || target == nullptr)
    {
        return false;
    }
    const std::vector<CfgNode*> successors = source->successors();
    for (const CfgNode* successor : successors)
    {
        if (successor == target)
        {
            return true;
        }
    }
    return false;
}

bool hasPredecessor(const CfgNode* target, const CfgNode* source)
{
    if (source == nullptr || target == nullptr)
    {
        return false;
    }
    const std::vector<CfgNode*> predecessors = target->predecessors();
    for (const CfgNode* predecessor : predecessors)
    {
        if (predecessor == source)
        {
            return true;
        }
    }
    return false;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { return 42; }", compilation),
            "single-block function builds through CFG");
        const CfgFunction* function = findFunction(compilation.cfgModule(), "Main");
        tests.expect(function != nullptr, "defined MIR function owns a CFG function");
        tests.expect(function != nullptr && function->nodes().size() == 1,
            "single-block function produces one CFG node");
        tests.expect(function != nullptr && function->edges().empty(),
            "return-only function produces no CFG edges");
        tests.expect(function != nullptr && function->entryNode() != nullptr &&
                function->entryNode()->reachable(),
            "entry CFG node is reachable");
        tests.expect(function != nullptr && function->reversePostOrder().size() == 1,
            "single-node CFG has one-node reverse postorder");
        tests.expect(function != nullptr && function->exitNodes().size() == 1 &&
                function->reachableExitNodes().size() == 1,
            "return block is both an exit and a reachable exit");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int x = 0; if (x == 0) { x = 1; } else { x = 2; } return x; }";
        tests.expect(compile(source, compilation), "if/else source builds an explicit CFG");
        const CfgFunction* function = findFunction(compilation.cfgModule(), "Main");
        const CfgNode* entry = findNode(function, "entry");
        const CfgNode* thenNode = findNode(function, "if.then");
        const CfgNode* elseNode = findNode(function, "if.else");
        const CfgNode* mergeNode = findNode(function, "if.end");
        tests.expect(entry != nullptr && entry->outgoingEdges().size() == 2,
            "if condition node has two outgoing control-flow edges");
        if (entry != nullptr && entry->outgoingEdges().size() == 2)
        {
            tests.expect(entry->outgoingEdges()[0]->kind() == CfgEdgeKind::BranchTrue,
                "first branch edge preserves true-edge identity");
            tests.expect(entry->outgoingEdges()[1]->kind() == CfgEdgeKind::BranchFalse,
                "second branch edge preserves false-edge identity");
        }
        tests.expect(hasSuccessor(entry, thenNode) && hasSuccessor(entry, elseNode),
            "if entry has then and else successor nodes");
        tests.expect(hasPredecessor(mergeNode, thenNode) && hasPredecessor(mergeNode, elseNode),
            "merge node records both branch predecessors");
        tests.expect(mergeNode != nullptr && mergeNode->predecessors().size() == 2,
            "merge predecessor list is block-unique");
    }

    {
        FrontendCompilation compilation;
        const char* source =
            "int Main() { int i = 0; while (i < 3) { i++; } return i; }";
        tests.expect(compile(source, compilation), "while loop builds through CFG");
        const CfgFunction* function = findFunction(compilation.cfgModule(), "Main");
        const CfgNode* condition = findNode(function, "while.cond");
        const CfgNode* body = findNode(function, "while.body");
        tests.expect(hasSuccessor(body, condition), "while body has a back edge to its condition");
        tests.expect(hasPredecessor(condition, body), "while condition records loop-body predecessor");
        tests.expect(condition != nullptr && condition->predecessors().size() == 2,
            "while condition has entry and back-edge predecessors");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int Main() { return 1; int x = 2; x++; }";
        tests.expect(compile(source, compilation), "source after return remains representable in CFG");
        const CfgFunction* function = findFunction(compilation.cfgModule(), "Main");
        bool foundUnreachable = false;
        for (const CfgNode* node : function->nodes())
        {
            if (!node->reachable())
            {
                foundUnreachable = true;
                tests.expect(node->incomingEdges().empty(),
                    "disconnected post-return block has no incoming CFG edges");
            }
        }
        tests.expect(foundUnreachable, "CFG marks disconnected post-return MIR block unreachable");
        tests.expect(function->reversePostOrder().size() < function->nodes().size(),
            "reverse postorder contains only reachable nodes");
        tests.expect(function->exitNodes().size() > function->reachableExitNodes().size(),
            "all exits include disconnected sinks while reachable exits exclude them");
    }

    {
        FrontendCompilation compilation;
        const char* source = "int F(int x); int Main() { return 0; }";
        tests.expect(compile(source, compilation), "prototype plus definition-free declaration builds CFG module");
        tests.expect(compilation.mirModule() != nullptr && compilation.cfgModule() != nullptr,
            "MIR and CFG modules are both available");
        if (compilation.mirModule() != nullptr && compilation.cfgModule() != nullptr)
        {
            int prototypes = 0;
            for (const MirFunction* function : compilation.mirModule()->functions())
            {
                if (function != nullptr && !function->defined())
                {
                    ++prototypes;
                    tests.expect(compilation.cfgModule()->findFunction(function) == nullptr,
                        "MIR prototype deliberately has no CFG function");
                }
            }
            tests.expect(prototypes == 1, "test source contributes one MIR prototype");
        }
    }

    {
        FrontendCompilation compilation;
        tests.expect(compile("int Main() { int x = 0; if (x == 0) { x = 1; } return x; }", compilation),
            "CFG dump sample compiles");
        std::ostringstream output;
        nativesharp::cfg::CfgDumper dumper;
        dumper.dump(compilation.cfgModule(), output);
        tests.expect(output.str().find("cfg.module") != std::string::npos,
            "CFG dumper emits module heading");
        tests.expect(output.str().find("pred:") != std::string::npos &&
                output.str().find("succ:") != std::string::npos,
            "CFG dumper exposes predecessor and successor information");
        tests.expect(output.str().find("rpo:") != std::string::npos,
            "CFG dumper exposes deterministic reverse postorder");
    }


    {
        nativesharp::sema::TypeContext types;
        nativesharp::sema::Type* functionType =
            types.functionType(types.builtin(nativesharp::sema::TypeKind::Void), {});
        nativesharp::sema::Symbol functionSymbol(
            nativesharp::sema::SymbolKind::Function, "SameTarget", functionType, nullptr);
        nativesharp::mir::MirArena mirArena;
        nativesharp::mir::MirModule* mirModule = mirArena.makeModule();
        nativesharp::mir::MirFunction* mirFunction = mirArena.makeFunction(&functionSymbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);
        nativesharp::mir::MirBlock* entry = mirArena.makeBlock("entry");
        nativesharp::mir::MirBlock* exit = mirArena.makeBlock("exit");
        mirFunction->addBlock(entry);
        mirFunction->addBlock(exit);
        nativesharp::mir::MirInstruction* branch =
            mirArena.makeInstruction(nativesharp::mir::MirOpcode::Branch, nativesharp::SourceRange());
        branch->setFirstTarget(exit);
        branch->setSecondTarget(exit);
        entry->addInstruction(branch);
        exit->addInstruction(
            mirArena.makeInstruction(nativesharp::mir::MirOpcode::Return, nativesharp::SourceRange()));

        nativesharp::cfg::CfgArena cfgArena;
        nativesharp::cfg::CfgBuilder builder(cfgArena);
        CfgModule* cfgModule = builder.build(mirModule);
        const CfgFunction* function = cfgModule == nullptr ? nullptr : cfgModule->functions().front();
        const CfgNode* entryNode = function == nullptr ? nullptr : function->entryNode();
        const CfgNode* exitNode = function == nullptr ? nullptr : function->nodes()[1];
        tests.expect(cfgModule != nullptr, "CFG builder accepts branch whose two edges share one target");
        tests.expect(entryNode != nullptr && entryNode->outgoingEdges().size() == 2,
            "same-target branch preserves two first-class CFG edges");
        tests.expect(entryNode != nullptr && entryNode->successors().size() == 1,
            "successor node view deduplicates parallel true/false branch edges");
        tests.expect(exitNode != nullptr && exitNode->incomingEdges().size() == 2,
            "same target retains both incoming edge identities");
        tests.expect(exitNode != nullptr && exitNode->predecessors().size() == 1,
            "predecessor node view deduplicates parallel edges from one source block");
        nativesharp::cfg::CfgVerifier verifier;
        tests.expect(verifier.verify(cfgModule),
            "CFG verifier accepts parallel true/false edges that share one target block");
    }


    {
        FrontendCompilation compilation;
        tests.expect(compile("extern C int puts(char* text);", compilation),
            "prototype-only translation unit builds an empty CFG module");
        tests.expect(compilation.cfgModule() != nullptr && compilation.cfgModule()->functions().empty(),
            "CFG module deliberately omits functions that have no MIR definition body");
    }

    return tests.result();
}
