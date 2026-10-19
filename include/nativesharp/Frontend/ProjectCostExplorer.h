#pragma once

#include "nativesharp/Frontend/ProjectCostReport.h"
#include "nativesharp/Frontend/ProjectFunctionCosts.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <span>
#include <string>

namespace nativesharp::frontend
{
ProjectCostReport exploreProjectCosts(std::span<const ProjectOopMirFunction> functions);
std::string projectCostReportText(const ProjectCostReport& report);
std::string projectCostReportJson(const ProjectCostReport& report);
}
