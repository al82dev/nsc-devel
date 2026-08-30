#pragma once

#include <iosfwd>

namespace nativesharp::compilation
{
class ProjectGraph;

class ProjectGraphDumper final
{
public:
    void dump(const ProjectGraph& graph, std::ostream& output) const;
};
}
