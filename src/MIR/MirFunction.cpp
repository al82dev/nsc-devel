#include "nativesharp/MIR/MirFunction.h"

#include <utility>

namespace nativesharp::mir
{
MirFunction::MirFunction(sema::Symbol* symbol) : symbol_(symbol) {}
sema::Symbol* MirFunction::symbol() const noexcept { return symbol_; }
bool MirFunction::defined() const noexcept { return defined_; }
void MirFunction::setDefined(bool value) noexcept { defined_ = value; }
void MirFunction::addParameter(MirValue* value) { parameters_.push_back(value); }
const std::vector<MirValue*>& MirFunction::parameters() const noexcept { return parameters_; }
void MirFunction::addLocal(sema::Symbol* symbol) { locals_.push_back(symbol); }
const std::vector<sema::Symbol*>& MirFunction::locals() const noexcept { return locals_; }
void MirFunction::addBlock(MirBlock* block) { blocks_.push_back(block); }

void MirFunction::replaceBlocks(std::vector<MirBlock*> blocks)
{
    blocks_ = std::move(blocks);
}
const std::vector<MirBlock*>& MirFunction::blocks() const noexcept { return blocks_; }
MirBlock* MirFunction::entryBlock() const noexcept { return blocks_.empty() ? nullptr : blocks_.front(); }
}
