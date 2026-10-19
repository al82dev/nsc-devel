#include "nativesharp/Passes/ConstantFolder.h"

#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nativesharp::passes
{
namespace
{
bool isSignedInteger(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return false;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Short:
        case sema::TypeKind::Int:
        case sema::TypeKind::Long:
        case sema::TypeKind::NInt:
        case sema::TypeKind::Enum:
            return true;
        default:
            return false;
    }
}

bool isUnsignedInteger(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return false;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
        case sema::TypeKind::UShort:
        case sema::TypeKind::UInt:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NUInt:
            return true;
        default:
            return false;
    }
}

bool isInteger(const sema::Type* type) noexcept
{
    return isSignedInteger(type) || isUnsignedInteger(type);
}

bool isFloating(const sema::Type* type) noexcept
{
    return type != nullptr &&
        (type->kind() == sema::TypeKind::Float ||
         type->kind() == sema::TypeKind::Double);
}

std::uint32_t integerWidth(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return 0;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
            return 8;
        case sema::TypeKind::Short:
        case sema::TypeKind::UShort:
            return 16;
        case sema::TypeKind::Int:
        case sema::TypeKind::UInt:
        case sema::TypeKind::Enum:
            return 32;
        case sema::TypeKind::Long:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NInt:
        case sema::TypeKind::NUInt:
            return 64;
        default:
            return 0;
    }
}

std::uint64_t maskForWidth(std::uint32_t width) noexcept
{
    return width >= 64
        ? std::numeric_limits<std::uint64_t>::max()
        : ((std::uint64_t{1} << width) - 1);
}

std::int64_t signedFromBits(std::uint64_t bits, std::uint32_t width) noexcept
{
    if (width >= 64)
    {
        return std::bit_cast<std::int64_t>(bits);
    }

    const std::uint64_t mask = maskForWidth(width);
    bits &= mask;
    const std::uint64_t sign = std::uint64_t{1} << (width - 1);
    if ((bits & sign) == 0)
    {
        return static_cast<std::int64_t>(bits);
    }

    const std::uint64_t magnitude = ((~bits) + 1) & mask;
    return -static_cast<std::int64_t>(magnitude);
}

std::uint64_t bitsFromSigned(std::int64_t value, std::uint32_t width) noexcept
{
    return std::bit_cast<std::uint64_t>(value) & maskForWidth(width);
}

std::optional<std::uint64_t> integerBits(
    const sema::ConstantValue& value,
    const sema::Type* type) noexcept
{
    const std::uint32_t width = integerWidth(type);
    if (width == 0)
    {
        return std::nullopt;
    }

    if (value.kind() == sema::ConstantKind::SignedInteger)
    {
        return bitsFromSigned(value.signedInteger(), width);
    }
    if (value.kind() == sema::ConstantKind::UnsignedInteger)
    {
        return value.unsignedInteger() & maskForWidth(width);
    }
    return std::nullopt;
}

sema::ConstantValue integerFromBits(
    std::uint64_t bits,
    const sema::Type* type) noexcept
{
    const std::uint32_t width = integerWidth(type);
    bits &= maskForWidth(width);

    if (isSignedInteger(type))
    {
        return sema::ConstantValue::signedInteger(signedFromBits(bits, width));
    }
    return sema::ConstantValue::unsignedInteger(bits);
}

std::optional<std::int64_t> shiftCount(const sema::ConstantValue& value) noexcept
{
    if (value.kind() == sema::ConstantKind::SignedInteger)
    {
        return value.signedInteger();
    }
    if (value.kind() == sema::ConstantKind::UnsignedInteger &&
        value.unsignedInteger() <=
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        return static_cast<std::int64_t>(value.unsignedInteger());
    }
    return std::nullopt;
}

