#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
struct ProjectOopMirNode;
struct ProjectOopHirModelTestAccess;

class ProjectOopMirModelDumper final
{
public:
    static std::string dump(const ProjectOopMirModel& model);

private:
    friend struct ProjectOopHirModelTestAccess;

    static std::string dumpNodeForTesting(const ProjectOopMirNode* node, int depth);
    static std::size_t nodeTraversalDepthForTesting(
        const ProjectOopMirNode* node,
        std::size_t& visitedNodes);
};
}
