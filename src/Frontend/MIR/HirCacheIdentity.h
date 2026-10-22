#pragma once

#include <string>

namespace nativesharp::frontend
{
struct ProjectOopHirFunction;

std::string serializeHirCacheIdentity(const ProjectOopHirFunction& function);
} // namespace nativesharp::frontend
