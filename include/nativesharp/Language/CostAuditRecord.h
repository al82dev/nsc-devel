#pragma once

#include <cstddef>
#include <string>

#include "nativesharp/Language/CostEvent.h"
#include "nativesharp/Language/CostOptimization.h"

namespace nativesharp::language
{
struct CostAuditRecord final
{
    CostEvent event;
    CostOptimization optimization = CostOptimization::Off;
    bool libraryFree = true;
    std::size_t mirOccurrences = 0;
    std::size_t assemblyOccurrences = 0;
    std::string approvedException;
};
}
