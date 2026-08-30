#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectMethodBodyModelDumper final
{
public:
    void dump(
        const ProjectMethodBodyModel& model,
        const ProjectMemberModel& memberModel,
        const ProjectNameLookup& nameLookup,
        std::ostream& output) const;
};
}
