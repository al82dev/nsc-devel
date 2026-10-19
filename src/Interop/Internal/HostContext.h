#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace nativesharp::interop
{
namespace
{
using frontend::ProjectMemberModel;
using frontend::ProjectNameLookup;
using frontend::ProjectObjectLayoutModel;

struct HostContext final
{
    const ProjectNameLookup& names;
    const ProjectMemberModel& members;
    const ProjectObjectLayoutModel& layouts;
    const frontend::ProjectNamespaceFunctionModel& namespaceFunctions;
    std::size_t rootProjectOrdinal = 0;
    std::unordered_map<std::string, std::string> cNames;
};
}
}
