#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"
#include <clang/AST/Type.h>
#include <span>

namespace clang { class CodeGenerator; class FunctionDecl; }

namespace nativesharp::interop::abi_plan
{
// The returned plan owns its data and does not reference the generator or AST.
std::optional<Plan> copyPlan(clang::CodeGenerator& generator,
                             const clang::FunctionDecl& function,
                             std::string& error);

// Classification of one actual variadic call; this is not a physical LLVM
// signature and does not, by itself, authorize backend emission.
struct VariadicCallPlan
{
    std::string symbol;
    std::string targetTriple;
    std::string dataLayout;
    unsigned callingConvention = 0;
    std::size_t requiredArguments = 0;
    LogicalValue result;
    std::vector<LogicalValue> arguments;
};
std::optional<VariadicCallPlan> copyVariadicCallPlan(
    clang::CodeGenerator& generator, const clang::FunctionDecl& function,
    std::span<const clang::QualType> argumentTypes, std::string& error);
}
