#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectObjectLayoutModelDumper::dump(
    const ProjectObjectLayoutModel& model,
    const ProjectMemberModel& members,
    const ProjectNameLookup& names,
    std::ostream& output) const
{
    output << "project-object-layouts\n";
    const target::TypeLayout reference = model.classReferenceLayout();
    output << "  class-reference size=" << reference.size
           << " align=" << reference.alignment << " representation=pointer\n";

    output << "  struct-values=" << model.structLayouts().size() << '\n';
    for (const StructValueLayout& layout : model.structLayouts())
    {
        output << "  struct " << layout.typeKey
               << " size=" << layout.size
               << " align=" << layout.alignment
               << " representation=inline-value\n";
        for (const ObjectFieldLayout& field : layout.fields)
        {
            output << "    field ordinal=" << field.memberOrdinal;
            if (field.memberOrdinal < members.members().size())
            {
                output << " name=" << members.members()[field.memberOrdinal].name;
            }
            output << " type=" << field.typeKey
                   << " offset=" << field.offset
                   << " size=" << field.size
                   << " align=" << field.alignment << '\n';
        }
    }

    for (const ClassObjectLayout& layout : model.classLayouts())
    {
        output << "  class " << layout.typeKey
               << " size=" << layout.size
               << " align=" << layout.alignment
               << " header=" << layout.headerSize
               << " reference-size=" << layout.referenceSize << '\n';
        for (const ObjectFieldLayout& field : layout.fields)
        {
            output << "    field ordinal=" << field.memberOrdinal;
            if (field.memberOrdinal < members.members().size())
            {
                output << " name=" << members.members()[field.memberOrdinal].name;
            }
            output << " type=" << field.typeKey
                   << " offset=" << field.offset
                   << " size=" << field.size
                   << " align=" << field.alignment << '\n';
        }
    }

    output << "  allocation-plans\n";
    for (const ObjectAllocationPlan& plan : model.allocationPlans())
    {
        output << "    construction=" << plan.constructionOrdinal
               << " target=" << plan.targetTypeKey
               << " object-size=" << plan.objectSize
               << " object-align=" << plan.objectAlignment
               << " failure=null"
               << " zero-init=" << (plan.zeroInitializesStorage ? "true" : "false") << '\n';
        for (const ObjectInitializationStep& step : plan.initializationSteps)
        {
            output << "      step=" << objectInitializationStepKindName(step.kind);
            if (step.constructorMemberOrdinal.has_value() &&
                *step.constructorMemberOrdinal < members.members().size())
            {
                output << " constructor=" << members.members()[*step.constructorMemberOrdinal].signatureKey;
            }
            output << '\n';
        }
        if (plan.targetTypeOrdinal < names.namedTypes().size())
        {
            output << "      class=" << names.namedTypes()[plan.targetTypeOrdinal].identity.key() << '\n';
        }
    }
}
}
