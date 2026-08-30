#include "nativesharp/Dominance/DominanceVerifier.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"

#include <algorithm>
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
using NodeSet = std::unordered_set<const cfg::CfgNode*>;
using DominatorSets = std::unordered_map<const cfg::CfgNode*, NodeSet>;

bool sameSet(const NodeSet& first, const NodeSet& second)
{
    if (first.size() != second.size())
    {
        return false;
    }
    for (const cfg::CfgNode* node : first)
    {
        if (!second.contains(node))
        {
            return false;
        }
    }
    return true;
}

DominatorSets computeDominatorSets(const cfg::CfgFunction* function)
{
    DominatorSets sets;
    const std::vector<cfg::CfgNode*>& rpo = function->reversePostOrder();
    if (rpo.empty())
    {
        return sets;
    }

    NodeSet all;
    for (const cfg::CfgNode* node : rpo)
    {
        all.insert(node);
    }

    const cfg::CfgNode* entry = function->entryNode();
    sets[entry] = {entry};
    for (std::size_t index = 1; index < rpo.size(); ++index)
    {
        sets[rpo[index]] = all;
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 1; index < rpo.size(); ++index)
        {
            const cfg::CfgNode* node = rpo[index];
            const std::vector<cfg::CfgNode*> predecessors = node->predecessors();
            bool havePredecessor = false;
            NodeSet intersection;

            for (const cfg::CfgNode* predecessor : predecessors)
            {
                if (predecessor == nullptr || !predecessor->reachable())
                {
                    continue;
                }
                const auto predecessorSet = sets.find(predecessor);
                if (predecessorSet == sets.end())
                {
                    continue;
                }
                if (!havePredecessor)
                {
                    intersection = predecessorSet->second;
                    havePredecessor = true;
                }
                else
                {
                    for (auto iterator = intersection.begin(); iterator != intersection.end();)
                    {
                        if (!predecessorSet->second.contains(*iterator))
                        {
                            iterator = intersection.erase(iterator);
                        }
                        else
                        {
                            ++iterator;
                        }
                    }
                }
            }

            if (!havePredecessor)
            {
                intersection.clear();
            }
            intersection.insert(node);
            if (!sameSet(intersection, sets[node]))
            {
                sets[node] = std::move(intersection);
                changed = true;
            }
        }
    }
    return sets;
}

const cfg::CfgNode* expectedImmediateDominator(
    const cfg::CfgNode* node,
    const cfg::CfgNode* entry,
    const DominatorSets& sets)
{
    if (node == entry)
    {
        return nullptr;
    }
    const auto nodeSetFound = sets.find(node);
    if (nodeSetFound == sets.end())
    {
        return nullptr;
    }

    for (const cfg::CfgNode* candidate : nodeSetFound->second)
    {
        if (candidate == node)
        {
            continue;
        }
        const auto candidateSet = sets.find(candidate);
        if (candidateSet == sets.end())
        {
            continue;
        }

        bool nearest = true;
        for (const cfg::CfgNode* other : nodeSetFound->second)
        {
            if (other == node || other == candidate)
            {
                continue;
            }
            if (!candidateSet->second.contains(other))
            {
                nearest = false;
                break;
            }
        }
        if (nearest)
        {
            return candidate;
        }
    }
    return nullptr;
}

bool vectorMatchesSet(
    const std::vector<const cfg::CfgNode*>& actual,
    const NodeSet& expected)
{
    if (actual.size() != expected.size())
    {
        return false;
    }
    std::unordered_set<const cfg::CfgNode*> seen;
    for (const cfg::CfgNode* node : actual)
    {
        if (!expected.contains(node) || !seen.insert(node).second)
        {
            return false;
        }
    }
    return true;
}
}

