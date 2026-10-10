#include "SsaContractSupport.h"

#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/SSA/SsaArena.h"
#include "nativesharp/SSA/SsaPromotability.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <type_traits>

namespace nsc::suite
{
namespace
{
using ssa_contract_support::addMatrix;
using ssa_contract_support::compileSsa;
using ssa_contract_support::findFunction;
using nativesharp::mir::MirArena;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirModule;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;
using nativesharp::ssa::SsaArena;
using nativesharp::ssa::SsaFunction;
using nativesharp::ssa::SsaModule;

void arenaOracle(TestContext& context, std::size_t variant)
{
    SsaArena arena;
    MirModule* output = arena.mirArena().makeModule();
    SsaModule* module = arena.makeModule(nullptr, output);
    const std::size_t count = variant == 7 ? 1024 : variant;
    for (std::size_t index = 0; index < count; ++index)
    {
        SsaFunction* function = arena.makeFunction(nullptr, nullptr, nullptr, nullptr);
        module->addFunction(function);
    }
    context.expect(module != nullptr && module->module() == output,
        "CF-SSA-001 arena must retain output MIR identity");
    context.expect(module->functions().size() == count,
        "CF-SSA-001 arena-owned function registry must preserve allocation sequence");
    context.expect(!std::is_copy_constructible_v<SsaArena>, "CF-SSA-001 arena must remain non-copyable");
}

void moduleOracle(TestContext& context, std::size_t variant)
{
    MirArena mir;
    MirModule* source = mir.makeModule();
    MirModule* output = mir.makeModule();
    SsaModule module(source, output);
    const std::size_t count = variant == 7 ? 512 : variant;
    std::vector<std::unique_ptr<SsaFunction>> functions;
    for (std::size_t index = 0; index < count; ++index)
    {
        MirFunction* sourceFunction = mir.makeFunction(nullptr);
        MirFunction* outputFunction = mir.makeFunction(nullptr);
        functions.push_back(std::make_unique<SsaFunction>(
            sourceFunction, outputFunction, nullptr, nullptr));
        module.addFunction(functions.back().get());
        context.expect(module.findFunction(sourceFunction) == functions.back().get(),
            "CF-SSA-002 source-function index must preserve pointer identity");
    }
    context.expect(module.sourceModule() == source && module.module() == output,
        "CF-SSA-002 module source/output tuple must be exact");
    context.expect(module.functions().size() == count,
        "CF-SSA-002 function metadata sequence must preserve cardinality");
}

void provenanceOracle(TestContext& context, std::size_t variant)
{
    MirArena mir;
    MirFunction* source = mir.makeFunction(nullptr);
    MirFunction* output = mir.makeFunction(nullptr);
    const auto* cfg = variant == 2 ? nullptr : reinterpret_cast<const nativesharp::cfg::CfgFunction*>(source);
    const auto* dominance = variant == 3 ? nullptr :
        reinterpret_cast<const nativesharp::dominance::DominanceFunction*>(output);
    SsaFunction function(source, output, cfg, dominance);
    context.expect(function.sourceFunction() == source && function.function() == output,
        "CF-SSA-003 MIR provenance identities must be exact");
    context.expect(function.cfgFunction() == cfg && function.dominanceFunction() == dominance,
        "CF-SSA-003 analysis provenance identities must be exact");
}

void promotedSetOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    const std::size_t count = variant == 7 ? 512 : variant;
    std::vector<std::unique_ptr<Symbol>> symbols;
    SsaFunction function(nullptr, nullptr, nullptr, nullptr);
    for (std::size_t index = 0; index < count; ++index)
    {
        symbols.push_back(std::make_unique<Symbol>(SymbolKind::Variable,
            "v" + std::to_string(index), types.builtin(TypeKind::Int), nullptr));
        function.addPromotedSymbol(symbols.back().get());
    }
    context.expect(function.promotedSymbols().size() == count,
        "CF-SSA-004 promoted set must preserve insertion cardinality and order");
    for (std::size_t index = 0; index < count; ++index)
    {
        context.expect(function.promotedSymbols()[index] == symbols[index].get() &&
                function.isPromoted(symbols[index].get()),
            "CF-SSA-004 membership lookup must agree with ordered storage");
    }
}

void mappingOracle(TestContext& context, std::size_t variant)
{
    MirArena mir;
    SsaFunction function(nullptr, nullptr, nullptr, nullptr);
    const std::size_t count = variant == 7 ? 512 : variant + 1;
    for (std::size_t index = 0; index < count; ++index)
    {
        MirBlock* source = mir.makeBlock("source." + std::to_string(index));
        MirBlock* output = mir.makeBlock("output." + std::to_string(index));
        function.mapBlock(source, output);
        context.expect(function.findOutputBlock(source) == output,
            "CF-SSA-005 source-to-output mapping must round trip");
        context.expect(function.findSourceBlock(output) == source,
            "CF-SSA-005 output-to-source mapping must round trip");
    }
}

void excludedOracle(TestContext& context, std::size_t variant)
{
    SsaFunction function(nullptr, nullptr, nullptr, nullptr);
    const std::size_t count = variant == 7 ? 100000 : variant;
    function.setExcludedUnreachableBlocks(count);
    context.expect(function.excludedUnreachableBlocks() == count,
        "CF-SSA-006 excluded-unreachable count must preserve zero and boundary values");
}

void promotableOracle(TestContext& context, std::size_t variant)
{
    const std::string locals = variant == 7 ?
        "int a=0;int b=1;int c=2;int d=3;int e=4;int f=5;int g=6;int h=7;" :
        "int a=0;int b=1;";
    auto compilation = compileSsa(context, "CF-SSA-007", "int F(){" + locals + "return a+b;}");
    SsaFunction* function = findFunction(compilation->ssaModule(), "F");
    context.expect(function != nullptr && !function->promotedSymbols().empty(),
        "CF-SSA-007 local variable storage must be discovered as promotable");
    for (const Symbol* symbol : function == nullptr ? std::vector<Symbol*>{} : function->promotedSymbols())
    {
        context.expect(symbol != nullptr && (symbol->kind() == SymbolKind::Variable ||
                symbol->kind() == SymbolKind::Parameter),
            "CF-SSA-007 promoted storage kind must be variable or parameter");
    }
}

void addressTakenOracle(TestContext& context, std::size_t variant)
{
    const std::string address = variant % 2 == 0 ? "int* p=&taken;" : "";
    auto compilation = compileSsa(context, "CF-SSA-008",
        "int F(){int plain=1;int taken=2;" + address + "return plain+taken;}");
    SsaFunction* function = findFunction(compilation->ssaModule(), "F");
    bool plain = false;
    bool taken = false;
    if (function != nullptr)
    {
        for (const Symbol* symbol : function->promotedSymbols())
        {
            plain = plain || (symbol != nullptr && symbol->name() == "plain");
            taken = taken || (symbol != nullptr && symbol->name() == "taken");
        }
    }
    context.expect(plain, "CF-SSA-008 storage without address-taking must remain promotable");
    context.expect(taken == address.empty(),
        "CF-SSA-008 AddressOfSymbol must exclude exactly the addressed local");
}
}

// Registers CF-SSA-001..008 arena, metadata, mapping, and promotability matrices.
void registerSsaModelModule(TestRegistry& registry)
{
    addMatrix(registry, 1, "unit", arenaOracle);
    addMatrix(registry, 2, "unit", moduleOracle);
    addMatrix(registry, 3, "unit", provenanceOracle);
    addMatrix(registry, 4, "unit", promotedSetOracle);
    addMatrix(registry, 5, "unit", mappingOracle);
    addMatrix(registry, 6, "unit", excludedOracle);
    addMatrix(registry, 7, "unit", promotableOracle);
    addMatrix(registry, 8, "unit", addressTakenOracle);
}
}
