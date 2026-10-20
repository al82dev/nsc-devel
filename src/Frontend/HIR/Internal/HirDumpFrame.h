#pragma once

#include "nativesharp/Frontend/ProjectOopHirNode.h"

namespace nativesharp::frontend::internal
{
struct HirDumpFrame final
{
    const ProjectOopHirNode* node = nullptr;
    int depth = 0;
};
}
