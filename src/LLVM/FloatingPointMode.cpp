#include "nativesharp/LLVM/FloatingPointMode.h"

#include <atomic>

namespace nativesharp::llvm_backend
{
namespace
{
std::atomic<FloatingPointMode> gActiveMode{FloatingPointMode::Precise};
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

FloatingPointModeScope::FloatingPointModeScope() noexcept
    : previous_(activeFloatingPointMode())
{
}

FloatingPointModeScope::~FloatingPointModeScope()
{
    set(previous_);
}

void FloatingPointModeScope::set(FloatingPointMode mode) noexcept
{
    gActiveMode.store(mode, std::memory_order_relaxed);
}
}
