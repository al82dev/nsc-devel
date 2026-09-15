#pragma once

#include <optional>
#include <string_view>

namespace nativesharp::llvm_backend
{
enum class FloatingPointMode
{
    Precise,
    Fast,
    Deterministic
};

enum class FmaContractPolicy
{
    Off,
    On,
    Fast
};

enum class FloatingPointRoundingMode
{
    Dynamic,
    NearestEven,
    TowardZero,
    Upward,
    Downward
};

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

class FloatingPointModeScope final
{
public:
    FloatingPointModeScope() noexcept;
    FloatingPointModeScope(const FloatingPointModeScope&) = delete;
    FloatingPointModeScope& operator=(const FloatingPointModeScope&) = delete;
    ~FloatingPointModeScope();

    void set(FloatingPointMode mode) noexcept;
    void setFmaContractPolicy(FmaContractPolicy policy) noexcept;
    void setRoundingMode(FloatingPointRoundingMode mode) noexcept;

private:
    FloatingPointMode previous_;
    FmaContractPolicy previousFmaContractPolicy_;
    FloatingPointRoundingMode previousRoundingMode_;
};
}
