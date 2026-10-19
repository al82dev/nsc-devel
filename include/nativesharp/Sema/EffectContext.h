#pragma once

#include "nativesharp/Sema/Effect.h"
#include "nativesharp/Sema/EffectSet.h"

#include <optional>
#include <string_view>

namespace nativesharp::sema
{
constexpr std::string_view effectName(Effect effect) noexcept
{
    switch (effect)
    {
        case Effect::Unsafe: return "unsafe";
        case Effect::Throws: return "throws";
        case Effect::Async: return "async";
        case Effect::Comptime: return "comptime";
        case Effect::NoHeap: return "noheap";
        case Effect::Generator: return "generator";
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
