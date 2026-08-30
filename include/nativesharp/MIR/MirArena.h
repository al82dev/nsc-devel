#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/MIR/MirOpcode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::sema
{
class Symbol;
class Type;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirGlobal;
class MirInstruction;
class MirModule;
class MirValue;

class MirArena final
{
public:
    MirArena() = default;
    MirArena(const MirArena&) = delete;
    MirArena& operator=(const MirArena&) = delete;
    ~MirArena();

    MirModule* makeModule();
    MirGlobal* makeGlobal(sema::Symbol* symbol);
    MirFunction* makeFunction(sema::Symbol* symbol);
    MirBlock* makeBlock(std::string name);
    MirInstruction* makeInstruction(MirOpcode opcode, SourceRange range);
    MirValue* makeValue(sema::Type* type);

    const std::vector<MirValue*>& values() const noexcept;

private:
    std::uint32_t nextBlockId_ = 0;
    std::uint32_t nextValueId_ = 0;
    std::vector<MirModule*> modules_;
    std::vector<MirGlobal*> globals_;
    std::vector<MirFunction*> functions_;
    std::vector<MirBlock*> blocks_;
    std::vector<MirInstruction*> instructions_;
    std::vector<MirValue*> values_;
};
}
