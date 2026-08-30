#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"
#include "../support/TestContext.h"

#include <vector>

using nativesharp::SourceRange;
using nativesharp::cfg::CfgArena;
using nativesharp::cfg::CfgEdge;
using nativesharp::cfg::CfgEdgeKind;
using nativesharp::cfg::CfgFunction;
using nativesharp::cfg::CfgModule;
using nativesharp::cfg::CfgNode;
using nativesharp::cfg::CfgVerifier;
using nativesharp::mir::MirArena;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirModule;
using nativesharp::mir::MirOpcode;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;

namespace
{
struct Fixture
{
    Fixture()
        : functionType(types.functionType(types.builtin(TypeKind::Void), {})),
          functionSymbol(SymbolKind::Function, "F", functionType, nullptr)
    {
        mirModule = mirArena.makeModule();
        mirFunction = mirArena.makeFunction(&functionSymbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);
        entry = mirArena.makeBlock("entry");
        mirFunction->addBlock(entry);
        MirInstruction* ret = mirArena.makeInstruction(MirOpcode::Return, SourceRange());
        entry->addInstruction(ret);
    }

    CfgModule* makeValidCfg()
    {
        cfgModule = cfgArena.makeModule(mirModule);
        cfgFunction = cfgArena.makeFunction(mirFunction);
        cfgModule->addFunction(cfgFunction);
        cfgNode = cfgArena.makeNode(entry);
        cfgFunction->addNode(cfgNode);
        cfgNode->setReachable(true);
        cfgFunction->setReversePostOrder({cfgNode});
        cfgFunction->addExitNode(cfgNode);
        cfgFunction->addReachableExitNode(cfgNode);
        return cfgModule;
    }

    TypeContext types;
    Type* functionType;
    Symbol functionSymbol;
    MirArena mirArena;
    MirModule* mirModule = nullptr;
    MirFunction* mirFunction = nullptr;
    MirBlock* entry = nullptr;
    CfgArena cfgArena;
    CfgModule* cfgModule = nullptr;
    CfgFunction* cfgFunction = nullptr;
    CfgNode* cfgNode = nullptr;
};
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    {
        Fixture fixture;
        CfgVerifier verifier;
        tests.expect(verifier.verify(fixture.makeValidCfg()), "verifier accepts a valid one-block CFG");
    }

    {
        Fixture fixture;
        CfgVerifier verifier;
        tests.expect(!verifier.verify(fixture.cfgArena.makeModule(fixture.mirModule)),
            "verifier rejects CFG module missing a defined MIR function graph");
        tests.expect(verifier.error().find("missing its CFG") != std::string::npos,
            "missing-function CFG test fails for the intended invariant");
    }

    {
        Fixture fixture;
        CfgModule* module = fixture.makeValidCfg();
        fixture.cfgNode->setReachable(false);
        CfgVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects stale CFG reachability metadata");
        tests.expect(verifier.error().find("reachable flag") != std::string::npos,
            "reachability negative test fails for the intended invariant");
    }

    {
        Fixture fixture;
        CfgModule* module = fixture.makeValidCfg();
        fixture.cfgFunction->setReversePostOrder({});
        CfgVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects incorrect reverse postorder metadata");
        tests.expect(verifier.error().find("reverse postorder") != std::string::npos,
            "RPO negative test fails for the intended invariant");
    }

    {
        Fixture fixture;
        fixture.cfgModule = fixture.cfgArena.makeModule(fixture.mirModule);
        fixture.cfgFunction = fixture.cfgArena.makeFunction(fixture.mirFunction);
        fixture.cfgModule->addFunction(fixture.cfgFunction);
        CfgNode* wrong = fixture.cfgArena.makeNode(nullptr);
        fixture.cfgFunction->addNode(wrong);
        CfgVerifier verifier;
        tests.expect(!verifier.verify(fixture.cfgModule), "verifier rejects CFG node without MIR block");
    }

