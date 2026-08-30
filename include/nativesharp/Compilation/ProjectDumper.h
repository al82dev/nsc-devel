#pragma once

#include <iosfwd>

namespace nativesharp::compilation
{
class Project;

class ProjectDumper final
{
public:
    void dump(const Project& project, std::ostream& output) const;
};
}
