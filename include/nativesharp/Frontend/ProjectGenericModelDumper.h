#pragma once

#include <string>

namespace nativesharp::frontend
{
class ProjectGenericModel;

class ProjectGenericModelDumper final
{
public:
    static std::string dump(const ProjectGenericModel& model);
};
}
