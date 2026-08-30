#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectConstructionModel;
class ProjectMemberModel;
class ProjectNameLookup;

class ProjectConstructionModelDumper final
{
public:
    void dump(
        const ProjectConstructionModel& model,
        const ProjectMemberModel& members,
        const ProjectNameLookup& names,
        std::ostream& output) const;
};
}
