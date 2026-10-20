#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <cstddef>

namespace nativesharp::frontend
{
struct HirCoreOperandCallPlan final
{
    std::size_t operandCount;
    ProjectOopHirNodeKind hirKind;
};
}
