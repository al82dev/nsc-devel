#pragma once

#include "nativesharp/Frontend/ProjectOopMirModel.h"

namespace nativesharp::frontend
{
class ProjectOopMirModel::MirVerifier final
{
public:
    explicit MirVerifier(ProjectOopMirModel& model) noexcept;

    bool finalize(std::string& error);

private:
    std::size_t recordGenericSpecialization(std::string_view key);
    void recordFunctionSpecializations();
    void recordStaticFieldSpecializations();
    bool recordDispatchSpecializationsAndBindings(std::string& error);
    bool appendInterfaceBinding(
        const ProjectOopMirDispatchTable& table,
        const ProjectOopMirDispatchTable::InterfaceTable& source,
        std::string& error);
    bool validatePanicHandler(std::string& error);

    ProjectOopMirModel& model_;
};
}
