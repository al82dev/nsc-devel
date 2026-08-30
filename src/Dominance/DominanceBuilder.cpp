#include "nativesharp/Dominance/DominanceBuilder.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::dominance
{
namespace
{
struct TreeFrame
{
    DominatorTreeNode* node;
    std::size_t nextChild;
    bool entered;
};
}

DominanceBuilder::DominanceBuilder(DominanceArena& arena) : arena_(arena) {}

DominanceModule* DominanceBuilder::build(const cfg::CfgModule* module)
{
    if (module == nullptr)
    {
        fail("dominance builder requires a CFG module");
        return nullptr;
    }

    DominanceModule* result = arena_.makeModule(module);
    for (const cfg::CfgFunction* function : module->functions())
    {
        if (function == nullptr || !buildFunction(function, result))
        {
            return nullptr;
        }
    }
    return result;
}

bool DominanceBuilder::succeeded() const noexcept { return error_.empty(); }
const std::string& DominanceBuilder::error() const noexcept { return error_; }

bool DominanceBuilder::buildFunction(
    const cfg::CfgFunction* cfgFunction,
    DominanceModule* module)
{
    if (cfgFunction == nullptr || cfgFunction->entryNode() == nullptr ||
        cfgFunction->reversePostOrder().empty())
    {
        return fail("dominance builder requires a reachable CFG entry");
    }

    DominanceFunction* function = arena_.makeFunction(cfgFunction);
    module->addFunction(function);

    DominatorTree* tree = arena_.makeTree(cfgFunction);
    function->setDominatorTree(tree);

    std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*> idoms;
    if (!computeImmediateDominators(cfgFunction, idoms) ||
        !materializeTree(cfgFunction, tree, idoms))
    {
        return false;
    }

    numberTree(tree);
    return buildFrontier(function);
}

bool DominanceBuilder::computeImmediateDominators(
    const cfg::CfgFunction* cfgFunction,
    std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms)
{
    const std::vector<cfg::CfgNode*>& rpo = cfgFunction->reversePostOrder();
    std::unordered_map<const cfg::CfgNode*, std::size_t> rpoIndices;
    for (std::size_t index = 0; index < rpo.size(); ++index)
    {
        if (rpo[index] == nullptr || !rpo[index]->reachable())
        {
            return fail("CFG reverse postorder contains an invalid dominance node");
        }
        rpoIndices.emplace(rpo[index], index);
    }

    const cfg::CfgNode* entry = cfgFunction->entryNode();
    idoms[entry] = entry;

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 1; index < rpo.size(); ++index)
        {
            const cfg::CfgNode* block = rpo[index];
            const cfg::CfgNode* newIdom = nullptr;
            const std::vector<cfg::CfgNode*> predecessors = block->predecessors();

            for (const cfg::CfgNode* predecessor : predecessors)
            {
                if (predecessor == nullptr || !predecessor->reachable() || !idoms.contains(predecessor))
                {
                    continue;
                }
                if (newIdom == nullptr)
                {
                    newIdom = predecessor;
                }
                else
                {
                    newIdom = intersect(newIdom, predecessor, idoms, rpoIndices);
                    if (newIdom == nullptr)
                    {
                        return fail("dominance intersection failed to reach a common ancestor");
                    }
                }
            }

            if (newIdom == nullptr)
            {
                return fail("reachable CFG node has no processed predecessor during dominance analysis");
            }

            const auto found = idoms.find(block);
            if (found == idoms.end() || found->second != newIdom)
            {
                idoms[block] = newIdom;
                changed = true;
            }
        }
    }
    return true;
}

const cfg::CfgNode* DominanceBuilder::intersect(
    const cfg::CfgNode* first,
    const cfg::CfgNode* second,
    const std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms,
    const std::unordered_map<const cfg::CfgNode*, std::size_t>& rpoIndices) const
{
    const cfg::CfgNode* left = first;
    const cfg::CfgNode* right = second;
    while (left != right)
    {
        const auto leftIndex = rpoIndices.find(left);
        const auto rightIndex = rpoIndices.find(right);
        if (leftIndex == rpoIndices.end() || rightIndex == rpoIndices.end())
        {
            return nullptr;
        }

        if (leftIndex->second > rightIndex->second)
        {
            const auto parent = idoms.find(left);
            if (parent == idoms.end())
            {
                return nullptr;
            }
            left = parent->second;
        }
        else if (rightIndex->second > leftIndex->second)
        {
            const auto parent = idoms.find(right);
            if (parent == idoms.end())
            {
                return nullptr;
            }
            right = parent->second;
        }
    }
    return left;
}

