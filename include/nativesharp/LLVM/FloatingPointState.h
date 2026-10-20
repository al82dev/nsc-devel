#pragma once

#include "nativesharp/LLVM/FloatingPointDenormalMode.h"
#include "nativesharp/LLVM/FloatingPointRoundingMode.h"
#include "nativesharp/LLVM/FmaContractPolicy.h"

namespace nativesharp::llvm_backend
{
struct FloatingPointState final
{
    FloatingPointMode mode = FloatingPointMode::Precise;
    FmaContractPolicy fmaContractPolicy = FmaContractPolicy::Off;
    FloatingPointRoundingMode roundingMode = FloatingPointRoundingMode::Dynamic;
    FloatingPointDenormalMode denormalMode = FloatingPointDenormalMode::Preserve;

    bool operator==(const FloatingPointState&) const = default;
};
}
