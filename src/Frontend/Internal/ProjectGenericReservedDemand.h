#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

#include <limits>

namespace nativesharp::frontend
{
struct ProjectGenericModel::ReservedDemand final
{
    GenericTypeDemand* demand = nullptr;
    std::size_t ordinal = std::numeric_limits<std::size_t>::max();
    std::string key;
    std::string specializedName;
    SpecializationContext callerContext;
};
}
