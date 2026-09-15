#include "nativesharp/LLVM/FloatingPointMode.h"

#include <atomic>

namespace nativesharp::llvm_backend
{
namespace
{
std::atomic<FloatingPointMode> gActiveMode{FloatingPointMode::Precise};
std::atomic<FmaContractPolicy> gActiveFmaContractPolicy{FmaContractPolicy::Off};
std::atomic<FloatingPointRoundingMode> gActiveRoundingMode{FloatingPointRoundingMode::Dynamic};
}

std::optional<FloatingPointMode> parseFloatingPointMode(std::string_view value) noexcept
{
    if (value == "precise") return FloatingPointMode::Precise;
    if (value == "fast") return FloatingPointMode::Fast;
    if (value == "deterministic") return FloatingPointMode::Deterministic;
    return std::nullopt;
}

std::string_view floatingPointModeName(FloatingPointMode mode) noexcept
{
    switch (mode)
    {
        case FloatingPointMode::Precise: return "precise";
        case FloatingPointMode::Fast: return "fast";
        case FloatingPointMode::Deterministic: return "deterministic";
    }
    return {};
}

FloatingPointMode activeFloatingPointMode() noexcept
{
    return gActiveMode.load(std::memory_order_relaxed);
}

std::optional<FmaContractPolicy> parseFmaContractPolicy(std::string_view value) noexcept
{
    if (value == "off") return FmaContractPolicy::Off;
    if (value == "on") return FmaContractPolicy::On;
    if (value == "fast") return FmaContractPolicy::Fast;
    return std::nullopt;
}

std::string_view fmaContractPolicyName(FmaContractPolicy policy) noexcept
{
    switch (policy)
    {
        case FmaContractPolicy::Off: return "off";
        case FmaContractPolicy::On: return "on";
        case FmaContractPolicy::Fast: return "fast";
    }
    return {};
}

FmaContractPolicy activeFmaContractPolicy() noexcept
{
    return gActiveFmaContractPolicy.load(std::memory_order_relaxed);
}

std::optional<FloatingPointRoundingMode> parseFloatingPointRoundingMode(std::string_view value) noexcept
{
    if (value == "dynamic") return FloatingPointRoundingMode::Dynamic;
    if (value == "nearest-even") return FloatingPointRoundingMode::NearestEven;
    if (value == "toward-zero") return FloatingPointRoundingMode::TowardZero;
    if (value == "upward") return FloatingPointRoundingMode::Upward;
    if (value == "downward") return FloatingPointRoundingMode::Downward;
    return std::nullopt;
}

std::string_view floatingPointRoundingModeName(FloatingPointRoundingMode mode) noexcept
{
    switch (mode)
    {
        case FloatingPointRoundingMode::Dynamic: return "dynamic";
        case FloatingPointRoundingMode::NearestEven: return "nearest-even";
        case FloatingPointRoundingMode::TowardZero: return "toward-zero";
        case FloatingPointRoundingMode::Upward: return "upward";
        case FloatingPointRoundingMode::Downward: return "downward";
    }
    return {};
}

std::string_view floatingPointRoundingMetadata(FloatingPointRoundingMode mode) noexcept
{
    switch (mode)
    {
        case FloatingPointRoundingMode::Dynamic: return "round.dynamic";
        case FloatingPointRoundingMode::NearestEven: return "round.tonearest";
        case FloatingPointRoundingMode::TowardZero: return "round.towardzero";
        case FloatingPointRoundingMode::Upward: return "round.upward";
        case FloatingPointRoundingMode::Downward: return "round.downward";
    }
    return {};
}

FloatingPointRoundingMode activeFloatingPointRoundingMode() noexcept
{
    return gActiveRoundingMode.load(std::memory_order_relaxed);
}

FloatingPointModeScope::FloatingPointModeScope() noexcept
    : previous_(activeFloatingPointMode()),
      previousFmaContractPolicy_(activeFmaContractPolicy()),
      previousRoundingMode_(activeFloatingPointRoundingMode())
{
}

FloatingPointModeScope::~FloatingPointModeScope()
{
    set(previous_);
    setFmaContractPolicy(previousFmaContractPolicy_);
    setRoundingMode(previousRoundingMode_);
}

void FloatingPointModeScope::setRoundingMode(FloatingPointRoundingMode mode) noexcept
{
    gActiveRoundingMode.store(mode, std::memory_order_relaxed);
}

void FloatingPointModeScope::setFmaContractPolicy(FmaContractPolicy policy) noexcept
{
    gActiveFmaContractPolicy.store(policy, std::memory_order_relaxed);
}

void FloatingPointModeScope::set(FloatingPointMode mode) noexcept
{
    gActiveMode.store(mode, std::memory_order_relaxed);
}
}
