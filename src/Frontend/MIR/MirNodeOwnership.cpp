#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <memory>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
void destroyChildrenIteratively(ProjectOopMirNode& root) noexcept
{
    std::vector<std::pair<std::unique_ptr<ProjectOopMirNode>, std::size_t>> work;
    std::size_t nextRootChild = 0;
    while (nextRootChild < root.children.size() || !work.empty())
    {
        if (work.empty())
        {
            std::unique_ptr<ProjectOopMirNode> child =
                std::move(root.children[nextRootChild++]);
            if (child != nullptr) work.emplace_back(std::move(child), 0U);
            continue;
        }
        auto& [node, nextChild] = work.back();
        if (nextChild < node->children.size())
        {
            std::unique_ptr<ProjectOopMirNode> child =
                std::move(node->children[nextChild++]);
            if (child != nullptr) work.emplace_back(std::move(child), 0U);
            continue;
        }
        node->children.clear();
        work.pop_back();
    }
    root.children.clear();
}
}

ProjectOopMirNode::~ProjectOopMirNode() noexcept
{
    destroyChildrenIteratively(*this);
}
}
