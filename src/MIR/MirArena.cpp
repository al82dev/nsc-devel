#include "nativesharp/MIR/MirArena.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"

#include <utility>

namespace nativesharp::mir
{
MirArena::~MirArena()
{
    for (MirInstruction* value : instructions_) { delete value; }
    for (MirValue* value : values_) { delete value; }
    for (MirBlock* value : blocks_) { delete value; }
    for (MirFunction* value : functions_) { delete value; }
    for (MirGlobal* value : globals_) { delete value; }
    for (MirModule* value : modules_) { delete value; }
}

MirModule* MirArena::makeModule()
{
    MirModule* value = new MirModule();
    modules_.push_back(value);
    return value;
}

MirGlobal* MirArena::makeGlobal(sema::Symbol* symbol)
{
    MirGlobal* value = new MirGlobal(symbol);
    globals_.push_back(value);
    return value;
}

MirFunction* MirArena::makeFunction(sema::Symbol* symbol)
{
    MirFunction* value = new MirFunction(symbol);
    functions_.push_back(value);
    return value;
}

MirBlock* MirArena::makeBlock(std::string name)
{
    MirBlock* value = new MirBlock(nextBlockId_++, std::move(name));
    blocks_.push_back(value);
    return value;
}

MirInstruction* MirArena::makeInstruction(MirOpcode opcode, SourceRange range)
{
    MirInstruction* value = new MirInstruction(opcode, range);
    instructions_.push_back(value);
    return value;
}

MirValue* MirArena::makeValue(sema::Type* type)
{
    MirValue* value = new MirValue(nextValueId_++, type);
    values_.push_back(value);
    return value;
}

const std::vector<MirValue*>& MirArena::values() const noexcept { return values_; }
}
