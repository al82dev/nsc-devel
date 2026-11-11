#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace clang { class CodeGenerator; class FunctionDecl; }

namespace nativesharp::interop::abi_plan
{
// The returned plan owns its data and does not reference the generator or AST.
std::optional<Plan> copyPlan(clang::CodeGenerator& generator,
                             const clang::FunctionDecl& function,
                             std::string& error);
}