std::optional<sema::ConstantValue> convertConstant(
    const sema::ConstantValue& value,
    const sema::Type* source,
    const sema::Type* destination)
{
    if (source == nullptr || destination == nullptr)
    {
        return std::nullopt;
    }

    if (isInteger(source) && isInteger(destination))
    {
        const std::optional<std::uint64_t> bits = integerBits(value, source);
        if (!bits.has_value())
        {
            return std::nullopt;
        }
        return integerFromBits(*bits, destination);
    }

    if (isInteger(source) && isFloating(destination))
    {
        double number = 0.0;
        if (value.kind() == sema::ConstantKind::SignedInteger)
        {
            number = static_cast<double>(value.signedInteger());
        }
        else if (value.kind() == sema::ConstantKind::UnsignedInteger)
        {
            number = static_cast<double>(value.unsignedInteger());
        }
        else
        {
            return std::nullopt;
        }

        if (destination->kind() == sema::TypeKind::Float)
        {
            number = static_cast<double>(static_cast<float>(number));
        }
        return sema::ConstantValue::floating(number);
    }

    if (isFloating(source) && isFloating(destination) &&
        value.kind() == sema::ConstantKind::Floating)
    {
        double number = value.floating();
        if (destination->kind() == sema::TypeKind::Float)
        {
            number = static_cast<double>(static_cast<float>(number));
        }
        return sema::ConstantValue::floating(number);
    }

    if (isFloating(source) && isInteger(destination) &&
        value.kind() == sema::ConstantKind::Floating)
    {
        const double number = value.floating();
        if (!std::isfinite(number))
        {
            return std::nullopt;
        }

        const long double truncated = std::trunc(static_cast<long double>(number));
        const std::uint32_t width = integerWidth(destination);
        if (isSignedInteger(destination))
        {
            const long double minimum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
                : -static_cast<long double>(std::uint64_t{1} << (width - 1));
            const long double maximum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                : static_cast<long double>((std::uint64_t{1} << (width - 1)) - 1);
            if (truncated < minimum || truncated > maximum)
            {
                return std::nullopt;
            }
            return sema::ConstantValue::signedInteger(
                static_cast<std::int64_t>(truncated));
        }

        const long double maximum =
            static_cast<long double>(maskForWidth(width));
        if (truncated < 0.0L || truncated > maximum)
        {
            return std::nullopt;
        }
        return sema::ConstantValue::unsignedInteger(
            static_cast<std::uint64_t>(truncated));
    }

    return std::nullopt;
}

std::optional<sema::ConstantValue> foldUnary(
    mir::MirOpcode opcode,
    const sema::ConstantValue& operand,
    const sema::Type* type)
{
    if (opcode == mir::MirOpcode::LogicalNot &&
        operand.kind() == sema::ConstantKind::Boolean)
    {
        return sema::ConstantValue::boolean(!operand.boolean());
    }

    if (isFloating(type) &&
        opcode == mir::MirOpcode::Negate &&
        operand.kind() == sema::ConstantKind::Floating)
    {
        double value = -operand.floating();
        if (type->kind() == sema::TypeKind::Float)
        {
            value = static_cast<double>(static_cast<float>(value));
        }
        return sema::ConstantValue::floating(value);
    }

    if (!isInteger(type))
    {
        return std::nullopt;
    }

    const std::optional<std::uint64_t> bits = integerBits(operand, type);
    if (!bits.has_value())
    {
        return std::nullopt;
    }

    std::uint64_t result = *bits;
    if (opcode == mir::MirOpcode::Negate)
    {
        result = (~result + 1) & maskForWidth(integerWidth(type));
    }
    else if (opcode == mir::MirOpcode::BitNot)
    {
        result = (~result) & maskForWidth(integerWidth(type));
    }
    else
    {
        return std::nullopt;
    }

    return integerFromBits(result, type);
}

