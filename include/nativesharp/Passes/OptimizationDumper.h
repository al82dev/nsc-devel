#pragma once

#include <iosfwd>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirValue;
}

namespace nativesharp::passes
{
class OptimizationModule;

class OptimizationDumper final
{
public:
    void dump(
        const OptimizationModule* module,
        std::ostream& output) const;

private:
    void dumpFunction(
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpBlock(
        const mir::MirBlock* block,
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpInstruction(
        const mir::MirInstruction* instruction,
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpValue(
        const mir::MirValue* value,
        std::ostream& output) const;
};
}
