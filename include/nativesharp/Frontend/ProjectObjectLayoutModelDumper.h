#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectMemberModel;
class ProjectNameLookup;
class ProjectObjectLayoutModel;

class ProjectObjectLayoutModelDumper final
{
public:
    void dump(
        const ProjectObjectLayoutModel& model,
        const ProjectMemberModel& members,
        const ProjectNameLookup& names,
        std::ostream& output) const;
};
}
