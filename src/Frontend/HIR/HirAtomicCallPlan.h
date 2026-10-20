#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <cstddef>

namespace nativesharp::frontend
{
struct HirAtomicCallPlan final
{
    std::size_t valueArgumentCount;
    ProjectOopHirNodeKind hirKind;
};
}