    {
        TypeContext types;
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        MirArena mirArena;
        MirModule* mirModule = mirArena.makeModule();
        MirFunction* mirFunction = mirArena.makeFunction(&functionSymbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);
        MirBlock* entry = mirArena.makeBlock("entry");
        MirBlock* target = mirArena.makeBlock("target");
        mirFunction->addBlock(entry);
        mirFunction->addBlock(target);
        MirInstruction* jump = mirArena.makeInstruction(MirOpcode::Jump, SourceRange());
        jump->setFirstTarget(target);
        entry->addInstruction(jump);
        MirInstruction* ret = mirArena.makeInstruction(MirOpcode::Return, SourceRange());
        target->addInstruction(ret);

        CfgArena cfgArena;
        CfgModule* cfgModule = cfgArena.makeModule(mirModule);
        CfgFunction* cfgFunction = cfgArena.makeFunction(mirFunction);
        cfgModule->addFunction(cfgFunction);
        CfgNode* entryNode = cfgArena.makeNode(entry);
        CfgNode* targetNode = cfgArena.makeNode(target);
        cfgFunction->addNode(entryNode);
        cfgFunction->addNode(targetNode);
        CfgEdge* edge = cfgArena.makeEdge(entryNode, targetNode, CfgEdgeKind::Jump);
        cfgFunction->addEdge(edge);
        entryNode->addOutgoing(edge);
        entryNode->setReachable(true);
        targetNode->setReachable(true);
        cfgFunction->setReversePostOrder({entryNode, targetNode});
        cfgFunction->addExitNode(targetNode);
        cfgFunction->addReachableExitNode(targetNode);

        CfgVerifier verifier;
        tests.expect(!verifier.verify(cfgModule),
            "verifier rejects edge not registered in target incoming list");
        tests.expect(verifier.error().find("registered exactly once") != std::string::npos,
            "edge-registration negative test fails for the intended invariant");
    }

    {
        TypeContext types;
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        MirArena mirArena;
        MirModule* mirModule = mirArena.makeModule();
        MirFunction* mirFunction = mirArena.makeFunction(&functionSymbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);
        MirBlock* entry = mirArena.makeBlock("entry");
        MirBlock* trueBlock = mirArena.makeBlock("true");
        MirBlock* falseBlock = mirArena.makeBlock("false");
        mirFunction->addBlock(entry);
        mirFunction->addBlock(trueBlock);
        mirFunction->addBlock(falseBlock);
        MirInstruction* branch = mirArena.makeInstruction(MirOpcode::Branch, SourceRange());
        branch->setFirstTarget(trueBlock);
        branch->setSecondTarget(falseBlock);
        entry->addInstruction(branch);
        trueBlock->addInstruction(mirArena.makeInstruction(MirOpcode::Return, SourceRange()));
        falseBlock->addInstruction(mirArena.makeInstruction(MirOpcode::Return, SourceRange()));

        CfgArena cfgArena;
        CfgModule* cfgModule = cfgArena.makeModule(mirModule);
        CfgFunction* cfgFunction = cfgArena.makeFunction(mirFunction);
        cfgModule->addFunction(cfgFunction);
        CfgNode* entryNode = cfgArena.makeNode(entry);
        CfgNode* trueNode = cfgArena.makeNode(trueBlock);
        CfgNode* falseNode = cfgArena.makeNode(falseBlock);
        cfgFunction->addNode(entryNode);
        cfgFunction->addNode(trueNode);
        cfgFunction->addNode(falseNode);
        CfgEdge* first = cfgArena.makeEdge(entryNode, trueNode, CfgEdgeKind::BranchFalse);
        CfgEdge* second = cfgArena.makeEdge(entryNode, falseNode, CfgEdgeKind::BranchTrue);
        cfgFunction->addEdge(first);
        cfgFunction->addEdge(second);
        entryNode->addOutgoing(first);
        entryNode->addOutgoing(second);
        trueNode->addIncoming(first);
        falseNode->addIncoming(second);
        entryNode->setReachable(true);
        trueNode->setReachable(true);
        falseNode->setReachable(true);
        cfgFunction->setReversePostOrder({entryNode, trueNode, falseNode});
        cfgFunction->addExitNode(trueNode);
        cfgFunction->addExitNode(falseNode);
        cfgFunction->addReachableExitNode(trueNode);
        cfgFunction->addReachableExitNode(falseNode);

        CfgVerifier verifier;
        tests.expect(!verifier.verify(cfgModule),
            "verifier rejects branch edges whose true/false identities do not match MIR targets");
        tests.expect(verifier.error().find("true/false") != std::string::npos,
            "branch-identity negative test fails for the intended invariant");
    }

