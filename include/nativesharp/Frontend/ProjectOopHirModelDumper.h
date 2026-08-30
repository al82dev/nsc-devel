#pragma once

#include <string>

namespace nativesharp::frontend
{
class ProjectOopHirModel;

class ProjectOopHirModelDumper final
{
public:
    static std::string dump(const ProjectOopHirModel& model);
};
}
