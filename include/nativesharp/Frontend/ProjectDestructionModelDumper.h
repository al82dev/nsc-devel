#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectDestructionModel;
class ProjectNameLookup;
class ProjectMemberModel;

class ProjectDestructionModelDumper final
{
public:
    static void dump(
        const ProjectDestructionModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::ostream& output);
};
}
