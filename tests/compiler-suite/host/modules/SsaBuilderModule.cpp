#include "SsaContractSupport.h"

#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/SSA/SsaArena.h"
#include "nativesharp/SSA/SsaBuilder.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/Type.h"

#include <unordered_set>

namespace nsc::suite
{
namespace
{
using ssa_contract_support::addMatrix;
using ssa_contract_support::compileSsa;
using ssa_contract_support::countOpcode;
using ssa_contract_support::findFunction;
using ssa_contract_support::findInstruction;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirValue;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::ssa::SsaArena;
using nativesharp::ssa::SsaBuilder;
using nativesharp::ssa::SsaFunction;
using nativesharp::ssa::SsaModule;

std::string richSource(std::size_t variant)
{
    const std::string addressed = variant == 5 ? "int addressed=7;int* pointer=&addressed;" : "";
    const std::string wide = variant == 7 ?
        "int a=1;int b=2;int c=3;int d=4;int e=5;int f=6;int g=7;int h=8;" : "";
    return "int Global=3;extern C int External(int value);"
        "int F(int input){int value=0;int other=3;" + addressed + wide +
        "if(input==0){value=1;}else{value=2;}"
        "while(other>0){other=other-1;}return value+other;}";
}

SsaFunction* compileFunction(TestContext& context, std::size_t variant,
    std::unique_ptr<nativesharp::frontend::FrontendCompilation>& compilation)
{
    compilation = compileSsa(context, "Host.SSA.builder", richSource(variant));
    SsaFunction* function = findFunction(compilation->ssaModule(), "F");
    context.expect(function != nullptr, "Host.SSA builder fixture must retain defined function metadata");
    return function;
}

std::size_t countPromotedOpcode(const SsaFunction* function, MirOpcode opcode)
{
    std::size_t count = 0;
    if (function == nullptr || function->function() == nullptr)
        return count;
    for (const MirBlock* block : function->function()->blocks())
    {
        if (block == nullptr)
            continue;
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode &&
                function->isPromoted(instruction->symbol()))
                ++count;
        }
    }
    return count;
}

void moduleGateOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    const SsaModule* module = compilation->ssaModule();
    context.expect(module != nullptr && module->sourceModule() == compilation->mirModule(),
        "CF-SSA-009 module must retain exact source MIR provenance");
    context.expect(module != nullptr && module->module() != nullptr && function != nullptr,
        "CF-SSA-009 successful gate must publish output module and function metadata");
}

void globalsOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    compileFunction(context, variant, compilation);
    const SsaModule* module = compilation->ssaModule();
    context.expect(module->sourceModule()->globals().size() == module->module()->globals().size(),
        "CF-SSA-010 global clone cardinality must be exact");
    for (std::size_t index = 0; index < module->sourceModule()->globals().size(); ++index)
    {
        context.expect(module->sourceModule()->globals()[index]->symbol() ==
                module->module()->globals()[index]->symbol(),
            "CF-SSA-010 global clone must preserve symbol order and identity");
    }
}

void prototypesOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    compileFunction(context, variant, compilation);
    const SsaModule* module = compilation->ssaModule();
    context.expect(module->sourceModule()->functions().size() == module->module()->functions().size(),
        "CF-SSA-011 output function sequence must preserve source cardinality");
    for (std::size_t index = 0; index < module->sourceModule()->functions().size(); ++index)
    {
        const MirFunction* source = module->sourceModule()->functions()[index];
        const MirFunction* output = module->module()->functions()[index];
        context.expect(source->symbol() == output->symbol() && source->defined() == output->defined(),
            "CF-SSA-011 prototype clone must preserve symbol and defined flag");
        if (!source->defined())
        {
            context.expect(module->findFunction(source) == nullptr,
                "CF-SSA-011 prototypes must not receive SsaFunction metadata");
        }
    }
}

void definedFunctionOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(function != nullptr && function->sourceFunction() != nullptr && function->function() != nullptr,
        "CF-SSA-012 defined pipeline must retain source and output functions");
    context.expect(function != nullptr && function->cfgFunction() != nullptr &&
            function->dominanceFunction() != nullptr,
        "CF-SSA-012 defined pipeline must retain CFG and dominance provenance");
}

void parameterResetOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(function->sourceFunction()->parameters().size() == function->function()->parameters().size(),
        "CF-SSA-013 output parameters must be complete and positional");
    for (std::size_t index = 0; index < function->sourceFunction()->parameters().size(); ++index)
    {
        context.expect(function->sourceFunction()->parameters()[index]->type() ==
                function->function()->parameters()[index]->type(),
            "CF-SSA-013 parameter reset must not leak types between functions");
    }
}

void classificationOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    std::unordered_set<const Symbol*> unique;
    for (const Symbol* symbol : function->promotedSymbols())
    {
        context.expect(symbol != nullptr && (symbol->kind() == SymbolKind::Variable ||
                symbol->kind() == SymbolKind::Parameter),
            "CF-SSA-014 only local variables and parameters may be promoted");
        context.expect(unique.insert(symbol).second,
            "CF-SSA-014 promoted-symbol metadata must be ordered and duplicate-free");
    }
}

void virtualDiscoveryOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    const std::size_t phiCount = countOpcode(function, MirOpcode::Phi);
    context.expect(phiCount >= 1, "CF-SSA-015 branch definitions must discover at least one merge value");
}

void reachableCloneOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(function->function()->blocks().size() + function->excludedUnreachableBlocks() ==
            function->sourceFunction()->blocks().size(),
        "CF-SSA-016 reachable clone and excluded count must partition source blocks exactly");
    for (MirBlock* output : function->function()->blocks())
    {
        context.expect(function->findSourceBlock(output) != nullptr,
            "CF-SSA-016 every output block must map to one reachable source block");
    }
}

void symbolDefinitionOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(countOpcode(function, MirOpcode::Undef) >= 1,
        "CF-SSA-017 promoted variables must have explicit entry definitions");
    context.expect(countPromotedOpcode(function, MirOpcode::StoreSymbol) == 0,
        "CF-SSA-017 promoted definitions must no longer be storage stores");
}

void valueDefinitionOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    std::unordered_set<const MirValue*> definitions;
    for (const MirBlock* block : function->function()->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                context.expect(definitions.insert(instruction->result()).second,
                    "CF-SSA-018 each output value must have one definition node");
            }
        }
    }
}

void phiPlanOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    context.expect(phi != nullptr && phi->symbol() != nullptr,
        "CF-SSA-019 iterated frontier must plan a symbol PHI at the branch merge");
}

void phiMaterializationOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    context.expect(phi != nullptr && phi->result() != nullptr && phi->result()->type() != nullptr,
        "CF-SSA-020 materialized PHI must own a fresh typed result");
}

void renameOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(function != nullptr && !function->function()->blocks().empty(),
        "CF-SSA-021 iterative rename must terminate with a complete output function");
    context.expect(countOpcode(function, MirOpcode::LoadSymbol) == 0,
        "CF-SSA-021 rename must resolve promoted loads through explicit stacks");
}

void scopedStackOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    context.expect(phi != nullptr && phi->phiIncoming().size() == 2,
        "CF-SSA-022 sibling definitions must meet only through the merge PHI");
    if (phi != nullptr && phi->phiIncoming().size() == 2)
    {
        context.expect(phi->phiIncoming()[0].value != phi->phiIncoming()[1].value,
            "CF-SSA-022 sibling reaching definitions must remain isolated");
    }
}

void undefOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    MirInstruction* undef = findInstruction(function, MirOpcode::Undef);
    context.expect(undef != nullptr && undef->result() != nullptr && undef->symbol() != nullptr,
        "CF-SSA-023 entry undef seed must retain symbol, type, and result");
    context.expect(function != nullptr && !function->function()->blocks().empty() &&
            function->function()->blocks().front()->instructions().front()->opcode() == MirOpcode::Undef,
        "CF-SSA-023 undef seeds must be placed at the logical entry prefix");
}

void loadEliminationOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(countOpcode(function, MirOpcode::LoadSymbol) == 0,
        "CF-SSA-024 promoted loads must be absent from output MIR");
}

void storeEliminationOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(countPromotedOpcode(function, MirOpcode::StoreSymbol) == 0,
        "CF-SSA-025 promoted stores must be absent from output MIR");
}

void cloneOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(countOpcode(function, MirOpcode::Branch) >= 1 &&
            countOpcode(function, MirOpcode::Return) == 1,
        "CF-SSA-026 general instructions must preserve opcode and control-flow payload");
}

void positionalParameterOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    context.expect(function->function()->parameters().size() == 1,
        "CF-SSA-027 parameter reconstruction must retain exact positional cardinality");
    context.expect(function->function()->parameters()[0] != nullptr &&
            function->function()->parameters()[0]->type() ==
                function->sourceFunction()->parameters()[0]->type(),
        "CF-SSA-027 reconstructed parameter value must retain exact type");
}

void valueResolutionOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    for (const MirBlock* block : function->function()->blocks())
    {
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                context.expect(instruction->result()->type() != nullptr,
                    "CF-SSA-028 every newly assigned value must be typed");
            }
            for (const MirValue* operand : instruction->operands())
            {
                context.expect(operand != nullptr, "CF-SSA-028 every cloned operand must resolve to a value");
            }
        }
    }
}

void incomingOracle(TestContext& context, std::size_t variant)
{
    std::unique_ptr<nativesharp::frontend::FrontendCompilation> compilation;
    SsaFunction* function = compileFunction(context, variant, compilation);
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    std::unordered_set<const MirBlock*> predecessors;
    if (phi != nullptr)
    {
        for (const auto& incoming : phi->phiIncoming())
        {
            context.expect(incoming.block != nullptr && incoming.value != nullptr,
                "CF-SSA-029 every PHI incoming must retain predecessor and value");
            context.expect(predecessors.insert(incoming.block).second,
                "CF-SSA-029 each predecessor may contribute exactly one incoming");
        }
    }
    context.expect(phi != nullptr && predecessors.size() == phi->phiIncoming().size(),
        "CF-SSA-029 incoming completion and uniqueness must agree");
}

void resetOracle(TestContext& context, std::size_t variant)
{
    auto compilation = compileSsa(context, "CF-SSA-030", richSource(variant));
    SsaArena arena;
    SsaBuilder builder(arena);
    SsaModule* first = builder.build(compilation->mirModule(), compilation->cfgModule(),
        compilation->dominanceModule());
    context.expect(first != nullptr && builder.succeeded() && builder.error().empty(),
        "CF-SSA-030 initial valid build must succeed with empty error");
    context.expect(builder.build(nullptr, compilation->cfgModule(), compilation->dominanceModule()) == nullptr &&
            !builder.succeeded() && !builder.error().empty(),
        "CF-SSA-030 invalid reuse must publish one first error");
}
}

// Registers CF-SSA-009..030 module construction, PHI planning, and renaming matrices.
void registerSsaBuilderModule(TestRegistry& registry)
{
    addMatrix(registry, 9, "component", moduleGateOracle);
    addMatrix(registry, 10, "component", globalsOracle);
    addMatrix(registry, 11, "component", prototypesOracle);
    addMatrix(registry, 12, "component", definedFunctionOracle);
    addMatrix(registry, 13, "component", parameterResetOracle);
    addMatrix(registry, 14, "component", classificationOracle);
    addMatrix(registry, 15, "component", virtualDiscoveryOracle);
    addMatrix(registry, 16, "component", reachableCloneOracle);
    addMatrix(registry, 17, "component", symbolDefinitionOracle);
    addMatrix(registry, 18, "component", valueDefinitionOracle);
    addMatrix(registry, 19, "component", phiPlanOracle);
    addMatrix(registry, 20, "component", phiMaterializationOracle);
    addMatrix(registry, 21, "component", renameOracle);
    addMatrix(registry, 22, "component", scopedStackOracle);
    addMatrix(registry, 23, "component", undefOracle);
    addMatrix(registry, 24, "component", loadEliminationOracle);
    addMatrix(registry, 25, "component", storeEliminationOracle);
    addMatrix(registry, 26, "component", cloneOracle);
    addMatrix(registry, 27, "component", positionalParameterOracle);
    addMatrix(registry, 28, "unit", valueResolutionOracle);
    addMatrix(registry, 29, "component", incomingOracle);
    addMatrix(registry, 30, "unit", resetOracle);
}
}
