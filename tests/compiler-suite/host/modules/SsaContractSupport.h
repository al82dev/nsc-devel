#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::ssa_contract_support
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::mir::MirBlock;
using nativesharp::mir::MirFunction;
using nativesharp::mir::MirInstruction;
using nativesharp::mir::MirOpcode;
using nativesharp::ssa::SsaFunction;
using nativesharp::ssa::SsaModule;
using MatrixOracle = std::function<void(TestContext&, std::size_t)>;

// Formats one stable SSA case identifier. Complexity is O(1).
inline std::string caseId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "TF-SSA-%03zu", ordinal);
    return buffer.data();
}

// Formats one stable SSA contract identifier. Complexity is O(1).
inline std::string contractId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "CF-SSA-%03zu", ordinal);
    return buffer.data();
}

// Registers all eight canonical variants of one SSA contract. Complexity is O(1).
inline void addMatrix(TestRegistry& registry, std::size_t contract, std::string layer, MatrixOracle oracle)
{
    const std::size_t first = ((contract - 1) * 8) + 1;
    for (std::size_t variant = 0; variant < 8; ++variant)
    {
        registry.add(TestCase{
            caseId(first + variant),
            contractId(contract),
            "Host.SSA",
            layer,
            variant == 7 ? "NIGHTLY" : "PR",
            {"ssa", "phase2", "matrix-" + std::to_string(variant + 1)},
            [variant, oracle](TestContext& context) { oracle(context, variant); },
        });
    }
}

// Compiles one isolated source through SSA and retains every producer lifetime. Complexity follows the pipeline.
inline std::unique_ptr<FrontendCompilation> compileSsa(
    TestContext& context, std::string_view label, std::string source)
{
    auto compilation = std::make_unique<FrontendCompilation>();
    const bool completed = compilation->compileBuffer(
        std::filesystem::path(std::string(label) + ".ns"), std::move(source), false);
    context.expect(completed, std::string(label) + " must complete the frontend pipeline");
    context.expect(compilation->ssaModule() != nullptr, std::string(label) + " must produce an SSA module");
    context.expect(compilation->ssaError().empty(), std::string(label) + " must not report an SSA error");
    return compilation;
}

// Finds defined-function metadata by stable symbol name. Complexity is O(f).
inline SsaFunction* findFunction(const SsaModule* module, std::string_view name)
{
    if (module == nullptr) return nullptr;
    for (SsaFunction* function : module->functions())
    {
        if (function != nullptr && function->function() != nullptr &&
            function->function()->symbol() != nullptr && function->function()->symbol()->name() == name)
        {
            return function;
        }
    }
    return nullptr;
}

// Finds one instruction iteratively in output block order. Complexity is O(i).
inline MirInstruction* findInstruction(SsaFunction* function, MirOpcode opcode)
{
    if (function == nullptr || function->function() == nullptr) return nullptr;
    for (MirBlock* block : function->function()->blocks())
    {
        if (block == nullptr) continue;
        for (MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == opcode) return instruction;
        }
    }
    return nullptr;
}

// Counts one opcode over the complete output function. Complexity is O(i).
inline std::size_t countOpcode(const SsaFunction* function, MirOpcode opcode)
{
    std::size_t count = 0;
    if (function == nullptr || function->function() == nullptr) return count;
    for (const MirBlock* block : function->function()->blocks())
    {
        if (block == nullptr) continue;
        for (const MirInstruction* instruction : block->instructions())
        {
            count += instruction != nullptr && instruction->opcode() == opcode ? 1 : 0;
        }
    }
    return count;
}
}