bool DominanceVerifier::verify(const DominanceModule* module)
{
    error_.clear();
    if (module == nullptr || module->cfgModule() == nullptr)
    {
        return fail("dominance module must reference a CFG module");
    }

    const cfg::CfgModule* cfgModule = module->cfgModule();
    if (module->functions().size() != cfgModule->functions().size())
    {
        return fail("dominance module must contain exactly one analysis per CFG function");
    }

    std::unordered_set<const cfg::CfgFunction*> seen;
    for (const DominanceFunction* function : module->functions())
    {
        if (function == nullptr || function->cfgFunction() == nullptr)
        {
            return fail("dominance module contains an invalid function analysis");
        }
        if (!seen.insert(function->cfgFunction()).second)
        {
            return fail("dominance module contains duplicate CFG function analysis");
        }
        if (cfgModule->findFunction(function->cfgFunction()->mirFunction()) != function->cfgFunction())
        {
            return fail("dominance function does not belong to the source CFG module");
        }
        if (module->findFunction(function->cfgFunction()) != function)
        {
            return fail("dominance module function index is inconsistent");
        }
        if (!verifyFunction(function))
        {
            return false;
        }
    }

    for (const cfg::CfgFunction* cfgFunction : cfgModule->functions())
    {
        if (module->findFunction(cfgFunction) == nullptr)
        {
            return fail("CFG function is missing dominance analysis");
        }
    }
    return true;
}

const std::string& DominanceVerifier::error() const noexcept { return error_; }

