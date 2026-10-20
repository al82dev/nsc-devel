#pragma once

#include <optional>
#include <string_view>

namespace nativesharp::llvm_backend
{
enum class FloatingPointMode
{
    Strict,
    Precise,
    Fast,
    Deterministic
};

}

#include "nativesharp/LLVM/FloatingPointDenormalMode.h"
#include "nativesharp/LLVM/FloatingPointModeScope.h"
#include "nativesharp/LLVM/FloatingPointRoundingMode.h"
#include "nativesharp/LLVM/FloatingPointState.h"
#include "nativesharp/LLVM/FmaContractPolicy.h"

namespace nativesharp::llvm_backend
{

std::optional<FloatingPointMode> parseFloatingPointMode(std::string_view value) noexcept;
std::string_view floatingPointModeName(FloatingPointMode mode) noexcept;
FloatingPointMode activeFloatingPointMode() noexcept;
std::optional<FmaContractPolicy> parseFmaContractPolicy(std::string_view value) noexcept;
std::string_view fmaContractPolicyName(FmaContractPolicy policy) noexcept;
FmaContractPolicy activeFmaContractPolicy() noexcept;
std::optional<FloatingPointRoundingMode> parseFloatingPointRoundingMode(std::string_view value) noexcept;
std::string_view floatingPointRoundingModeName(FloatingPointRoundingMode mode) noexcept;
std::string_view floatingPointRoundingMetadata(FloatingPointRoundingMode mode) noexcept;
FloatingPointRoundingMode activeFloatingPointRoundingMode() noexcept;
std::optional<FloatingPointDenormalMode> parseFloatingPointDenormalMode(std::string_view value) noexcept;
std::string_view floatingPointDenormalModeName(FloatingPointDenormalMode mode) noexcept;
FloatingPointDenormalMode activeFloatingPointDenormalMode() noexcept;

FloatingPointState captureFloatingPointState() noexcept;

// Installs a captured contract in a freshly-created long-lived worker thread. Unlike the scoped
// API below, this deliberately has no restore step: the worker owns the state until it exits.
void initializeFloatingPointStateForWorker(const FloatingPointState& state) noexcept;

}
