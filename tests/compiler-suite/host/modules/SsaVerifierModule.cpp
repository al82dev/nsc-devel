#include "SsaContractSupport.h"

#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Sema/Symbol.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using ssa_contract_support::addMatrix;
using ssa_contract_support::compileSsa;
using ssa_contract_support::findFunction;
using ssa_contract_support::findInstruction;
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirGlobal;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirPhiIncoming;
using nativesharp::mir::MirValue;
using nativesharp::sema::Symbol;
using nativesharp::ssa::SsaFunction;
using nativesharp::ssa::SsaModule;
using nativesharp::ssa::SsaVerifier;

const char* kVerifierSource =
    "int Global=3;"
    "int F(int input){int value=0;int other=3;int addressed=7;int* pointer=&addressed;"
    "if(input==0){value=1;}else{value=2;}return value+other+*pointer;}";

std::unique_ptr<FrontendCompilation> fixture(TestContext& context)
{
    return compileSsa(context, "Host.SSA.verifier", kVerifierSource);
}

void expectVerify(TestContext& context, const SsaModule* module, bool expected, std::string_view contract)
{
    SsaVerifier verifier;
    const bool actual = verifier.verify(module);
    context.expect(actual == expected, std::string(contract) + " verifier verdict must be exact");
    context.expect(actual == verifier.error().empty(),
        std::string(contract) + " verdict and first-error state must agree");
}

void shapeOracle(TestContext& context, std::size_t variant)
{
    auto compilation = variant == 3
        ? compileSsa(context, "Host.SSA.verifier.order",
            std::string(kVerifierSource) + "int G(){return 1;}")
        : fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    bool accepted = variant == 0;
    if (variant == 1)
    {
        auto& globals = const_cast<std::vector<MirGlobal*>&>(module->module()->globals());
        globals.clear();
    }
    else if (variant == 2)
    {
        auto& functions = const_cast<std::vector<MirFunction*>&>(module->module()->functions());
        functions.clear();
    }
    else if (variant == 3)
    {
        auto& functions = const_cast<std::vector<MirFunction*>&>(module->module()->functions());
        std::reverse(functions.begin(), functions.end());
    }
    else if (variant == 4)
    {
        module->module()->addFunction(nullptr);
    }
    else if (variant == 5)
    {
        module->module()->addGlobal(nullptr);
    }
    else if (variant == 6)
    {
        module->addFunction(nullptr);
    }
    else if (variant == 7)
    {
        auto& functions = const_cast<std::vector<MirFunction*>&>(module->module()->functions());
        functions.insert(functions.end(), functions.begin(), functions.end());
    }
    expectVerify(context, module, accepted, "CF-SSA-034");
}

void provenanceOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    bool accepted = variant == 0;
    if (variant == 1) function->function()->setDefined(false);
    else if (variant == 2) function->function()->addLocal(nullptr);
    else if (variant == 3)
    {
        auto& locals = const_cast<std::vector<Symbol*>&>(function->function()->locals());
        std::reverse(locals.begin(), locals.end());
    }
    else if (variant == 4) function->function()->addParameter(nullptr);
    else if (variant == 5) function->function()->replaceBlocks({});
    else if (variant == 6) function->setExcludedUnreachableBlocks(999);
    else if (variant == 7)
    {
        function->function()->addLocal(function->function()->locals().front());
    }
    expectVerify(context, module, accepted, "CF-SSA-035");
}

void promotionOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    bool accepted = variant == 0;
    MirInstruction* address = findInstruction(function, MirOpcode::AddressOfSymbol);
    if (variant == 1 && address != nullptr) function->addPromotedSymbol(address->symbol());
    else if (variant == 2 && !function->promotedSymbols().empty())
        function->addPromotedSymbol(function->promotedSymbols().front());
    else if (variant == 3)
    {
        auto& symbols = const_cast<std::vector<Symbol*>&>(function->promotedSymbols());
        std::reverse(symbols.begin(), symbols.end());
    }
    else if (variant == 4) function->addPromotedSymbol(nullptr);
    else if (variant == 5 && !function->promotedSymbols().empty())
    {
        auto& symbols = const_cast<std::vector<Symbol*>&>(function->promotedSymbols());
        symbols.erase(symbols.begin());
    }
    else if (variant == 6 && address != nullptr) function->addPromotedSymbol(address->symbol());
    else if (variant == 7)
    {
        for (std::size_t index = 0; index < 256; ++index) function->addPromotedSymbol(nullptr);
    }
    expectVerify(context, module, accepted, "CF-SSA-036");
}

void mappingOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    bool accepted = variant == 0;
    if (variant == 1) function->setExcludedUnreachableBlocks(function->excludedUnreachableBlocks() + 1);
    else if (variant == 2) function->function()->replaceBlocks({});
    else if (variant == 3 && !function->function()->blocks().empty())
    {
        auto blocks = function->function()->blocks();
        std::reverse(blocks.begin(), blocks.end());
        function->function()->replaceBlocks(std::move(blocks));
    }
    else if (variant == 4 && !function->function()->blocks().empty())
        function->function()->addBlock(function->function()->blocks().front());
    else if (variant == 5 && !function->function()->blocks().empty())
    {
        auto blocks = function->function()->blocks();
        blocks.front() = nullptr;
        function->function()->replaceBlocks(std::move(blocks));
    }
    else if (variant == 6) function->setExcludedUnreachableBlocks(999);
    else if (variant == 7)
    {
        auto blocks = function->function()->blocks();
        blocks.insert(blocks.end(), blocks.begin(), blocks.end());
        function->function()->replaceBlocks(std::move(blocks));
    }
    expectVerify(context, module, accepted, "CF-SSA-037");
}

void definitionsOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    bool accepted = variant == 0;
    MirInstruction* first = nullptr;
    MirInstruction* second = nullptr;
    for (MirBlock* block : function->function()->blocks())
    {
        for (MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                if (first == nullptr) first = instruction;
                else if (second == nullptr) second = instruction;
            }
        }
    }
    if (variant == 1 && first != nullptr && second != nullptr) second->setResult(first->result());
    else if (variant == 2 && first != nullptr) first->setResult(nullptr);
    else if (variant == 3 && first != nullptr) first->resetTo(MirOpcode::LoadSymbol);
    else if (variant == 4 && first != nullptr) first->resetTo(MirOpcode::StoreSymbol);
    else if (variant == 5 && first != nullptr) first->resetTo(MirOpcode::AddressOfSymbol);
    else if (variant == 6 && second != nullptr) second->setResult(first == nullptr ? nullptr : first->result());
    else if (variant == 7)
    {
        for (MirBlock* block : function->function()->blocks())
            for (MirInstruction* instruction : block->instructions()) instruction->setResult(first->result());
    }
    expectVerify(context, module, accepted, "CF-SSA-038");
}

void undefOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    MirInstruction* undef = findInstruction(function, MirOpcode::Undef);
    bool accepted = variant == 0;
    if (variant == 1 && undef != nullptr) undef->setSymbol(function->sourceFunction()->locals().front());
    else if (variant == 2 && undef != nullptr) undef->setResult(nullptr);
    else if (variant == 3 && undef != nullptr) undef->setSymbol(nullptr);
    else if (variant == 4 && undef != nullptr) undef->resetTo(MirOpcode::Constant);
    else if (variant == 5 && undef != nullptr)
    {
        MirBlock* entry = function->function()->blocks().front();
        auto instructions = entry->instructions();
        instructions.push_back(undef);
        entry->replaceInstructions(std::move(instructions));
    }
    else if (variant == 6 && undef != nullptr)
        undef->setSymbol(function->sourceFunction()->locals().back());
    else if (variant == 7 && undef != nullptr)
    {
        for (std::size_t index = 0; index < 128; ++index)
            function->function()->blocks().front()->addInstruction(undef);
    }
    expectVerify(context, module, accepted, "CF-SSA-039");
}

void phiOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    bool accepted = variant == 0;
    if (phi != nullptr && variant == 1)
    {
        auto& incoming = const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
        incoming.pop_back();
    }
    else if (phi != nullptr && variant == 2 && phi->phiIncoming().size() > 1)
    {
        auto& incoming = const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
        incoming[1].block = incoming[0].block;
    }
    else if (phi != nullptr && variant == 3) phi->clearPhiIncoming();
    else if (phi != nullptr && variant == 4 && !phi->phiIncoming().empty())
    {
        auto& incoming = const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
        incoming.front().value = nullptr;
    }
    else if (phi != nullptr && variant == 5) phi->setSymbol(nullptr);
    else if (phi != nullptr && variant == 6) phi->setResult(nullptr);
    else if (phi != nullptr && variant == 7)
    {
        auto& incoming = const_cast<std::vector<MirPhiIncoming>&>(phi->phiIncoming());
        incoming.insert(incoming.end(), incoming.begin(), incoming.end());
    }
    expectVerify(context, module, accepted, "CF-SSA-040");
}

void plannedPhiOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaFunction* function = findFunction(module, "F");
    MirInstruction* phi = findInstruction(function, MirOpcode::Phi);
    bool accepted = variant == 0;
    if (variant == 1 && phi != nullptr) phi->setSymbol(nullptr);
    else if (variant == 2 && phi != nullptr) phi->resetTo(MirOpcode::Add);
    else if (variant == 3 && phi != nullptr) phi->clearPhiIncoming();
    else if (variant == 4 && phi != nullptr) phi->setResult(nullptr);
    else if (variant == 5 && phi != nullptr) phi->setSymbol(function->promotedSymbols().back());
    else if (variant == 6 && phi != nullptr)
        function->function()->blocks().front()->addInstruction(phi);
    else if (variant == 7 && phi != nullptr)
    {
        for (std::size_t index = 0; index < 128; ++index)
            function->function()->blocks().front()->addInstruction(phi);
    }
    expectVerify(context, module, accepted, "CF-SSA-041");
}

void resetOracle(TestContext& context, std::size_t variant)
{
    auto compilation = fixture(context);
    SsaModule* module = const_cast<SsaModule*>(compilation->ssaModule());
    SsaVerifier verifier;
    context.expect(verifier.verify(module) && verifier.error().empty(),
        "CF-SSA-042 first valid verification must clear error state");
    SsaFunction* function = findFunction(module, "F");
    function->setExcludedUnreachableBlocks(function->excludedUnreachableBlocks() + variant + 1);
    context.expect(!verifier.verify(module) && !verifier.error().empty(),
        "CF-SSA-042 corrupt verification must retain its first structural error");
}
}

// Registers CF-SSA-034..042 independent verifier and reset matrices.
void registerSsaVerifierModule(TestRegistry& registry)
{
    addMatrix(registry, 34, "component", shapeOracle);
    addMatrix(registry, 35, "component", provenanceOracle);
    addMatrix(registry, 36, "component", promotionOracle);
    addMatrix(registry, 37, "component", mappingOracle);
    addMatrix(registry, 38, "component", definitionsOracle);
    addMatrix(registry, 39, "component", undefOracle);
    addMatrix(registry, 40, "component", phiOracle);
    addMatrix(registry, 41, "component", plannedPhiOracle);
    addMatrix(registry, 42, "unit", resetOracle);
}
}