    {
        Fixture fixture;
        CfgModule* module = fixture.makeValidCfg();
        fixture.cfgFunction->addExitNode(fixture.cfgNode);
        CfgVerifier verifier;
        tests.expect(!verifier.verify(module), "verifier rejects duplicate/stale CFG exit metadata");
        tests.expect(verifier.error().find("exit node set") != std::string::npos,
            "exit-set negative test fails for the intended invariant");
    }


    {
        TypeContext types;
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol firstSymbol(SymbolKind::Function, "First", functionType, nullptr);
        Symbol secondSymbol(SymbolKind::Function, "Second", functionType, nullptr);
        MirArena mirArena;
        MirModule* mirModule = mirArena.makeModule();
        MirFunction* firstFunction = mirArena.makeFunction(&firstSymbol);
        MirFunction* secondFunction = mirArena.makeFunction(&secondSymbol);
        firstFunction->setDefined(true);
        secondFunction->setDefined(true);
        mirModule->addFunction(firstFunction);
        mirModule->addFunction(secondFunction);
        MirBlock* firstBlock = mirArena.makeBlock("first.entry");
        MirBlock* secondBlock = mirArena.makeBlock("second.entry");
        firstFunction->addBlock(firstBlock);
        secondFunction->addBlock(secondBlock);
        MirInstruction* firstJump = mirArena.makeInstruction(MirOpcode::Jump, SourceRange());
        firstJump->setFirstTarget(secondBlock);
        MirInstruction* secondReturn = mirArena.makeInstruction(MirOpcode::Return, SourceRange());
        firstBlock->addInstruction(firstJump);
        secondBlock->addInstruction(secondReturn);

        CfgArena cfgArena;
        CfgModule* cfgModule = cfgArena.makeModule(mirModule);
        CfgFunction* firstCfg = cfgArena.makeFunction(firstFunction);
        CfgFunction* secondCfg = cfgArena.makeFunction(secondFunction);
        cfgModule->addFunction(firstCfg);
        cfgModule->addFunction(secondCfg);
        CfgNode* firstNode = cfgArena.makeNode(firstBlock);
        CfgNode* secondNode = cfgArena.makeNode(secondBlock);
        firstCfg->addNode(firstNode);
        secondCfg->addNode(secondNode);
        firstNode->setReachable(true);
        secondNode->setReachable(true);
        firstCfg->setReversePostOrder({firstNode});
        secondCfg->setReversePostOrder({secondNode});
        secondCfg->addExitNode(secondNode);
        secondCfg->addReachableExitNode(secondNode);

        CfgEdge* crossEdge = cfgArena.makeEdge(firstNode, secondNode, CfgEdgeKind::Jump);
        firstCfg->addEdge(crossEdge);
        firstNode->addOutgoing(crossEdge);
        secondNode->addIncoming(crossEdge);

        CfgVerifier verifier;
        tests.expect(!verifier.verify(cfgModule),
            "verifier rejects CFG edge whose target node belongs to another function");
        tests.expect(verifier.error().find("endpoints") != std::string::npos,
            "cross-function edge test fails for the intended endpoint invariant");
    }


    {
        Fixture fixture;
        CfgModule* module = fixture.makeValidCfg();
        module->addFunction(fixture.cfgFunction);
        CfgVerifier verifier;
        tests.expect(!verifier.verify(module),
            "verifier rejects duplicate CFG function entries for one MIR definition");
    }

