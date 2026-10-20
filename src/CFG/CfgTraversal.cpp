#include "nativesharp/CFG/CfgTraversal.h"

#include "Internal/DfsFrame.h"

#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgNode.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <vector>

namespace nativesharp::cfg
{
using internal::DfsFrame;

std::vector<CfgNode*> CfgTraversal::reversePostOrder(CfgNode* entry)
{
    std::vector<CfgNode*> postOrder;
    if (entry == nullptr)
    {
        return postOrder;
    }

    std::unordered_set<CfgNode*> visited;
    std::vector<DfsFrame> stack;
    visited.insert(entry);
    stack.push_back({entry, 0});

    while (!stack.empty())
    {
        DfsFrame& frame = stack.back();
        const std::vector<CfgEdge*>& outgoing = frame.node->outgoingEdges();
        if (frame.nextEdge < outgoing.size())
        {
            CfgEdge* edge = outgoing[frame.nextEdge++];
            if (edge == nullptr || edge->target() == nullptr)
            {
                continue;
            }
            CfgNode* target = edge->target();
            if (visited.insert(target).second)
            {
                stack.push_back({target, 0});
            }
            continue;
        }

        postOrder.push_back(frame.node);
        stack.pop_back();
    }

    std::reverse(postOrder.begin(), postOrder.end());
    return postOrder;
}
}
