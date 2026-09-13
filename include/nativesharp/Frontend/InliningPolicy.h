#pragma once

#include <cstdint>
#include <string_view>

namespace nativesharp::frontend
{
// Authoritative semantic policy carried by a callable from source binding through code generation.
// The spelling is deliberately not parsed here: items 280-282 add the source modifiers, while
// item 278 gives every later phase one closed vocabulary and prevents subtly different meanings.
enum class InliningPolicy : std::uint8_t
{
    Automatic,
    Preferred,
    Required,
    Forbidden
};

constexpr bool isValidInliningPolicy(InliningPolicy policy) noexcept
{
    switch (policy)
    {
    case InliningPolicy::Automatic:
    case InliningPolicy::Preferred:
    case InliningPolicy::Required:
    case InliningPolicy::Forbidden:
        return true;
    }
    return false;
}

constexpr std::string_view inliningPolicyName(InliningPolicy policy) noexcept
{
    switch (policy)
    {
    case InliningPolicy::Automatic: return "automatic";
    case InliningPolicy::Preferred: return "inline";
    case InliningPolicy::Required: return "forceinline";
    case InliningPolicy::Forbidden: return "noinline";
    }
    return "invalid";
}

constexpr bool permitsInlining(InliningPolicy policy) noexcept
{
    return policy != InliningPolicy::Forbidden;
}

constexpr bool requestsInlining(InliningPolicy policy) noexcept
{
    return policy == InliningPolicy::Preferred || policy == InliningPolicy::Required;
}

constexpr bool requiresInlining(InliningPolicy policy) noexcept
{
    return policy == InliningPolicy::Required;
}

constexpr bool requiresOutOfLineCall(InliningPolicy policy) noexcept
{
    return policy == InliningPolicy::Forbidden;
}
}
