#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectInheritanceModel;
class ProjectNameLookup;

class ProjectInheritanceModelDumper final
{
public:
    void dump(
        const ProjectInheritanceModel& model,
        const ProjectNameLookup& names,
        std::ostream& output) const;
};
}
