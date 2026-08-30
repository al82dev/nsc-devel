#pragma once

#include <iosfwd>

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirModule;
class MirValue;

class MirDumper final
{
public:
    void dump(const MirModule* module, std::ostream& output) const;

private:
    void dumpFunction(const MirFunction* function, std::ostream& output) const;
    void dumpBlock(const MirBlock* block, const MirFunction* function, std::ostream& output) const;
    void dumpInstruction(
        const MirInstruction* instruction,
        const MirFunction* function,
        std::ostream& output) const;
    void dumpSymbol(const sema::Symbol* symbol, const MirFunction* function, std::ostream& output) const;
    void dumpValue(const MirValue* value, std::ostream& output) const;
};
}
