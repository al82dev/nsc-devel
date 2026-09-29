#include "nativesharp/Frontend/ProjectGenericModelDumper.h"

#include "nativesharp/Frontend/ProjectGenericModel.h"

#include <sstream>

namespace nativesharp::frontend
{
std::string ProjectGenericModelDumper::dump(const ProjectGenericModel& model)
{
    std::ostringstream output;
    const GenericInstantiationReport report = model.instantiationReport();
    output << "project.generics templates=" << model.templates().size()
           << " specializations=" << model.specializations().size()
           << " materialized=" << report.materializedCount
           << " cache-hits=" << report.cacheHitCount
           << " maximum-depth=" << report.maximumObservedDepth
           << " depth-limit=" << report.maximumDepth
           << " count-limit=" << report.maximumCount << '\n';
    for (const GenericTemplateRecord& record : model.templates())
    {
        output << "template " << genericTemplateKindName(record.kind)
               << " key=" << record.canonicalKey
               << " params=" << record.parameters.size();
        if (!record.ownerName.empty()) output << " owner=" << record.ownerName;
        output << '\n';
    }
    for (const GenericSpecializationRecord& record : model.specializations())
    {
        output << "specialization #" << record.cacheOrdinal
               << " kind=" << genericTemplateKindName(record.kind)
               << " origin=" << record.templateKey
               << " key=" << record.specializationKey
               << " name=" << record.specializedName
               << " args=" << record.typeArguments.size() << '\n';
        for (std::size_t index = 0; index < record.typeArguments.size(); ++index)
            output << "  arg[" << index << "]=" << record.typeArguments[index] << '\n';
    }
    return output.str();
}
}
