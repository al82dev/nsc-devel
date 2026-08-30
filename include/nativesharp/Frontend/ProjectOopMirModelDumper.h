#pragma once

#include <string>

namespace nativesharp::frontend
{
class ProjectOopMirModel;

class ProjectOopMirModelDumper final
{
public:
    static std::string dump(const ProjectOopMirModel& model);
};
}
