#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectMemberModel;
class ProjectNameLookup;
class ProjectVirtualDispatchModel;

class ProjectVirtualDispatchModelDumper final
{
public:
    void dump(
        const ProjectVirtualDispatchModel& model,
        const ProjectMemberModel& members,
        const ProjectNameLookup& names,
        std::ostream& output) const;
};
}
