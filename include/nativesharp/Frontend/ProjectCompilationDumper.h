#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectCompilation;

class ProjectCompilationDumper final
{
public:
    void dump(const ProjectCompilation& compilation, std::ostream& output) const;
    void dumpComptime(const ProjectCompilation& compilation, std::ostream& output) const;
};
}
