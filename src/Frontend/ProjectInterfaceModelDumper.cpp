#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectInterfaceModelDumper::dump(const ProjectInterfaceModel& model, std::ostream& output)
{
    output << "project-interfaces\n";
    for (const InterfaceTypeInfo& info : model.interfaces())
    {
        output << "  interface " << model.typeKeyForOrdinal(info.typeOrdinal) << '\n';
        output << "    bases:";
        for (const std::size_t base : info.directBaseInterfaceOrdinals)
        {
            output << ' ' << model.typeKeyForOrdinal(base);
        }
        output << '\n';
        output << "    effective-methods:";
        for (const std::size_t member : info.effectiveMethodMemberOrdinals)
        {
            output << ' ' << member;
        }
        output << '\n';
    }
    for (const ClassInterfaceTablePlan& plan : model.tablePlans())
    {
        output << "  table class=" << model.typeKeyForOrdinal(plan.classTypeOrdinal)
               << " interface=" << model.typeKeyForOrdinal(plan.interfaceTypeOrdinal)
               << " type-id=" << plan.stableInterfaceTypeId << '\n';
        for (std::size_t slot = 0; slot < plan.interfaceMethodMemberOrdinals.size(); ++slot)
        {
            output << "    slot[" << slot << "] contract="
                   << plan.interfaceMethodMemberOrdinals[slot] << " implementation=";
            if (slot < plan.implementationMemberOrdinals.size() &&
                plan.implementationMemberOrdinals[slot].has_value())
            {
                output << *plan.implementationMemberOrdinals[slot];
            }
            else
            {
                output << "<abstract>";
            }
            output << '\n';
        }
    }
}
}
