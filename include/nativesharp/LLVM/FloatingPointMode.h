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

std::optional<FloatingPointMode> parseFloatingPointMode(std::string_view value) noexcept;
std::string_view floatingPointModeName(FloatingPointMode mode) noexcept;
FloatingPointMode activeFloatingPointMode() noexcept;

class FloatingPointModeScope final
{
public:
    FloatingPointModeScope() noexcept;
    FloatingPointModeScope(const FloatingPointModeScope&) = delete;
    FloatingPointModeScope& operator=(const FloatingPointModeScope&) = delete;
    ~FloatingPointModeScope();

    void set(FloatingPointMode mode) noexcept;

private:
    FloatingPointMode previous_;
};
}
