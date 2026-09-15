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

std::optional<FloatingPointMode> parseFloatingPointMode(std::string_view value) noexcept;
std::string_view floatingPointModeName(FloatingPointMode mode) noexcept;
FloatingPointMode activeFloatingPointMode() noexcept;
std::optional<FmaContractPolicy> parseFmaContractPolicy(std::string_view value) noexcept;
std::string_view fmaContractPolicyName(FmaContractPolicy policy) noexcept;
FmaContractPolicy activeFmaContractPolicy() noexcept;

class FloatingPointModeScope final
{
public:
    FloatingPointModeScope() noexcept;
    FloatingPointModeScope(const FloatingPointModeScope&) = delete;
    FloatingPointModeScope& operator=(const FloatingPointModeScope&) = delete;
    ~FloatingPointModeScope();

    void set(FloatingPointMode mode) noexcept;
    void setFmaContractPolicy(FmaContractPolicy policy) noexcept;

private:
    FloatingPointMode previous_;
    FmaContractPolicy previousFmaContractPolicy_;
};
}
