#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"

#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectConstructionModelDumper::dump(
    const ProjectConstructionModel& model,
    const ProjectMemberModel& members,
    const ProjectNameLookup& names,
    std::ostream& output) const
{
    output << "project-constructions\n";
    for (std::size_t ordinal = 0; ordinal < model.constructorInitializerBindings().size(); ++ordinal)
    {
        const ConstructorInitializerBinding& binding = model.constructorInitializerBindings()[ordinal];
        output << "  initializer[" << ordinal << "] constructor-member=" << binding.constructorMemberOrdinal
               << " kind=" << constructorInitializerKindName(binding.kind)
               << " resolution=" << constructorInitializerResolutionKindName(binding.resolution);
        if (binding.targetTypeOrdinal.has_value())
        {
            output << " target-type=" << *binding.targetTypeOrdinal;
        }
        output << '\n';
        if (binding.selectedImplicitDefault)
        {
            output << "    selected=implicit-default\n";
        }
        else if (binding.selectedConstructorMemberOrdinal.has_value() &&
                 *binding.selectedConstructorMemberOrdinal < members.members().size())
        {
            output << "    selected="
                   << members.members()[*binding.selectedConstructorMemberOrdinal].signatureKey << '\n';
        }
        if (binding.selectedEntryConstructorMemberOrdinal.has_value() &&
            *binding.selectedEntryConstructorMemberOrdinal < members.members().size())
        {
            output << "    entry="
                   << members.members()[*binding.selectedEntryConstructorMemberOrdinal].signatureKey << '\n';
        }
    }
    for (std::size_t ordinal = 0; ordinal < model.constructions().size(); ++ordinal)
    {
        const ObjectConstructionBinding& binding = model.constructions()[ordinal];
        output << "  [" << ordinal << "] target=" << binding.targetTypeKey
               << " resolution=" << constructionResolutionKindName(binding.resolution)
               << " caller-member=" << binding.callerMemberOrdinal << '\n';
        for (const ConstructorCandidateBinding& candidate : binding.evaluatedCandidates)
        {
            output << "    candidate=";
            if (candidate.implicitDefault)
            {
                output << "implicit-default";
            }
            else if (candidate.memberOrdinal.has_value() && *candidate.memberOrdinal < members.members().size())
            {
                output << members.members()[*candidate.memberOrdinal].signatureKey;
            }
            else
            {
                output << "<invalid>";
            }
            output << " accessible=" << (candidate.accessible ? "true" : "false")
                   << " viable=" << (candidate.viable ? "true" : "false");
            for (const ProjectImplicitConversion& conversion : candidate.conversions)
            {
                output << " conversion=" << projectConversionKindName(conversion.kind)
                       << '(' << conversion.sourceTypeKey << "->" << conversion.targetTypeKey << ')';
            }
            output << '\n';
        }
        if (binding.selectedImplicitDefault)
        {
            output << "    selected=implicit-default\n";
        }
        else if (binding.selectedConstructorMemberOrdinal.has_value() &&
                 *binding.selectedConstructorMemberOrdinal < members.members().size())
        {
            output << "    selected="
                   << members.members()[*binding.selectedConstructorMemberOrdinal].signatureKey << '\n';
        }
        if (binding.targetTypeOrdinal < names.namedTypes().size())
        {
            output << "    class=" << names.namedTypes()[binding.targetTypeOrdinal].identity.key() << '\n';
        }
    }
}
}
