#include "nativesharp/MIR/MirBlock.h"

#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"

#include <utility>

#include <utility>

namespace nativesharp::mir
{
MirBlock::MirBlock(std::uint32_t id, std::string name) : id_(id), name_(std::move(name)) {}
std::uint32_t MirBlock::id() const noexcept { return id_; }
const std::string& MirBlock::name() const noexcept { return name_; }
void MirBlock::addInstruction(MirInstruction* instruction) { instructions_.push_back(instruction); }

void MirBlock::replaceInstructions(std::vector<MirInstruction*> instructions)
{
    instructions_ = std::move(instructions);
}
const std::vector<MirInstruction*>& MirBlock::instructions() const noexcept { return instructions_; }
bool MirBlock::terminated() const noexcept
{
    return !instructions_.empty() && isMirTerminator(instructions_.back()->opcode());
}
}