std::optional<sema::ConstantValue> foldComparison(
    mir::MirOpcode opcode,
    const sema::ConstantValue& left,
    const sema::ConstantValue& right,
    const sema::Type* operandType)
{
    bool value = false;

    if (operandType->kind() == sema::TypeKind::Bool &&
        left.kind() == sema::ConstantKind::Boolean &&
        right.kind() == sema::ConstantKind::Boolean)
    {
        if (opcode == mir::MirOpcode::CompareEqual)
        {
            value = left.boolean() == right.boolean();
        }
        else if (opcode == mir::MirOpcode::CompareNotEqual)
        {
            value = left.boolean() != right.boolean();
        }
        else
        {
            return std::nullopt;
        }
        return sema::ConstantValue::boolean(value);
    }

    if (isFloating(operandType) &&
        left.kind() == sema::ConstantKind::Floating &&
        right.kind() == sema::ConstantKind::Floating)
    {
        const double a = left.floating();
        const double b = right.floating();
        switch (opcode)
        {
            case mir::MirOpcode::CompareEqual: value = a == b; break;
            case mir::MirOpcode::CompareNotEqual: value = a != b; break;
            case mir::MirOpcode::CompareLess: value = a < b; break;
            case mir::MirOpcode::CompareLessEqual: value = a <= b; break;
            case mir::MirOpcode::CompareGreater: value = a > b; break;
            case mir::MirOpcode::CompareGreaterEqual: value = a >= b; break;
            default: return std::nullopt;
        }
        return sema::ConstantValue::boolean(value);
    }

    if (!isInteger(operandType))
    {
        return std::nullopt;
    }

    const std::optional<std::uint64_t> aBits = integerBits(left, operandType);
    const std::optional<std::uint64_t> bBits = integerBits(right, operandType);
    if (!aBits.has_value() || !bBits.has_value())
    {
        return std::nullopt;
    }

    if (isSignedInteger(operandType))
    {
        const std::int64_t a = signedFromBits(*aBits, integerWidth(operandType));
        const std::int64_t b = signedFromBits(*bBits, integerWidth(operandType));
        switch (opcode)
        {
            case mir::MirOpcode::CompareEqual: value = a == b; break;
            case mir::MirOpcode::CompareNotEqual: value = a != b; break;
            case mir::MirOpcode::CompareLess: value = a < b; break;
            case mir::MirOpcode::CompareLessEqual: value = a <= b; break;
            case mir::MirOpcode::CompareGreater: value = a > b; break;
            case mir::MirOpcode::CompareGreaterEqual: value = a >= b; break;
            default: return std::nullopt;
        }
    }
    else
    {
        const std::uint64_t a = *aBits;
        const std::uint64_t b = *bBits;
        switch (opcode)
        {
            case mir::MirOpcode::CompareEqual: value = a == b; break;
            case mir::MirOpcode::CompareNotEqual: value = a != b; break;
            case mir::MirOpcode::CompareLess: value = a < b; break;
            case mir::MirOpcode::CompareLessEqual: value = a <= b; break;
            case mir::MirOpcode::CompareGreater: value = a > b; break;
            case mir::MirOpcode::CompareGreaterEqual: value = a >= b; break;
            default: return std::nullopt;
        }
    }

    return sema::ConstantValue::boolean(value);
}

std::optional<sema::ConstantValue> foldFloatingBinary(
    mir::MirOpcode opcode,
    const sema::ConstantValue& left,
    const sema::ConstantValue& right,
    const sema::Type* resultType)
{
    const double a = left.floating();
    const double b = right.floating();
    double result = 0.0;
    switch (opcode)
    {
        case mir::MirOpcode::Add: result = a + b; break;
        case mir::MirOpcode::Subtract: result = a - b; break;
        case mir::MirOpcode::Multiply: result = a * b; break;
        case mir::MirOpcode::Divide:
            if (b == 0.0)
            {
                return std::nullopt;
            }
            result = a / b;
            break;
        default:
            return std::nullopt;
    }
    if (resultType->kind() == sema::TypeKind::Float)
    {
        result = static_cast<double>(static_cast<float>(result));
    }
    return sema::ConstantValue::floating(result);
}

std::optional<sema::ConstantValue> foldShiftBinary(
    mir::MirOpcode opcode,
    std::uint64_t aBits,
    const sema::ConstantValue& right,
    const sema::Type* resultType,
    std::uint32_t width)
{
    const std::optional<std::int64_t> count = shiftCount(right);
    if (!count.has_value() || *count < 0 ||
        static_cast<std::uint64_t>(*count) >= width)
    {
        return std::nullopt;
    }
    const unsigned shift = static_cast<unsigned>(*count);
    std::uint64_t result = 0;
    if (opcode == mir::MirOpcode::ShiftLeft)
    {
        result = (aBits << shift) & maskForWidth(width);
    }
    else if (isSignedInteger(resultType))
    {
        const std::int64_t signedValue = signedFromBits(aBits, width);
        if (shift == 0)
        {
            result = aBits;
        }
        else if (signedValue >= 0)
        {
            result = aBits >> shift;
        }
        else
        {
            const std::uint64_t highMask = maskForWidth(width) ^ maskForWidth(width - shift);
            result = (aBits >> shift) | highMask;
        }
    }
    else
    {
        result = aBits >> shift;
    }
    return integerFromBits(result, resultType);
}

