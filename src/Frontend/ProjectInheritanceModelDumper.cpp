#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectInheritanceModelDumper::dump(
    const ProjectInheritanceModel& model,
    const ProjectNameLookup& names,
    std::ostream& output) const
{
    output << "project-inheritance\n";
    for (const ClassInheritanceInfo& info : model.classes())
    {
        if (info.typeOrdinal >= names.namedTypes().size())
        {
            continue;
        }
        output << "  class=" << names.namedTypes()[info.typeOrdinal].identity.key()
               << " depth=" << info.depth
               << " base=";
        if (info.baseTypeOrdinal.has_value() && *info.baseTypeOrdinal < names.namedTypes().size())
        {
            output << names.namedTypes()[*info.baseTypeOrdinal].identity.key();
        }
        else
        {
            output << "<none>";
        }
        output << " derived=" << info.directlyDerivedTypeOrdinals.size() << '\n';
        for (const std::size_t derived : info.directlyDerivedTypeOrdinals)
        {
            if (derived < names.namedTypes().size())
            {
                output << "    child=" << names.namedTypes()[derived].identity.key() << '\n';
            }
        }
    }
}
}
