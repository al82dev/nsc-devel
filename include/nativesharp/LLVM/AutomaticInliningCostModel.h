#pragma once

namespace nativesharp::llvm_backend
{
enum class AutomaticInliningCostModel
{
    Disabled,
    Conservative,
    Balanced,
    Aggressive
};
}
