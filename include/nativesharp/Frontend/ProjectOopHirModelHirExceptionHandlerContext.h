#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
struct ProjectOopHirModel::HirExceptionHandlerContext final
{
    std::size_t hirRegionOrdinal = 0;
    std::size_t retainedCleanupDepth = 0;
};
}
