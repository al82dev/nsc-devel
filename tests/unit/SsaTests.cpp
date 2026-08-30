#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/SSA/SsaArena.h"
#include "nativesharp/SSA/SsaBuilder.h"
#include "nativesharp/SSA/SsaDumper.h"
#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Sema/TypeKind.h"
#include "../support/TestContext.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirValue;
using nativesharp::ssa::SsaFunction;

bool compile(std::string_view source, FrontendCompilation& compilation)
{
    return compilation.compileBuffer(std::filesystem::path("ssa_test.ns"), std::string(source), false);
}

const SsaFunction* findFunction(const FrontendCompilation& compilation, std::string_view name)
{
    if (compilation.ssaModule() == nullptr)
    {
        return nullptr;
    }
    for (const SsaFunction* function : compilation.ssaModule()->functions())
    {
        if (function != nullptr && function->function() != nullptr && function->function()->symbol() != nullptr &&
            function->function()->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

const MirBlock* findBlock(const SsaFunction* function, std::string_view prefix)
{
    if (function == nullptr || function->function() == nullptr)
    {
        return nullptr;
    }
    for (const MirBlock* block : function->function()->blocks())
    {
        if (block != nullptr && block->name().starts_with(prefix))
        {
            return block;
        }
    }
    return nullptr;
}

const MirInstruction* findPhiForSymbol(const MirBlock* block, std::string_view symbolName)
{
    if (block == nullptr)
    {
        return nullptr;
    }
    for (const MirInstruction* instruction : block->instructions())
    {
        if (instruction == nullptr || instruction->opcode() != MirOpcode::Phi)
        {
            continue;
        }
        if (instruction->symbol() != nullptr && instruction->symbol()->name() == symbolName)
        {
            return instruction;
        }
    }
    return nullptr;
}

bool hasOpcode(const SsaFunction* function, MirOpcode opcode)
{
    if (function == nullptr || function->function() == nullptr)
    {
        return false;
    }
    for (const MirBlock* block : function->function()->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode)
            {
                return true;
            }
        }
    }
    return false;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    const char* source =
        "bool Logic(bool left, bool right) { return left && right; }"
        "int DeadTail() { return 1; int dead = 2; return dead; }"
        "int Main() {"
        "  int value = 0; int maybe; int addressed = 7; int* pointer = &addressed;"
        "  if (value == 0) { value = 1; } else { value = 2; }"
        "  while (value < 4) { value++; }"
        "  if (Logic(true, false)) { value += *pointer; }"
        "  return value + maybe;"
        "}";

    FrontendCompilation compilation;
    tests.expect(compile(source, compilation), "stage 15 source builds through SSA construction and verification");
    tests.expect(compilation.ssaModule() != nullptr, "frontend exposes verified SSA module");

    nativesharp::ssa::SsaVerifier verifier;
    tests.expect(verifier.verify(compilation.ssaModule()), "standalone SSA verifier accepts generated module");

    const SsaFunction* mainFunction = findFunction(compilation, "Main");
    tests.expect(mainFunction != nullptr, "Main has SSA function metadata");
    tests.expect(mainFunction != nullptr && mainFunction->isPromoted(
            mainFunction->function()->locals()[0]),
        "ordinary scalar local is promotable");

    bool addressedPromoted = false;
    bool addressedStorageOperation = false;
    if (mainFunction != nullptr)
    {
        for (const auto* symbol : mainFunction->promotedSymbols())
        {
            if (symbol != nullptr && symbol->name() == "addressed")
            {
                addressedPromoted = true;
            }
        }
        for (const MirBlock* block : mainFunction->function()->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->symbol() != nullptr &&
                    instruction->symbol()->name() == "addressed" &&
                    (instruction->opcode() == MirOpcode::AddressOfSymbol ||
                     instruction->opcode() == MirOpcode::LoadSymbol ||
                     instruction->opcode() == MirOpcode::StoreSymbol))
                {
                    addressedStorageOperation = true;
                }
            }
        }
    }
    tests.expect(!addressedPromoted, "address-taken local is conservatively excluded from SSA promotion");
    tests.expect(addressedStorageOperation, "non-promoted address-taken local retains explicit storage operations");

    const MirBlock* diamondMerge = findBlock(mainFunction, "if.end");
    const MirInstruction* diamondPhi = findPhiForSymbol(diamondMerge, "value");
    tests.expect(diamondPhi != nullptr && diamondPhi->phiIncoming().size() == 2,
        "diamond merge receives two-input PHI for promoted local");

    const MirBlock* loopHeader = findBlock(mainFunction, "while.cond");
    const MirInstruction* loopPhi = findPhiForSymbol(loopHeader, "value");
    tests.expect(loopPhi != nullptr && loopPhi->phiIncoming().size() >= 2,
        "loop header receives PHI for loop-carried promoted local");

    bool sawMaybeUndef = false;
    if (mainFunction != nullptr)
    {
        for (const MirInstruction* instruction : mainFunction->function()->entryBlock()->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == MirOpcode::Undef &&
                instruction->symbol() != nullptr && instruction->symbol()->name() == "maybe")
            {
                sawMaybeUndef = true;
            }
        }
    }
    tests.expect(sawMaybeUndef, "uninitialized promotable local begins with explicit SSA undef");

    const SsaFunction* logicFunction = findFunction(compilation, "Logic");
    bool sawVirtualPhi = false;
    if (logicFunction != nullptr)
    {
        for (const MirBlock* block : logicFunction->function()->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->opcode() == MirOpcode::Phi &&
                    instruction->symbol() == nullptr)
                {
                    sawVirtualPhi = instruction->phiIncoming().size() == 2;
                }
            }
        }
    }
    tests.expect(sawVirtualPhi, "short-circuit multi-defined MIR value becomes symbol-less SSA PHI");

    bool parameterReceivedUndef = false;
    if (logicFunction != nullptr)
    {
        for (const MirBlock* block : logicFunction->function()->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->opcode() == MirOpcode::Undef &&
                    instruction->symbol() != nullptr &&
                    instruction->symbol()->kind() == nativesharp::sema::SymbolKind::Parameter)
                {
                    parameterReceivedUndef = true;
                }
            }
        }
    }
    tests.expect(!parameterReceivedUndef,
        "promoted parameters begin from param definitions rather than artificial undef seeds");

    const SsaFunction* deadFunction = findFunction(compilation, "DeadTail");
    tests.expect(deadFunction != nullptr && deadFunction->excludedUnreachableBlocks() > 0,
        "SSA representation excludes source MIR blocks unreachable from function entry");

    std::unordered_map<const nativesharp::mir::MirValue*, std::size_t> definitionCounts;
    for (const SsaFunction* function : compilation.ssaModule()->functions())
    {
        for (const MirBlock* block : function->function()->blocks())
        {
            for (const MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->result() != nullptr)
                {
                    ++definitionCounts[instruction->result()];
                }
            }
        }
    }
    bool allSingleDefinition = true;
    for (const auto& pair : definitionCounts)
    {
        if (pair.second != 1)
        {
            allSingleDefinition = false;
        }
    }
    tests.expect(allSingleDefinition, "every SSA virtual value has exactly one defining instruction");

    tests.expect(!hasOpcode(mainFunction, MirOpcode::Copy),
        "test Main does not require legacy multi-definition copy after SSA lowering");

    std::ostringstream dump;
    nativesharp::ssa::SsaDumper dumper;
    dumper.dump(compilation.ssaModule(), dump);
    tests.expect(dump.str().find("ssa.module") != std::string::npos,
        "SSA dumper identifies representation explicitly");
    tests.expect(dump.str().find("phi @value") != std::string::npos,
        "SSA dump exposes symbol PHI nodes");
    tests.expect(dump.str().find("excluded-unreachable:") != std::string::npos,
        "SSA dump reports unreachable MIR blocks excluded from SSA domain");


    {
        nativesharp::sema::TypeContext types;
        nativesharp::sema::Type* intType = types.builtin(nativesharp::sema::TypeKind::Int);
        nativesharp::sema::Type* boolType = types.builtin(nativesharp::sema::TypeKind::Bool);
        nativesharp::sema::Type* functionType = types.functionType(intType, {});
        nativesharp::sema::Symbol functionSymbol(
            nativesharp::sema::SymbolKind::Function,
            "Irreducible",
            functionType,
            nullptr);
        functionSymbol.setDefined(true);
        nativesharp::sema::Symbol localSymbol(
            nativesharp::sema::SymbolKind::Variable,
            "x",
            intType,
            nullptr);

        nativesharp::mir::MirArena sourceArena;
        nativesharp::mir::MirModule* sourceModule = sourceArena.makeModule();
        nativesharp::mir::MirFunction* sourceFunction = sourceArena.makeFunction(&functionSymbol);
        sourceFunction->setDefined(true);
        sourceFunction->addLocal(&localSymbol);
        sourceModule->addFunction(sourceFunction);

        MirBlock* entry = sourceArena.makeBlock("entry.manual");
        MirBlock* left = sourceArena.makeBlock("left.manual");
        MirBlock* right = sourceArena.makeBlock("right.manual");
        MirBlock* exit = sourceArena.makeBlock("exit.manual");
        sourceFunction->addBlock(entry);
        sourceFunction->addBlock(left);
        sourceFunction->addBlock(right);
        sourceFunction->addBlock(exit);

        MirValue* entryCondition = sourceArena.makeValue(boolType);
        MirInstruction* entryConstant = sourceArena.makeInstruction(MirOpcode::Constant, {});
        entryConstant->setResult(entryCondition);
        entryConstant->setConstant(nativesharp::sema::ConstantValue::boolean(true));
        entry->addInstruction(entryConstant);
        MirInstruction* entryBranch = sourceArena.makeInstruction(MirOpcode::Branch, {});
        entryBranch->addOperand(entryCondition);
        entryBranch->setFirstTarget(left);
        entryBranch->setSecondTarget(right);
        entry->addInstruction(entryBranch);

        MirValue* leftValue = sourceArena.makeValue(intType);
        MirInstruction* leftConstant = sourceArena.makeInstruction(MirOpcode::Constant, {});
        leftConstant->setResult(leftValue);
        leftConstant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        left->addInstruction(leftConstant);
        MirInstruction* leftStore = sourceArena.makeInstruction(MirOpcode::StoreSymbol, {});
        leftStore->setSymbol(&localSymbol);
        leftStore->addOperand(leftValue);
        left->addInstruction(leftStore);
        MirValue* leftCondition = sourceArena.makeValue(boolType);
        MirInstruction* leftBool = sourceArena.makeInstruction(MirOpcode::Constant, {});
        leftBool->setResult(leftCondition);
        leftBool->setConstant(nativesharp::sema::ConstantValue::boolean(true));
        left->addInstruction(leftBool);
        MirInstruction* leftBranch = sourceArena.makeInstruction(MirOpcode::Branch, {});
        leftBranch->addOperand(leftCondition);
        leftBranch->setFirstTarget(right);
        leftBranch->setSecondTarget(exit);
        left->addInstruction(leftBranch);

        MirValue* rightValue = sourceArena.makeValue(intType);
        MirInstruction* rightConstant = sourceArena.makeInstruction(MirOpcode::Constant, {});
        rightConstant->setResult(rightValue);
        rightConstant->setConstant(nativesharp::sema::ConstantValue::signedInteger(2));
        right->addInstruction(rightConstant);
        MirInstruction* rightStore = sourceArena.makeInstruction(MirOpcode::StoreSymbol, {});
        rightStore->setSymbol(&localSymbol);
        rightStore->addOperand(rightValue);
        right->addInstruction(rightStore);
        MirValue* rightCondition = sourceArena.makeValue(boolType);
        MirInstruction* rightBool = sourceArena.makeInstruction(MirOpcode::Constant, {});
        rightBool->setResult(rightCondition);
        rightBool->setConstant(nativesharp::sema::ConstantValue::boolean(true));
        right->addInstruction(rightBool);
        MirInstruction* rightBranch = sourceArena.makeInstruction(MirOpcode::Branch, {});
        rightBranch->addOperand(rightCondition);
        rightBranch->setFirstTarget(left);
        rightBranch->setSecondTarget(exit);
        right->addInstruction(rightBranch);

        MirValue* exitValue = sourceArena.makeValue(intType);
        MirInstruction* exitLoad = sourceArena.makeInstruction(MirOpcode::LoadSymbol, {});
        exitLoad->setResult(exitValue);
        exitLoad->setSymbol(&localSymbol);
        exit->addInstruction(exitLoad);
        MirInstruction* exitReturn = sourceArena.makeInstruction(MirOpcode::Return, {});
        exitReturn->addOperand(exitValue);
        exit->addInstruction(exitReturn);

        nativesharp::mir::MirVerifier sourceVerifier;
        tests.expect(sourceVerifier.verify(sourceModule), "manual irreducible pre-SSA MIR verifies");

        nativesharp::cfg::CfgArena cfgArena;
        nativesharp::cfg::CfgBuilder cfgBuilder(cfgArena);
        nativesharp::cfg::CfgModule* cfgModule = cfgBuilder.build(sourceModule);
        nativesharp::cfg::CfgVerifier cfgVerifier;
        tests.expect(cfgModule != nullptr && cfgVerifier.verify(cfgModule),
            "manual irreducible MIR builds a verified CFG");

        nativesharp::dominance::DominanceArena dominanceArena;
        nativesharp::dominance::DominanceBuilder dominanceBuilder(dominanceArena);
        nativesharp::dominance::DominanceModule* dominanceModule = dominanceBuilder.build(cfgModule);
        nativesharp::dominance::DominanceVerifier dominanceVerifier;
        tests.expect(dominanceModule != nullptr && dominanceVerifier.verify(dominanceModule),
            "manual irreducible CFG builds verified dominance information");

        nativesharp::ssa::SsaArena ssaArena;
        nativesharp::ssa::SsaBuilder ssaBuilder(ssaArena);
        nativesharp::ssa::SsaModule* ssaModule = ssaBuilder.build(sourceModule, cfgModule, dominanceModule);
        nativesharp::ssa::SsaVerifier ssaVerifier;
        tests.expect(ssaModule != nullptr && ssaVerifier.verify(ssaModule),
            "SSA construction works on valid irreducible CFG, not only structured source CFGs");

        const SsaFunction* irreducible = ssaModule == nullptr ? nullptr : ssaModule->findFunction(sourceFunction);
        std::size_t phiCount = 0;
        if (irreducible != nullptr)
        {
            for (const MirBlock* block : irreducible->function()->blocks())
            {
                for (const MirInstruction* instruction : block->instructions())
                {
                    if (instruction != nullptr && instruction->opcode() == MirOpcode::Phi &&
                        instruction->symbol() == &localSymbol)
                    {
                        ++phiCount;
                    }
                }
            }
        }
        tests.expect(phiCount >= 2,
            "irreducible two-entry SCC requires multiple PHI placements for promoted local");
    }

    return tests.result();
}
