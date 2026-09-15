#include "nativesharp/LLVM/FloatingPointMode.h"

namespace nativesharp::llvm_backend
{
namespace
{
thread_local FloatingPointState gActiveState;
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
    return gActiveState.mode;
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
    return gActiveState.fmaContractPolicy;
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
    return gActiveState.roundingMode;
}

std::optional<FloatingPointDenormalMode> parseFloatingPointDenormalMode(std::string_view value) noexcept
{
    if (value == "preserve") return FloatingPointDenormalMode::Preserve;
    if (value == "ftz") return FloatingPointDenormalMode::FlushToZero;
    if (value == "daz") return FloatingPointDenormalMode::DenormalsAreZero;
    if (value == "ftz-daz") return FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero;
    return std::nullopt;
}

std::string_view floatingPointDenormalModeName(FloatingPointDenormalMode mode) noexcept
{
    switch (mode)
    {
        case FloatingPointDenormalMode::Preserve: return "preserve";
        case FloatingPointDenormalMode::FlushToZero: return "ftz";
        case FloatingPointDenormalMode::DenormalsAreZero: return "daz";
        case FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero: return "ftz-daz";
    }
    return {};
}

FloatingPointDenormalMode activeFloatingPointDenormalMode() noexcept
{
    return gActiveState.denormalMode;
}

FloatingPointState captureFloatingPointState() noexcept
{
    return gActiveState;
}

void initializeFloatingPointStateForWorker(const FloatingPointState& state) noexcept
{
    gActiveState = state;
}

FloatingPointModeScope::FloatingPointModeScope() noexcept
    : previous_(captureFloatingPointState())
{
}

FloatingPointModeScope::~FloatingPointModeScope()
{
    setState(previous_);
}

void FloatingPointModeScope::setRoundingMode(FloatingPointRoundingMode mode) noexcept
{
    gActiveState.roundingMode = mode;
}

void FloatingPointModeScope::setDenormalMode(FloatingPointDenormalMode mode) noexcept
{
    gActiveState.denormalMode = mode;
}

void FloatingPointModeScope::setFmaContractPolicy(FmaContractPolicy policy) noexcept
{
    gActiveState.fmaContractPolicy = policy;
}

void FloatingPointModeScope::set(FloatingPointMode mode) noexcept
{
    gActiveState.mode = mode;
}

void FloatingPointModeScope::setState(const FloatingPointState& state) noexcept
{
    gActiveState = state;
}
}