bool DominanceVerifier::verifyFunction(const DominanceFunction* function)
{
    const cfg::CfgFunction* cfgFunction = function->cfgFunction();
    const DominatorTree* tree = function->dominatorTree();
    const DominanceFrontier* frontier = function->dominanceFrontier();
    if (tree == nullptr || frontier == nullptr)
    {
        return fail("dominance function requires a dominator tree and dominance frontier");
    }
    if (tree->cfgFunction() != cfgFunction || frontier->tree() != tree)
    {
        return fail("dominance analysis source relationships are inconsistent");
    }

    const std::vector<cfg::CfgNode*>& rpo = cfgFunction->reversePostOrder();
    if (rpo.empty() || cfgFunction->entryNode() == nullptr)
    {
        return fail("dominance analysis requires a reachable CFG entry");
    }
    if (tree->nodes().size() != rpo.size())
    {
        return fail("dominator tree must contain exactly the reachable CFG nodes");
    }
    if (tree->root() == nullptr || tree->root()->cfgNode() != cfgFunction->entryNode())
    {
        return fail("dominator tree root must be the CFG entry");
    }
    if (tree->root()->immediateDominator() != nullptr || tree->root()->depth() != 0)
    {
        return fail("dominator tree root must have no immediate dominator and depth zero");
    }

    std::unordered_set<const cfg::CfgNode*> reachable(rpo.begin(), rpo.end());
    std::unordered_set<const DominatorTreeNode*> treeNodes;
    std::unordered_set<std::uint32_t> traversalNumbers;
    for (std::size_t index = 0; index < tree->nodes().size(); ++index)
    {
        const DominatorTreeNode* node = tree->nodes()[index];
        if (node == nullptr || node->cfgNode() == nullptr || !reachable.contains(node->cfgNode()))
        {
            return fail("dominator tree contains a null or unreachable CFG node");
        }
        if (!treeNodes.insert(node).second || tree->findNode(node->cfgNode()) != node)
        {
            return fail("dominator tree node identity/index is inconsistent");
        }
        if (node->reversePostOrderIndex() != index || node->cfgNode() != rpo[index])
        {
            return fail("dominator tree nodes must preserve CFG reverse-postorder indexing");
        }
        if (node->preorderNumber() >= node->postorderNumber())
        {
            return fail("dominator tree traversal interval must have preorder before postorder");
        }
        if (!traversalNumbers.insert(node->preorderNumber()).second ||
            !traversalNumbers.insert(node->postorderNumber()).second)
        {
            return fail("dominator tree traversal numbers must be unique");
        }
    }

    for (const cfg::CfgNode* cfgNode : cfgFunction->nodes())
    {
        if (!cfgNode->reachable() && tree->findNode(cfgNode) != nullptr)
        {
            return fail("unreachable CFG node must be excluded from dominance analysis");
        }
    }

    const DominatorSets sets = computeDominatorSets(cfgFunction);
    const cfg::CfgNode* entry = cfgFunction->entryNode();
    std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*> expectedIdoms;
    for (const cfg::CfgNode* cfgNode : rpo)
    {
        const cfg::CfgNode* expected = expectedImmediateDominator(cfgNode, entry, sets);
        expectedIdoms[cfgNode] = expected;
        if (tree->immediateDominator(cfgNode) != expected)
        {
            return fail("immediate dominator does not match fixed-point dominator sets");
        }
    }

    for (const DominatorTreeNode* node : tree->nodes())
    {
        const cfg::CfgNode* cfgNode = node->cfgNode();
        const cfg::CfgNode* expectedParent = expectedIdoms[cfgNode];
        const DominatorTreeNode* parent = node->immediateDominator();
        if ((parent == nullptr ? nullptr : parent->cfgNode()) != expectedParent)
        {
            return fail("dominator tree parent does not match immediate dominator");
        }
        if (parent != nullptr && node->depth() != parent->depth() + 1U)
        {
            return fail("dominator tree depth is inconsistent with its parent");
        }

        std::unordered_set<const DominatorTreeNode*> expectedChildren;
        for (const DominatorTreeNode* candidate : tree->nodes())
        {
            if (candidate->immediateDominator() == node)
            {
                expectedChildren.insert(candidate);
            }
        }
        if (node->children().size() != expectedChildren.size())
        {
            return fail("dominator tree child list does not match immediate-dominator relations");
        }
        std::unordered_set<const DominatorTreeNode*> actualChildren;
        for (const DominatorTreeNode* child : node->children())
        {
            if (child == nullptr || !expectedChildren.contains(child) || !actualChildren.insert(child).second)
            {
                return fail("dominator tree child registration is invalid or duplicated");
            }
        }
    }

    for (const cfg::CfgNode* dominator : rpo)
    {
        for (const cfg::CfgNode* node : rpo)
        {
            const bool expected = sets.at(node).contains(dominator);
            if (tree->dominates(dominator, node) != expected)
            {
                return fail("dominates query disagrees with independently computed dominator sets");
            }
            if (tree->strictlyDominates(dominator, node) != (expected && dominator != node))
            {
                return fail("strict dominance query is inconsistent");
            }
        }

        std::vector<const cfg::CfgNode*> expectedChain;
        const cfg::CfgNode* current = dominator;
        while (current != nullptr)
        {
            expectedChain.push_back(current);
            current = expectedIdoms[current];
        }
        std::reverse(expectedChain.begin(), expectedChain.end());
        if (tree->dominators(dominator) != expectedChain)
        {
            return fail("dominator chain query does not match immediate-dominator ancestry");
        }
    }

    for (const cfg::CfgNode* first : rpo)
    {
        for (const cfg::CfgNode* second : rpo)
        {
            const cfg::CfgNode* expected = first;
            while (expected != nullptr && !sets.at(second).contains(expected))
            {
                expected = expectedIdoms[expected];
            }
            if (tree->nearestCommonDominator(first, second) != expected)
            {
                return fail("nearest common dominator query is inconsistent");
            }
        }
    }

    std::unordered_map<const cfg::CfgNode*, std::size_t> rpoIndex;
    for (std::size_t index = 0; index < rpo.size(); ++index)
    {
        rpoIndex.emplace(rpo[index], index);
    }

    for (const cfg::CfgNode* owner : rpo)
    {
        NodeSet expectedFrontier;
        for (const cfg::CfgNode* candidate : rpo)
        {
            bool dominatesPredecessor = false;
            for (const cfg::CfgNode* predecessor : candidate->predecessors())
            {
                if (predecessor != nullptr && predecessor->reachable() &&
                    sets.at(predecessor).contains(owner))
                {
                    dominatesPredecessor = true;
                    break;
                }
            }
            const bool strictlyDominatesCandidate = owner != candidate && sets.at(candidate).contains(owner);
            if (dominatesPredecessor && !strictlyDominatesCandidate)
            {
                expectedFrontier.insert(candidate);
            }
        }

        const std::vector<const cfg::CfgNode*>& actual = frontier->frontier(owner);
        if (!vectorMatchesSet(actual, expectedFrontier))
        {
            return fail("dominance frontier does not match its graph-theoretic definition");
        }
        for (std::size_t index = 1; index < actual.size(); ++index)
        {
            if (rpoIndex.at(actual[index - 1]) >= rpoIndex.at(actual[index]))
            {
                return fail("dominance frontier must be unique and sorted by CFG reverse postorder");
            }
        }
    }

    return true;
}

bool DominanceVerifier::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
