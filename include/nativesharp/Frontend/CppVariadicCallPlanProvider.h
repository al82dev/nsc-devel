#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>

namespace nativesharp::interop::abi_plan { struct VariadicCallPlan; }
namespace nativesharp::frontend
{
struct NamespaceFunctionSymbol;
// The optional type keys are in ABI parameter order after default promotions.
using CppVariadicCallPlanProvider = std::function<std::shared_ptr<const interop::abi_plan::VariadicCallPlan>(
    const NamespaceFunctionSymbol&, std::span<const std::string>)>;
}
