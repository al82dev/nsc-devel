#pragma once

#include <vector>

namespace nativesharp::cfg
{
class CfgNode;

class CfgTraversal final
{
public:
    static std::vector<CfgNode*> reversePostOrder(CfgNode* entry);
};
}
