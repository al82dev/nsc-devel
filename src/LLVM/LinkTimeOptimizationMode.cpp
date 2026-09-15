#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"

namespace nativesharp::llvm_backend
{
std::string_view linkTimeOptimizationModeName(LinkTimeOptimizationMode mode) noexcept
{
    switch (mode)
    {
    case LinkTimeOptimizationMode::None: return "none";
    case LinkTimeOptimizationMode::Full: return "full";
    case LinkTimeOptimizationMode::Thin: return "thin";
    }
    return {};
}
}
