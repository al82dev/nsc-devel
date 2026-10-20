#pragma once

#include "nativesharp/MIR/MirPhiIncoming.h"

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nativesharp::sema
{
class Symbol;
class Type;
}

namespace nativesharp::mir
{
class MirBlock;
class MirValue;

class MirInstruction final
{
public:
    MirInstruction(MirOpcode opcode, SourceRange range);

    MirOpcode opcode() const noexcept;
    SourceRange range() const noexcept;
    void resetTo(MirOpcode opcode) noexcept;

    MirValue* result() const noexcept;
    void setResult(MirValue* value) noexcept;

    void addOperand(MirValue* value);
    void setOperand(std::size_t index, MirValue* value);
    void clearOperands() noexcept;
    void replaceValueUse(const MirValue* from, MirValue* to) noexcept;
    const std::vector<MirValue*>& operands() const noexcept;

    void addPhiIncoming(MirBlock* block, MirValue* value);
    void clearPhiIncoming() noexcept;
    const std::vector<MirPhiIncoming>& phiIncoming() const noexcept;

    sema::Symbol* symbol() const noexcept;
    void setSymbol(sema::Symbol* symbol) noexcept;

    sema::Type* auxiliaryType() const noexcept;
    void setAuxiliaryType(sema::Type* type) noexcept;

    bool hasConstant() const noexcept;
    const sema::ConstantValue& constant() const noexcept;
    void setConstant(sema::ConstantValue value);

    bool hasConversion() const noexcept;
    sema::ConversionKind conversionKind() const noexcept;
    void setConversionKind(sema::ConversionKind kind) noexcept;

    bool hasImmediate() const noexcept;
    std::uint64_t immediate() const noexcept;
    void setImmediate(std::uint64_t value) noexcept;

    bool hasOffset() const noexcept;
    std::uint64_t offset() const noexcept;
    void setOffset(std::uint64_t value) noexcept;

    MirBlock* firstTarget() const noexcept;
    MirBlock* secondTarget() const noexcept;
    void setFirstTarget(MirBlock* block) noexcept;
    void setSecondTarget(MirBlock* block) noexcept;

private:
    MirOpcode opcode_;
    SourceRange range_;
    MirValue* result_ = nullptr;
    std::vector<MirValue*> operands_;
    std::vector<MirPhiIncoming> phiIncoming_;
    sema::Symbol* symbol_ = nullptr;
    sema::Type* auxiliaryType_ = nullptr;
    bool hasConstant_ = false;
    sema::ConstantValue constant_;
    bool hasConversion_ = false;
    sema::ConversionKind conversionKind_ = sema::ConversionKind::Identity;
    bool hasImmediate_ = false;
    std::uint64_t immediate_ = 0;
    bool hasOffset_ = false;
    std::uint64_t offset_ = 0;
    MirBlock* firstTarget_ = nullptr;
    MirBlock* secondTarget_ = nullptr;
};
}
