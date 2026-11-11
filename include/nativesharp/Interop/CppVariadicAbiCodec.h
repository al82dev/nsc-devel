#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"
#include <limits>
#include <utility>

namespace nativesharp::interop::abi_plan
{
// Internal cache encoding only. A decoded plan still needs semantic validation.
namespace variadic_cache_detail
{
inline constexpr std::size_t maxBytes = 4U * 1024U * 1024U;
inline constexpr std::size_t maxArguments = 1024;
inline constexpr std::size_t maxPhysical = 2049;
inline constexpr std::size_t maxElements = 1024;

struct Writer
{
    std::string bytes;
    bool valid = true;
    void u8(std::uint8_t value) { if (bytes.size() >= maxBytes) valid = false; else bytes.push_back(static_cast<char>(value)); }
    void u64(std::uint64_t value) { for (unsigned i = 0; i < 8; ++i) u8(static_cast<std::uint8_t>(value >> (i * 8))); }
    void text(std::string_view value)
    {
        if (value.size() > maxBytes || bytes.size() > maxBytes - value.size() || maxBytes - bytes.size() - value.size() < 8)
        { valid = false; return; }
        u64(value.size()); bytes.append(value);
    }
};
struct Reader
{
    std::string_view bytes;
    std::size_t pos = 0;
    bool u8(std::uint8_t& value)
    { if (pos >= bytes.size()) return false; value = static_cast<std::uint8_t>(bytes[pos++]); return true; }
    bool boolean(bool& value)
    { std::uint8_t v = 0; if (!u8(v) || v > 1) return false; value = v != 0; return true; }
    bool u64(std::uint64_t& value)
    {
        value = 0;
        for (unsigned i = 0; i < 8; ++i) { std::uint8_t v = 0; if (!u8(v)) return false; value |= std::uint64_t(v) << (i * 8); }
        return true;
    }
    bool text(std::string& value)
    {
        std::uint64_t size = 0;
        if (!u64(size) || size > bytes.size() - pos) return false;
        value.assign(bytes.substr(pos, static_cast<std::size_t>(size))); pos += static_cast<std::size_t>(size); return true;
    }
};
inline bool writeType(Writer& w, const Type& type, unsigned depth)
{
    if (depth > 32 || static_cast<unsigned>(type.kind) > static_cast<unsigned>(TypeKind::Vector) || type.elements.size() > maxElements) return false;
    w.u8(static_cast<std::uint8_t>(type.kind)); w.u64(type.widthOrCount); w.u8(type.packed); w.u64(type.elements.size());
    for (const auto& child : type.elements) if (!writeType(w, child, depth + 1)) return false;
    return w.valid;
}
inline bool readType(Reader& r, Type& type, unsigned depth)
{
    std::uint8_t kind = 0; std::uint64_t size = 0;
    if (depth > 32 || !r.u8(kind) || kind > static_cast<std::uint8_t>(TypeKind::Vector) ||
        !r.u64(type.widthOrCount) || !r.boolean(type.packed) || !r.u64(size) || size > maxElements || size > r.bytes.size() - r.pos) return false;
    type.kind = static_cast<TypeKind>(kind); type.elements.resize(static_cast<std::size_t>(size));
    for (auto& child : type.elements) if (!readType(r, child, depth + 1)) return false;
    return true;
}
inline bool writeLogical(Writer& w, const LogicalValue& value)
{
    if (static_cast<unsigned>(value.passing) > static_cast<unsigned>(Passing::Ignore)) return false;
    w.u8(static_cast<std::uint8_t>(value.passing)); w.u8(value.coercion.has_value());
    if (value.coercion && !writeType(w, *value.coercion, 0)) return false;
    w.u64(value.offset); w.u64(value.alignment); w.u64(value.firstPhysical); w.u64(value.physicalCount);
    w.u8(value.flatten); w.u8(value.byValue); w.u8(value.signExtend); w.u8(value.zeroExtend); return w.valid;
}
inline bool readLogical(Reader& r, LogicalValue& value)
{
    std::uint8_t passing = 0; bool present = false;
    if (!r.u8(passing) || passing > static_cast<std::uint8_t>(Passing::Ignore) || !r.boolean(present)) return false;
    value.passing = static_cast<Passing>(passing);
    if (present) { value.coercion.emplace(); if (!readType(r, *value.coercion, 0)) return false; }
    return r.u64(value.offset) && r.u64(value.alignment) && r.u64(value.firstPhysical) && r.u64(value.physicalCount) &&
        r.boolean(value.flatten) && r.boolean(value.byValue) && r.boolean(value.signExtend) && r.boolean(value.zeroExtend);
}
inline bool writePhysical(Writer& w, const PhysicalParameter& value)
{
    if (!writeType(w, value.type, 0)) return false;
    w.u8(value.pointee.has_value()); if (value.pointee && !writeType(w, *value.pointee, 0)) return false;
    w.u64(value.alignment); w.u8(value.structureReturn); w.u8(value.byValue); w.u8(value.signExtend);
    w.u8(value.zeroExtend); w.u8(value.inRegister); w.u8(value.noAlias); return w.valid;
}
inline bool readPhysical(Reader& r, PhysicalParameter& value)
{
    bool present = false;
    if (!readType(r, value.type, 0) || !r.boolean(present)) return false;
    if (present) { value.pointee.emplace(); if (!readType(r, *value.pointee, 0)) return false; }
    return r.u64(value.alignment) && r.boolean(value.structureReturn) && r.boolean(value.byValue) &&
        r.boolean(value.signExtend) && r.boolean(value.zeroExtend) && r.boolean(value.inRegister) && r.boolean(value.noAlias);
}
}

inline std::optional<std::string> encodeVariadicCallPlanForCache(const VariadicCallPlan& plan)
{
    using namespace variadic_cache_detail;
    if (plan.arguments.size() > maxArguments || plan.physicalParameters.size() > maxPhysical) return std::nullopt;
    Writer w; w.text(plan.symbol); w.text(plan.targetTriple); w.text(plan.dataLayout);
    w.u64(plan.callingConvention); w.u64(plan.requiredArguments);
    if (!writeLogical(w, plan.result)) return std::nullopt;
    w.u64(plan.arguments.size());
    for (const auto& value : plan.arguments) if (!writeLogical(w, value)) return std::nullopt;
    if (!writeType(w, plan.physicalReturn, 0)) return std::nullopt;
    w.u8(plan.returnSignExtend); w.u8(plan.returnZeroExtend); w.u64(plan.physicalParameters.size());
    for (const auto& value : plan.physicalParameters) if (!writePhysical(w, value)) return std::nullopt;
    return w.valid ? std::optional<std::string>(std::move(w.bytes)) : std::nullopt;
}

inline std::optional<VariadicCallPlan> decodeVariadicCallPlanFromCache(std::string_view bytes)
{
    using namespace variadic_cache_detail;
    if (bytes.size() > maxBytes) return std::nullopt;
    Reader r{bytes}; VariadicCallPlan plan; std::uint64_t convention = 0, required = 0, count = 0;
    if (!r.text(plan.symbol) || !r.text(plan.targetTriple) || !r.text(plan.dataLayout) || !r.u64(convention) ||
        convention > std::numeric_limits<unsigned>::max() || !r.u64(required) || required > maxArguments ||
        !readLogical(r, plan.result) || !r.u64(count) || count > maxArguments || count > bytes.size() - r.pos) return std::nullopt;
    plan.callingConvention = static_cast<unsigned>(convention); plan.requiredArguments = static_cast<std::size_t>(required);
    plan.arguments.resize(static_cast<std::size_t>(count));
    for (auto& value : plan.arguments) if (!readLogical(r, value)) return std::nullopt;
    if (!readType(r, plan.physicalReturn, 0) || !r.boolean(plan.returnSignExtend) || !r.boolean(plan.returnZeroExtend) ||
        !r.u64(count) || count > maxPhysical || count > bytes.size() - r.pos) return std::nullopt;
    plan.physicalParameters.resize(static_cast<std::size_t>(count));
    for (auto& value : plan.physicalParameters) if (!readPhysical(r, value)) return std::nullopt;
    if (r.pos != bytes.size()) return std::nullopt;
    return plan;
}

// Reuses the bounded internal cache representation; this does not validate ABI semantics.
inline std::optional<std::string> encodeFunctionPointerPlanForCache(const Plan& plan)
{
    if (plan.variadic) return std::nullopt;
    VariadicCallPlan value;
    value.symbol = plan.symbol; value.targetTriple = plan.targetTriple; value.dataLayout = plan.dataLayout;
    value.callingConvention = plan.callingConvention; value.requiredArguments = plan.logicalParameters.size();
    value.result = plan.logicalReturn; value.arguments = plan.logicalParameters;
    value.physicalReturn = plan.physicalReturn; value.returnSignExtend = plan.returnSignExtend;
    value.returnZeroExtend = plan.returnZeroExtend; value.physicalParameters = plan.physicalParameters;
    return encodeVariadicCallPlanForCache(value);
}

inline std::optional<Plan> decodeFunctionPointerPlanFromCache(std::string_view bytes)
{
    auto value = decodeVariadicCallPlanFromCache(bytes);
    if (!value || value->requiredArguments != value->arguments.size()) return std::nullopt;
    Plan plan;
    plan.symbol = std::move(value->symbol); plan.targetTriple = std::move(value->targetTriple);
    plan.dataLayout = std::move(value->dataLayout); plan.callingConvention = value->callingConvention;
    plan.logicalReturn = std::move(value->result); plan.logicalParameters = std::move(value->arguments);
    plan.physicalReturn = std::move(value->physicalReturn); plan.returnSignExtend = value->returnSignExtend;
    plan.returnZeroExtend = value->returnZeroExtend; plan.physicalParameters = std::move(value->physicalParameters);
    return plan;
}
}
