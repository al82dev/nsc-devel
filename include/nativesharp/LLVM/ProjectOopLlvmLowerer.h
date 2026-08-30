#pragma once

#include <memory>
#include <string>

namespace nativesharp::frontend
{
class ProjectNameLookup;
class ProjectOopMirModel;
}

namespace nativesharp::llvm_backend
{
class LlvmModule;

class ProjectOopLlvmLowerer final
{
public:
    std::unique_ptr<LlvmModule> lower(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectOopMirModel& mirModel);

    const std::string& error() const noexcept;

private:
    std::string error_;
};
}
