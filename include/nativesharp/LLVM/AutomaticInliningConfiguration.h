#pragma once

#include "nativesharp/LLVM/AutomaticInliningCostModel.h"

#include <string_view>

namespace nativesharp::llvm_backend
{
struct AutomaticInliningConfiguration final
{
    AutomaticInliningCostModel costModel;
    std::string_view pipeline;

    constexpr bool enabled() const noexcept
    {
        return costModel != AutomaticInliningCostModel::Disabled;
    }
};
}
