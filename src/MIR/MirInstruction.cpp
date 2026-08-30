#include "nativesharp/MIR/MirInstruction.h"

#include <utility>

namespace nativesharp::mir
{
MirInstruction::MirInstruction(MirOpcode opcode, SourceRange range) : opcode_(opcode), range_(range) {}
MirOpcode MirInstruction::opcode() const noexcept { return opcode_; }
SourceRange MirInstruction::range() const noexcept { return range_; }

void MirInstruction::resetTo(MirOpcode opcode) noexcept
{
    opcode_ = opcode;
    operands_.clear();
    phiIncoming_.clear();
    symbol_ = nullptr;
    auxiliaryType_ = nullptr;
    hasConstant_ = false;
    constant_ = sema::ConstantValue();
    hasConversion_ = false;
    conversionKind_ = sema::ConversionKind::Identity;
    hasImmediate_ = false;
    immediate_ = 0;
    hasOffset_ = false;
    offset_ = 0;
    firstTarget_ = nullptr;
    secondTarget_ = nullptr;
}
MirValue* MirInstruction::result() const noexcept { return result_; }
void MirInstruction::setResult(MirValue* value) noexcept { result_ = value; }
void MirInstruction::addOperand(MirValue* value) { operands_.push_back(value); }

void MirInstruction::setOperand(std::size_t index, MirValue* value)
{
    if (index < operands_.size())
    {
        operands_[index] = value;
    }
}

void MirInstruction::clearOperands() noexcept { operands_.clear(); }

void MirInstruction::replaceValueUse(const MirValue* from, MirValue* to) noexcept
{
    for (MirValue*& operand : operands_)
    {
        if (operand == from)
        {
            operand = to;
        }
    }
    for (MirPhiIncoming& incoming : phiIncoming_)
    {
        if (incoming.value == from)
        {
            incoming.value = to;
        }
    }
}

const std::vector<MirValue*>& MirInstruction::operands() const noexcept { return operands_; }
void MirInstruction::addPhiIncoming(MirBlock* block, MirValue* value)
{
    phiIncoming_.push_back({block, value});
}
void MirInstruction::clearPhiIncoming() noexcept { phiIncoming_.clear(); }
const std::vector<MirPhiIncoming>& MirInstruction::phiIncoming() const noexcept { return phiIncoming_; }
sema::Symbol* MirInstruction::symbol() const noexcept { return symbol_; }
void MirInstruction::setSymbol(sema::Symbol* symbol) noexcept { symbol_ = symbol; }
sema::Type* MirInstruction::auxiliaryType() const noexcept { return auxiliaryType_; }
void MirInstruction::setAuxiliaryType(sema::Type* type) noexcept { auxiliaryType_ = type; }
bool MirInstruction::hasConstant() const noexcept { return hasConstant_; }
const sema::ConstantValue& MirInstruction::constant() const noexcept { return constant_; }
void MirInstruction::setConstant(sema::ConstantValue value)
{
    constant_ = std::move(value);
    hasConstant_ = true;
}
bool MirInstruction::hasConversion() const noexcept { return hasConversion_; }
sema::ConversionKind MirInstruction::conversionKind() const noexcept { return conversionKind_; }
void MirInstruction::setConversionKind(sema::ConversionKind kind) noexcept
{
    conversionKind_ = kind;
    hasConversion_ = true;
}
bool MirInstruction::hasImmediate() const noexcept { return hasImmediate_; }
std::uint64_t MirInstruction::immediate() const noexcept { return immediate_; }
void MirInstruction::setImmediate(std::uint64_t value) noexcept
{
    immediate_ = value;
    hasImmediate_ = true;
}
bool MirInstruction::hasOffset() const noexcept { return hasOffset_; }
std::uint64_t MirInstruction::offset() const noexcept { return offset_; }
void MirInstruction::setOffset(std::uint64_t value) noexcept
{
    offset_ = value;
    hasOffset_ = true;
}
MirBlock* MirInstruction::firstTarget() const noexcept { return firstTarget_; }
MirBlock* MirInstruction::secondTarget() const noexcept { return secondTarget_; }
void MirInstruction::setFirstTarget(MirBlock* block) noexcept { firstTarget_ = block; }
void MirInstruction::setSecondTarget(MirBlock* block) noexcept { secondTarget_ = block; }
}
