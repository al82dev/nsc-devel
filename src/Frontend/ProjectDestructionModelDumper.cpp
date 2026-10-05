#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"

#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectDestructionModelDumper::dump(
    const ProjectDestructionModel& model,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::ostream& output)
{
    output << "NativeSharp Stage 35 destruction model\n";
    for (const ClassDestructionPlan& plan : model.plans())
    {
        output << "class[" << plan.typeOrdinal << "] ";
        if (plan.typeOrdinal < nameLookup.namedTypes().size())
        {
            output << nameLookup.namedTypes()[plan.typeOrdinal].identity.key();
        }
        output << " entry=" << plan.destructionEntryName
               << " object-entry=" << plan.objectDestructionEntryName
               << " xmod-abi=" << plan.crossModuleLifetimeAbiMajor
               << " xmod-create=" << plan.crossModuleCreateEntryName
               << " xmod-destroy=" << plan.crossModuleDestroyEntryName
               << " xmod-deallocate=" << plan.crossModuleDeallocateEntryName
               << " explicit=";
        if (plan.explicitDestructorMemberOrdinal.has_value())
        {
            output << *plan.explicitDestructorMemberOrdinal;
        }
        else
        {
            output << "none";
        }
        output << " chain=";
        if (plan.destructorMemberOrdinals.empty())
        {
            output << "<free-only>";
        }
        else
        {
            for (std::size_t index = 0; index < plan.destructorMemberOrdinals.size(); ++index)
            {
                if (index != 0)
                {
                    output << " -> ";
                }
                const std::size_t memberOrdinal = plan.destructorMemberOrdinals[index];
                output << '#' << memberOrdinal;
                if (memberOrdinal < memberModel.members().size())
                {
                    output << ':' << memberModel.members()[memberOrdinal].signatureKey;
                }
            }
        }
        output << '\n';
    }
}
}
