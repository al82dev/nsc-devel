#pragma once

#include "nativesharp/Passes/OptimizationArena.h"

#include <string>
#include <unordered_map>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirModule;
class MirValue;
}

namespace nativesharp::ssa
{
class SsaModule;
}

namespace nativesharp::passes
{
class OptimizationModule;

class OptimizationBuilder final
{
public:
    explicit OptimizationBuilder(OptimizationArena& arena);

    OptimizationModule* build(const ssa::SsaModule* source);
    const std::string& error() const noexcept;

private:
    bool cloneModule(const mir::MirModule* source, mir::MirModule* output);
    bool cloneFunction(const mir::MirFunction* source, mir::MirModule* output);
    bool cloneDefinedFunction(const mir::MirFunction* source, mir::MirFunction* output);
    bool cloneInstruction(
        const mir::MirInstruction* source,
        mir::MirInstruction* output);
    bool fail(std::string message);

    OptimizationArena& arena_;
    std::unordered_map<const mir::MirBlock*, mir::MirBlock*> blocks_;
    std::unordered_map<const mir::MirValue*, mir::MirValue*> values_;
    std::string error_;
};
}
