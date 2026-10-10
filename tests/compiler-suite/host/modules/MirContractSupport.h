#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirDumper.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite::mir_contract_support
{
using nativesharp::SourceRange;
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirArena;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirModule;
using nativesharp::mir::MirOpcode;
using nativesharp::mir::MirValue;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;
using MatrixOracle = std::function<void(TestContext&, std::size_t)>;

inline std::string caseId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "TF-MIR-%03zu", ordinal);
    return buffer.data();
}

inline std::string contractId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "CF-MIR-%03zu", ordinal);
    return buffer.data();
}

// Registers the eight canonical variants without collapsing their individual identities.
inline void addMatrix(TestRegistry& registry, std::size_t contract, std::string layer, MatrixOracle oracle)
{
    const std::size_t first = ((contract - 1) * 8) + 1;
    for (std::size_t variant = 0; variant < 8; ++variant)
    {
        registry.add(TestCase{
            caseId(first + variant),
            contractId(contract),
            "Host.MIR",
            layer,
            variant == 7 ? "NIGHTLY" : "PR",
            {"mir", "phase2", "matrix-" + std::to_string(variant + 1)},
            [variant, oracle](TestContext& context) { oracle(context, variant); },
        });
    }
}

inline std::unique_ptr<FrontendCompilation> compileMir(
    TestContext& context, std::string_view label, std::string source)
{
    auto compilation = std::make_unique<FrontendCompilation>();
    const bool completed = compilation->compileBuffer(
        std::filesystem::path(std::string(label) + ".ns"), std::move(source), false);
    context.expect(completed, std::string(label) + " must complete the frontend pipeline");
    context.expect(compilation->mirModule() != nullptr, std::string(label) + " must produce MIR");
    context.expect(compilation->mirError().empty(), std::string(label) + " must not report a MIR error");
    return compilation;
}

inline MirFunction* findFunction(MirModule* module, std::string_view name)
{
    if (module == nullptr) return nullptr;
    for (MirFunction* function : module->functions())
    {
        if (function != nullptr && function->symbol() != nullptr && function->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

inline const MirFunction* findFunction(const MirModule* module, std::string_view name)
{
    if (module == nullptr) return nullptr;
    for (const MirFunction* function : module->functions())
    {
        if (function != nullptr && function->symbol() != nullptr && function->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

inline MirBlock* findBlock(MirFunction* function, std::string_view prefix)
{
    if (function == nullptr) return nullptr;
    for (MirBlock* block : function->blocks())
    {
        if (block != nullptr && block->name().starts_with(prefix)) return block;
    }
    return nullptr;
}

inline const MirBlock* findBlock(const MirFunction* function, std::string_view prefix)
{
    if (function == nullptr) return nullptr;
    for (const MirBlock* block : function->blocks())
    {
        if (block != nullptr && block->name().starts_with(prefix)) return block;
    }
    return nullptr;
}

inline MirInstruction* findInstruction(MirFunction* function, MirOpcode opcode)
{
    if (function == nullptr) return nullptr;
    for (MirBlock* block : function->blocks())
    {
        if (block == nullptr) continue;
        for (MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode) return instruction;
        }
    }
    return nullptr;
}

inline const MirInstruction* findInstruction(const MirFunction* function, MirOpcode opcode)
{
    if (function == nullptr) return nullptr;
    for (const MirBlock* block : function->blocks())
    {
        if (block == nullptr) continue;
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode) return instruction;
        }
    }
    return nullptr;
}

inline std::size_t countOpcode(const MirFunction* function, MirOpcode opcode)
{
    std::size_t count = 0;
    if (function == nullptr) return count;
    for (const MirBlock* block : function->blocks())
    {
        if (block == nullptr) continue;
        for (const MirInstruction* instruction : block->instructions())
        {
            count += instruction != nullptr && instruction->opcode() == opcode ? 1 : 0;
        }
    }
    return count;
}

struct FunctionFixture
{
    TypeContext types;
    Type* functionType = nullptr;
    Symbol functionSymbol;
    MirArena arena;
    MirModule* module = nullptr;
    MirFunction* function = nullptr;
    MirBlock* entry = nullptr;

    explicit FunctionFixture(TypeKind resultKind = TypeKind::Void, bool defined = true)
        : functionType(types.functionType(types.builtin(resultKind), {})),
          functionSymbol(SymbolKind::Function, "F", functionType, nullptr)
    {
        module = arena.makeModule();
        function = arena.makeFunction(&functionSymbol);
        function->setDefined(defined);
        module->addFunction(function);
        if (defined)
        {
            entry = arena.makeBlock("entry");
            function->addBlock(entry);
        }
    }

    MirInstruction* addReturn()
    {
        MirInstruction* instruction = arena.makeInstruction(MirOpcode::Return, SourceRange());
        entry->addInstruction(instruction);
        return instruction;
    }
};

inline std::string dumpModule(const MirModule* module)
{
    std::ostringstream output;
    nativesharp::mir::MirDumper dumper;
    dumper.dump(module, output);
    return output.str();
}
}
