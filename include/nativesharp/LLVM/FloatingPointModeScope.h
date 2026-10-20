#pragma once

#include "nativesharp/LLVM/FloatingPointState.h"

namespace nativesharp::llvm_backend
{
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
    void setDenormalMode(FloatingPointDenormalMode mode) noexcept;
    void setState(const FloatingPointState& state) noexcept;

private:
    FloatingPointState previous_;
};
}
