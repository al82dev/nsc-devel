#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Value-only representation; NSMeta persists the limited SysV record form consumed by the backend.
namespace nativesharp::interop::abi_plan
{
enum class TypeKind { Void, Integer, Float, Double, Pointer, Structure, Array, Vector };
struct Type
{
    TypeKind kind = TypeKind::Void;
    std::uint64_t widthOrCount = 0;
    bool packed = false;
    std::vector<Type> elements;
};
enum class Passing { Direct, Extend, Indirect, Ignore, Expand };
struct LogicalValue
{
    Passing passing = Passing::Ignore;
    std::optional<Type> coercion;
    std::uint64_t offset = 0;
    std::uint64_t alignment = 0;
    std::uint64_t firstPhysical = 0;
    std::uint64_t physicalCount = 0;
    bool flatten = false;
    bool byValue = false;
    bool signExtend = false;
    bool zeroExtend = false;
    // Byte offsets of the physical parameters of an expanded aggregate.
    // Empty for all other passing kinds.
    std::vector<std::uint64_t> expandedOffsets;
};
struct PhysicalParameter
{
    Type type;
    std::optional<Type> pointee;
    std::uint64_t alignment = 0;
    bool structureReturn = false;
    bool byValue = false;
    bool signExtend = false;
    bool zeroExtend = false;
    bool inRegister = false;
    bool noAlias = false;
};
struct Plan
{
    std::string symbol;
    std::string targetTriple;
    std::string dataLayout;
    unsigned callingConvention = 0;
    bool variadic = false;
    bool returnSignExtend = false;
    bool returnZeroExtend = false;
    Type physicalReturn;
    LogicalValue logicalReturn;
    std::vector<LogicalValue> logicalParameters;
    std::vector<PhysicalParameter> physicalParameters;
    // A set entry denotes a physical size_t after this logical parameter.
    std::vector<std::uint8_t> objectSizeAfter;
};

// Classification of one actual variadic call; it does not itself authorize emission.
struct VariadicCallPlan
{
    std::string symbol;
    std::string targetTriple;
    std::string dataLayout;
    unsigned callingConvention = 0;
    std::size_t requiredArguments = 0;
    LogicalValue result;
    std::vector<LogicalValue> arguments;
    Type physicalReturn;
    bool returnSignExtend = false;
    bool returnZeroExtend = false;
    std::vector<PhysicalParameter> physicalParameters;
    // Optional bitmap indexed by the required logical parameters. A set entry
    // denotes an additional size_t immediately after that physical argument.
    std::vector<std::uint8_t> objectSizeAfter;
};

// Supported aggregate shapes: one small record passed and returned directly.
inline bool isSysVSmallRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto bits = plan.physicalReturn.widthOrCount;
    const auto integer = [bits](const Type& type) {
        return type.kind == TypeKind::Integer && type.widthOrCount == bits;
    };
    if ((bits < 8 || bits > 64 || bits % 8 != 0) ||
        plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || !integer(plan.physicalReturn) ||
        plan.logicalReturn.passing != Passing::Direct || !plan.logicalReturn.coercion ||
        !integer(*plan.logicalReturn.coercion) || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 1) return false;
    const auto& logical = plan.logicalParameters[0];
    const auto& physical = plan.physicalParameters[0];
    return logical.passing == Passing::Direct && logical.coercion && integer(*logical.coercion) &&
        logical.offset == 0 && logical.firstPhysical == 0 && logical.physicalCount == 1 &&
        integer(physical.type) && !physical.structureReturn && !physical.byValue &&
        !physical.signExtend && !physical.zeroExtend && !physical.inRegister && !physical.noAlias;
}
inline bool isSysVThreeByteRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    return plan.physicalReturn.widthOrCount == 24 && isSysVSmallRecordRoundTrip(plan, symbol);
}
}