bool DominanceBuilder::materializeTree(
    const cfg::CfgFunction* cfgFunction,
    DominatorTree* tree,
    const std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms)
{
    const std::vector<cfg::CfgNode*>& rpo = cfgFunction->reversePostOrder();
    for (std::size_t index = 0; index < rpo.size(); ++index)
    {
        DominatorTreeNode* node = arena_.makeTreeNode(rpo[index]);
        node->setReversePostOrderIndex(index);
        tree->addNode(node);
    }

    DominatorTreeNode* root = tree->findNode(cfgFunction->entryNode());
    if (root == nullptr)
    {
        return fail("dominance tree root does not correspond to CFG entry");
    }
    tree->setRoot(root);
    root->setDepth(0);

    for (std::size_t index = 1; index < rpo.size(); ++index)
    {
        cfg::CfgNode* cfgNode = rpo[index];
        const auto idomFound = idoms.find(cfgNode);
        if (idomFound == idoms.end() || idomFound->second == cfgNode)
        {
            return fail("non-entry dominance node has invalid immediate dominator");
        }

        DominatorTreeNode* node = tree->findNode(cfgNode);
        DominatorTreeNode* parent = tree->findNode(idomFound->second);
        if (node == nullptr || parent == nullptr)
        {
            return fail("dominance tree could not materialize immediate-dominator relation");
        }
        node->setImmediateDominator(parent);
        node->setDepth(parent->depth() + 1U);
        parent->addChild(node);
    }
    return true;
}

void DominanceBuilder::numberTree(DominatorTree* tree)
{
    if (tree == nullptr || tree->root() == nullptr)
    {
        return;
    }

    std::uint32_t counter = 0;
    std::unordered_map<DominatorTreeNode*, std::uint32_t> preorders;
    std::vector<TreeFrame> stack;
    stack.push_back({tree->root(), 0, false});

    while (!stack.empty())
    {
        TreeFrame& frame = stack.back();
        if (!frame.entered)
        {
            frame.entered = true;
            preorders[frame.node] = counter++;
        }

        if (frame.nextChild < frame.node->children().size())
        {
            DominatorTreeNode* child = frame.node->children()[frame.nextChild++];
            stack.push_back({child, 0, false});
            continue;
        }

        frame.node->setTraversalNumbers(preorders[frame.node], counter++);
        stack.pop_back();
    }
}

bool DominanceBuilder::buildFrontier(DominanceFunction* function)
{
    DominatorTree* tree = function->dominatorTree();
    if (tree == nullptr || tree->cfgFunction() == nullptr)
    {
        return fail("dominance frontier requires a dominator tree");
    }

    DominanceFrontier* frontier = arena_.makeFrontier(tree);
    function->setDominanceFrontier(frontier);

    std::unordered_map<const cfg::CfgNode*, std::unordered_set<const cfg::CfgNode*>> frontierSets;
    for (const cfg::CfgNode* block : tree->cfgFunction()->reversePostOrder())
    {
        std::vector<cfg::CfgNode*> predecessors;
        for (cfg::CfgNode* predecessor : block->predecessors())
        {
            if (predecessor != nullptr && predecessor->reachable())
            {
                predecessors.push_back(predecessor);
            }
        }

        if (predecessors.size() < 2)
        {
            continue;
        }

        const cfg::CfgNode* blockIdom = tree->immediateDominator(block);
        for (const cfg::CfgNode* predecessor : predecessors)
        {
            const cfg::CfgNode* runner = predecessor;
            while (runner != nullptr && runner != blockIdom)
            {
                frontierSets[runner].insert(block);
                runner = tree->immediateDominator(runner);
            }
        }
    }

    std::unordered_map<const cfg::CfgNode*, std::size_t> rpoIndices;
    const std::vector<cfg::CfgNode*>& rpo = tree->cfgFunction()->reversePostOrder();
    for (std::size_t index = 0; index < rpo.size(); ++index)
    {
        rpoIndices.emplace(rpo[index], index);
    }

    for (const cfg::CfgNode* node : rpo)
    {
        std::vector<const cfg::CfgNode*> values;
        const auto found = frontierSets.find(node);
        if (found != frontierSets.end())
        {
            values.assign(found->second.begin(), found->second.end());
            for (std::size_t index = 1; index < values.size(); ++index)
            {
                const cfg::CfgNode* value = values[index];
                std::size_t position = index;
                while (position > 0 && rpoIndices.at(value) < rpoIndices.at(values[position - 1]))
                {
                    values[position] = values[position - 1];
                    --position;
                }
                values[position] = value;
            }
        }
        frontier->setFrontier(node, std::move(values));
    }
    return true;
}

bool DominanceBuilder::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
