#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace nativesharp::sema
{
enum class Effect : std::uint32_t
{
    Unsafe = 1U << 0U,
    Throws = 1U << 1U,
    Async = 1U << 2U,
    Comptime = 1U << 3U,
    NoHeap = 1U << 4U
};

class EffectSet final
{
public:
    constexpr EffectSet() noexcept = default;
    constexpr explicit EffectSet(Effect effect) noexcept
        : bits_(static_cast<std::uint32_t>(effect)) {}

    constexpr EffectSet with(Effect effect) const noexcept
    {
        EffectSet result = *this;
        result.bits_ |= static_cast<std::uint32_t>(effect);
        return result;
    }

    constexpr bool contains(Effect effect) const noexcept
    {
        return (bits_ & static_cast<std::uint32_t>(effect)) != 0;
    }

    constexpr std::uint32_t bits() const noexcept { return bits_; }

private:
    std::uint32_t bits_ = 0;
};

constexpr std::string_view effectName(Effect effect) noexcept
{
    switch (effect)
    {
        case Effect::Unsafe: return "unsafe";
        case Effect::Throws: return "throws";
        case Effect::Async: return "async";
        case Effect::Comptime: return "comptime";
        case Effect::NoHeap: return "noheap";
    }
    return "unknown";
}

constexpr bool effectParticipatesInFunctionType(Effect effect) noexcept
{
    return effect == Effect::Throws || effect == Effect::Async || effect == Effect::NoHeap;
}

class EffectContext final
{
public:
    explicit constexpr EffectContext(
        EffectSet local = {},
        const EffectContext* parent = nullptr) noexcept
        : local_(local), parent_(parent) {}

    constexpr bool allows(Effect effect) const noexcept
    {
        return local_.contains(effect) || (parent_ != nullptr && parent_->allows(effect));
    }

    constexpr std::optional<Effect> require(Effect effect) const noexcept
    {
        return allows(effect) ? std::nullopt : std::optional<Effect>(effect);
    }

    constexpr EffectSet localEffects() const noexcept { return local_; }

private:
    EffectSet local_;
    const EffectContext* parent_ = nullptr;
};
}