    {
        Fixture fixture;
        fixture.cfgModule = fixture.cfgArena.makeModule(fixture.mirModule);
        fixture.cfgFunction = fixture.cfgArena.makeFunction(fixture.mirFunction);
        fixture.cfgModule->addFunction(fixture.cfgFunction);
        fixture.cfgNode = fixture.cfgArena.makeNode(fixture.entry);
        fixture.cfgFunction->addNode(fixture.cfgNode);
        fixture.cfgNode->setReachable(true);
        fixture.cfgFunction->setReversePostOrder({fixture.cfgNode});
        fixture.cfgFunction->addExitNode(fixture.cfgNode);
        CfgVerifier verifier;
        tests.expect(!verifier.verify(fixture.cfgModule),
            "verifier rejects reachable sink omitted from reachable-exit metadata");
        tests.expect(verifier.error().find("reachable exit") != std::string::npos,
            "reachable-exit negative test fails for the intended invariant");
    }

    {
        TypeContext types;
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        MirArena mirArena;
        MirModule* mirModule = mirArena.makeModule();
        MirFunction* mirFunction = mirArena.makeFunction(&functionSymbol);
        mirFunction->setDefined(true);
        mirModule->addFunction(mirFunction);
        MirBlock* entry = mirArena.makeBlock("entry");
        MirBlock* exit = mirArena.makeBlock("exit");
        mirFunction->addBlock(entry);
        mirFunction->addBlock(exit);
        MirInstruction* jump = mirArena.makeInstruction(MirOpcode::Jump, SourceRange());
        jump->setFirstTarget(exit);
        entry->addInstruction(jump);
        exit->addInstruction(mirArena.makeInstruction(MirOpcode::Return, SourceRange()));

        CfgArena cfgArena;
        CfgModule* cfgModule = cfgArena.makeModule(mirModule);
        CfgFunction* cfgFunction = cfgArena.makeFunction(mirFunction);
        cfgModule->addFunction(cfgFunction);
        CfgNode* entryNode = cfgArena.makeNode(entry);
        CfgNode* exitNode = cfgArena.makeNode(exit);
        cfgFunction->addNode(entryNode);
        cfgFunction->addNode(exitNode);
        CfgEdge* edge = cfgArena.makeEdge(entryNode, exitNode, CfgEdgeKind::BranchTrue);
        cfgFunction->addEdge(edge);
        entryNode->addOutgoing(edge);
        exitNode->addIncoming(edge);
        entryNode->setReachable(true);
        exitNode->setReachable(true);
        cfgFunction->setReversePostOrder({entryNode, exitNode});
        cfgFunction->addExitNode(exitNode);
        cfgFunction->addReachableExitNode(exitNode);

        CfgVerifier verifier;
        tests.expect(!verifier.verify(cfgModule),
            "verifier rejects CFG edge kind that does not match a MIR jump terminator");
        tests.expect(verifier.error().find("jump terminator") != std::string::npos,
            "jump-edge-kind negative test fails for the intended invariant");
    }

    {
        TypeContext types;
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "Prototype", functionType, nullptr);
        MirArena mirArena;
        MirModule* mirModule = mirArena.makeModule();
        MirFunction* prototype = mirArena.makeFunction(&functionSymbol);
        mirModule->addFunction(prototype);

        CfgArena cfgArena;
        CfgModule* cfgModule = cfgArena.makeModule(mirModule);
        CfgFunction* cfgFunction = cfgArena.makeFunction(prototype);
        cfgModule->addFunction(cfgFunction);

        CfgVerifier verifier;
        tests.expect(!verifier.verify(cfgModule),
            "verifier rejects CFG function materialized for a MIR prototype");
        tests.expect(verifier.error().find("prototype") != std::string::npos,
            "prototype-CFG negative test fails for the intended invariant");
    }

    return tests.result();
}
