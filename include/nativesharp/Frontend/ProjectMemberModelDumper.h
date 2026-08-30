#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectMemberModel;
class ProjectNameLookup;

class ProjectMemberModelDumper final
{
public:
    void dump(
        const ProjectMemberModel& model,
        const ProjectNameLookup& nameLookup,
        std::ostream& output) const;
};
}
