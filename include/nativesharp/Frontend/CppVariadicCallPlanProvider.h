#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::interop::abi_plan { struct VariadicCallPlan; struct Plan; }
namespace nativesharp::frontend
{
struct NamespaceFunctionSymbol;
struct CppImportedTypeAlias;
// The optional type keys are in ABI parameter order after default promotions.
using CppVariadicCallPlanProvider = std::function<std::shared_ptr<const interop::abi_plan::VariadicCallPlan>(
    const NamespaceFunctionSymbol&, std::span<const std::string>)>;
using CppFunctionPointerPlanProvider = std::function<std::shared_ptr<const interop::abi_plan::Plan>(
    const CppImportedTypeAlias&, std::string_view)>;
}
