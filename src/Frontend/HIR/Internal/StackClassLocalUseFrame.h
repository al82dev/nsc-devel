#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
struct ProjectOopHirNode;
}

namespace nativesharp::frontend::internal
{
struct StackClassLocalUseFrame final
{
    const ProjectOopHirNode* node = nullptr;
    const ProjectOopHirNode* parent = nullptr;
    const ProjectOopHirNode* grandparent = nullptr;
    std::size_t childIndex = 0;
    std::size_t parentChildIndex = 0;
};
}
