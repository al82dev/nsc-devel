#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace nativesharp::llvm_backend
{
class LlvmOptimizationBudgetTracker final
{
public:
    explicit LlvmOptimizationBudgetTracker(
        std::uint64_t budgetMilliseconds = kDefaultLlvmO2BudgetMilliseconds) noexcept;

    void setBudgetMilliseconds(std::uint64_t value) noexcept;
    void observe(LlvmOptimizationLevel level, std::chrono::nanoseconds elapsed) noexcept;

    std::uint64_t budgetMilliseconds() const noexcept;
    std::chrono::nanoseconds elapsed() const noexcept;
    std::size_t invocations() const noexcept;
    bool exceeded() const noexcept;
    std::string report() const;

private:
    std::uint64_t budgetMilliseconds_;
    std::chrono::nanoseconds elapsed_{};
    std::size_t invocations_ = 0;
};
}
