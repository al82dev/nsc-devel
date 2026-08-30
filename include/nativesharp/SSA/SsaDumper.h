#pragma once

#include <iosfwd>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirValue;
}

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::ssa
{
class SsaFunction;
class SsaModule;

class SsaDumper final
{
public:
    void dump(const SsaModule* module, std::ostream& output) const;

private:
    void dumpFunction(const SsaFunction* function, std::ostream& output) const;
    void dumpBlock(
        const mir::MirBlock* block,
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpInstruction(
        const mir::MirInstruction* instruction,
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpSymbol(
        const sema::Symbol* symbol,
        const mir::MirFunction* function,
        std::ostream& output) const;
    void dumpValue(const mir::MirValue* value, std::ostream& output) const;
};
}
