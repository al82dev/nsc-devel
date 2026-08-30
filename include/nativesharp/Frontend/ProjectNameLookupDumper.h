#pragma once

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectNameLookup;
class NamespaceSymbol;

class ProjectNameLookupDumper final
{
public:
    void dump(const ProjectNameLookup& lookup, std::ostream& output) const;

private:
    static void dumpNamespace(
        const ProjectNameLookup& lookup,
        const NamespaceSymbol& namespaceSymbol,
        std::ostream& output,
        int depth);
};
}
