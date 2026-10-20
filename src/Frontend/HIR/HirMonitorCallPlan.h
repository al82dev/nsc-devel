#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
struct HirMonitorCallPlan final
{
    ProjectOopHirNodeKind hirKind;
    bool waits;
};
}