std::optional<sema::ConstantValue> foldIntegerBinary(
    mir::MirOpcode opcode,
    std::uint64_t aBits,
    const sema::ConstantValue& right,
    const sema::Type* resultType,
    const sema::Type* rightType,
    std::uint32_t width)
{
    const std::optional<std::uint64_t> bBits = integerBits(right, rightType);
    if (!bBits.has_value())
    {
        return std::nullopt;
    }
    const std::uint64_t a = aBits & maskForWidth(width);
    const std::uint64_t b = *bBits & maskForWidth(width);
    std::uint64_t result = 0;
    switch (opcode)
    {
        case mir::MirOpcode::Add: result = a + b; break;
        case mir::MirOpcode::Subtract: result = a - b; break;
        case mir::MirOpcode::Multiply: result = a * b; break;
        case mir::MirOpcode::BitAnd: result = a & b; break;
        case mir::MirOpcode::BitOr: result = a | b; break;
        case mir::MirOpcode::BitXor: result = a ^ b; break;

        case mir::MirOpcode::Divide:
        case mir::MirOpcode::Remainder:
            if (b == 0)
            {
                return std::nullopt;
            }
            if (isSignedInteger(resultType))
            {
                const std::int64_t signedA = signedFromBits(a, width);
                const std::int64_t signedB = signedFromBits(b, width);
                const std::int64_t minimum = width == 64
                    ? std::numeric_limits<std::int64_t>::min()
                    : -(std::int64_t{1} << (width - 1));
                if (signedA == minimum && signedB == -1)
                {
                    return std::nullopt;
                }
                const std::int64_t computed =
                    opcode == mir::MirOpcode::Divide
                        ? signedA / signedB
                        : signedA % signedB;
                result = bitsFromSigned(computed, width);
            }
            else
            {
                result = opcode == mir::MirOpcode::Divide ? a / b : a % b;
            }
            break;

        default:
            return std::nullopt;
    }
    return integerFromBits(result & maskForWidth(width), resultType);
}

std::optional<sema::ConstantValue> foldBinary(
    mir::MirOpcode opcode,
    const sema::ConstantValue& left,
    const sema::ConstantValue& right,
    const sema::Type* resultType,
    const sema::Type* rightType)
{
    if (isFloating(resultType) &&
        left.kind() == sema::ConstantKind::Floating &&
        right.kind() == sema::ConstantKind::Floating)
    {
        return foldFloatingBinary(opcode, left, right, resultType);
    }
    if (!isInteger(resultType)) return std::nullopt;
    const std::optional<std::uint64_t> aBits = integerBits(left, resultType);
    if (!aBits.has_value()) return std::nullopt;
    const std::uint32_t width = integerWidth(resultType);
    if (opcode == mir::MirOpcode::ShiftLeft || opcode == mir::MirOpcode::ShiftRight)
    {
        return foldShiftBinary(opcode, *aBits, right, resultType, width);
    }
    return foldIntegerBinary(opcode, *aBits, right, resultType, rightType, width);
}

const sema::ConstantValue* knownValue(
    const mir::MirValue* value,
    const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known)
{
    const auto found = known.find(value);
    return found == known.end() ? nullptr : &found->second;
}

std::optional<sema::ConstantValue> foldPhiInstruction(
    const mir::MirInstruction* instruction,
    const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known)
{
    const sema::ConstantValue* first = nullptr;
    for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
    {
        const sema::ConstantValue* value = knownValue(incoming.value, known);
        if (value == nullptr) return std::nullopt;
        if (first == nullptr)
        {
            first = value;
        }
        else if (!ConstantFolder::equal(*first, *value))
        {
            return std::nullopt;
        }
    }
    return first == nullptr
        ? std::nullopt
        : std::optional<sema::ConstantValue>(*first);
}

