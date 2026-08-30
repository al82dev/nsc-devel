#pragma once

#include "nativesharp/Frontend/ProjectInterfaceModel.h"

#include <iosfwd>

namespace nativesharp::frontend
{
class ProjectInterfaceModelDumper final
{
public:
    static void dump(const ProjectInterfaceModel& model, std::ostream& output);
};
}
