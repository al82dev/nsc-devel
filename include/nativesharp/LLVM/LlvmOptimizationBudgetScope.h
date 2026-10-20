#pragma once

namespace nativesharp::llvm_backend
{
class LlvmOptimizationBudgetTracker;

// Installs a tracker only for the current compiler thread and restores the previous tracker on
// scope exit. Future worker threads must install their own scope explicitly.
class LlvmOptimizationBudgetScope final
{
public:
    explicit LlvmOptimizationBudgetScope(LlvmOptimizationBudgetTracker& tracker) noexcept;
    ~LlvmOptimizationBudgetScope();

    LlvmOptimizationBudgetScope(const LlvmOptimizationBudgetScope&) = delete;
    LlvmOptimizationBudgetScope& operator=(const LlvmOptimizationBudgetScope&) = delete;

private:
    LlvmOptimizationBudgetTracker* previous_;
};
}