std::optional<sema::ConstantValue> foldSingleOperandInstruction(
    const mir::MirInstruction* instruction,
    const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known)
{
    const sema::ConstantValue* operand = knownValue(instruction->operands()[0], known);
    if (operand == nullptr) return std::nullopt;
    if (instruction->opcode() == mir::MirOpcode::Convert)
    {
        return convertConstant(
            *operand,
            instruction->operands()[0]->type(),
            instruction->result()->type());
    }
    return foldUnary(instruction->opcode(), *operand, instruction->result()->type());
}

std::optional<sema::ConstantValue> foldTwoOperandInstruction(
    const mir::MirInstruction* instruction,
    const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known)
{
    if (instruction->operands().size() != 2) return std::nullopt;
    const sema::ConstantValue* left = knownValue(instruction->operands()[0], known);
    const sema::ConstantValue* right = knownValue(instruction->operands()[1], known);
    if (left == nullptr || right == nullptr) return std::nullopt;
    using mir::MirOpcode;
    switch (instruction->opcode())
    {
        case MirOpcode::CompareEqual:
        case MirOpcode::CompareNotEqual:
        case MirOpcode::CompareLess:
        case MirOpcode::CompareLessEqual:
        case MirOpcode::CompareGreater:
        case MirOpcode::CompareGreaterEqual:
            return foldComparison(
                instruction->opcode(),
                *left,
                *right,
                instruction->operands()[0]->type());
        case MirOpcode::Add:
        case MirOpcode::Subtract:
        case MirOpcode::Multiply:
        case MirOpcode::Divide:
        case MirOpcode::Remainder:
        case MirOpcode::BitAnd:
        case MirOpcode::BitOr:
        case MirOpcode::BitXor:
        case MirOpcode::ShiftLeft:
        case MirOpcode::ShiftRight:
            return foldBinary(
                instruction->opcode(),
                *left,
                *right,
                instruction->result()->type(),
                instruction->operands()[1]->type());
        default:
            return std::nullopt;
    }
}
}

std::optional<sema::ConstantValue> ConstantFolder::fold(
    const mir::MirInstruction* instruction,
    const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known)
{
    if (instruction == nullptr)
    {
        return std::nullopt;
    }

    using mir::MirOpcode;
    if (instruction->opcode() == MirOpcode::Constant && instruction->hasConstant())
    {
        return instruction->constant();
    }

    if (instruction->opcode() == MirOpcode::Copy &&
        instruction->operands().size() == 1)
    {
        const sema::ConstantValue* value = knownValue(instruction->operands()[0], known);
        return value == nullptr
            ? std::nullopt
            : std::optional<sema::ConstantValue>(*value);
    }

    if (instruction->opcode() == MirOpcode::Phi)
    {
        return foldPhiInstruction(instruction, known);
    }

    if (instruction->result() == nullptr)
    {
        return std::nullopt;
    }

    if (instruction->opcode() == MirOpcode::Convert &&
        instruction->operands().size() == 1)
    {
        return foldSingleOperandInstruction(instruction, known);
    }

    if ((instruction->opcode() == MirOpcode::Negate ||
         instruction->opcode() == MirOpcode::BitNot ||
         instruction->opcode() == MirOpcode::LogicalNot) &&
        instruction->operands().size() == 1)
    {
        return foldSingleOperandInstruction(instruction, known);
    }
    return foldTwoOperandInstruction(instruction, known);
}

bool ConstantFolder::equal(
    const sema::ConstantValue& left,
    const sema::ConstantValue& right) noexcept
{
    if (left.kind() != right.kind())
    {
        return false;
    }

    switch (left.kind())
    {
        case sema::ConstantKind::None:
            return true;
        case sema::ConstantKind::SignedInteger:
            return left.signedInteger() == right.signedInteger();
        case sema::ConstantKind::UnsignedInteger:
            return left.unsignedInteger() == right.unsignedInteger();
        case sema::ConstantKind::Floating:
            return std::bit_cast<std::uint64_t>(left.floating()) ==
                std::bit_cast<std::uint64_t>(right.floating());
        case sema::ConstantKind::Boolean:
            return left.boolean() == right.boolean();
        case sema::ConstantKind::Null:
            return true;
        case sema::ConstantKind::String:
            return left.string() == right.string();
    }
    return false;
}
}
