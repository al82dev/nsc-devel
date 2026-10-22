#include "Internal/MirNodeMachine.h"

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopMirNode> ProjectOopMirModel::lowerNode(
    const ProjectOopHirNode* node,
    std::string& error)
{
    return MirNodeMachine(*this).lower(node, error);
}

std::unique_ptr<ProjectOopMirNode> ProjectOopMirModel::lowerNode(
    const ProjectOopHirNode* node,
    std::string& error,
    std::span<const ProjectOopMirStructLayout> structLayouts)
{
    return MirNodeMachine(*this, structLayouts).lower(node, error);
}
}
