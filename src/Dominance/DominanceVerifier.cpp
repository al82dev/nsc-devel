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
using ImmediateDominators = std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>;
using RpoIndices = std::unordered_map<const cfg::CfgNode*, std::size_t>;

const cfg::CfgNode* intersectImmediateDominators(
    const cfg::CfgNode* first,
    const cfg::CfgNode* second,
    const ImmediateDominators& idoms,
    const RpoIndices& rpoIndices)
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

bool computeExpectedImmediateDominators(
    const cfg::CfgFunction* function,
    ImmediateDominators& idoms,
    RpoIndices& rpoIndices)
{
    const std::vector<cfg::CfgNode*>& rpo = function->reversePostOrder();
    for (std::size_t index = 0; index < rpo.size(); ++index)
    {
        rpoIndices.emplace(rpo[index], index);
    }

    const cfg::CfgNode* entry = function->entryNode();
    idoms.emplace(entry, entry);
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 1; index < rpo.size(); ++index)
        {
            const cfg::CfgNode* block = rpo[index];
            const cfg::CfgNode* newIdom = nullptr;
            for (const cfg::CfgNode* predecessor : block->predecessors())
            {
                if (predecessor == nullptr || !predecessor->reachable() || !idoms.contains(predecessor))
                {
                    continue;
                }
                newIdom = newIdom == nullptr ? predecessor :
                    intersectImmediateDominators(newIdom, predecessor, idoms, rpoIndices);
                if (newIdom == nullptr)
                {
                    return false;
                }
            }
            if (newIdom == nullptr)
            {
                return false;
            }
            const auto found = idoms.find(block);
            if (found == idoms.end() || found->second != newIdom)
            {
                idoms[block] = newIdom;
                changed = true;
            }
        }
    }
    return idoms.size() == rpo.size();
}

bool expectedDominates(
    const cfg::CfgNode* dominator,
    const cfg::CfgNode* node,
    const cfg::CfgNode* entry,
    const ImmediateDominators& idoms)
{
    const cfg::CfgNode* cursor = node;
    for (std::size_t step = 0; cursor != nullptr && step <= idoms.size(); ++step)
    {
        if (cursor == dominator)
        {
            return true;
        }
        if (cursor == entry)
        {
            return false;
        }
        const auto parent = idoms.find(cursor);
        cursor = parent == idoms.end() ? nullptr : parent->second;
    }
    return false;
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

    const cfg::CfgNode* entry = cfgFunction->entryNode();
    ImmediateDominators expectedIdoms;
    RpoIndices rpoIndex;
    if (!computeExpectedImmediateDominators(cfgFunction, expectedIdoms, rpoIndex))
    {
        return fail("independent immediate-dominator analysis did not converge over the reachable CFG");
    }
    for (const cfg::CfgNode* cfgNode : rpo)
    {
        const cfg::CfgNode* expected = cfgNode == entry ? nullptr : expectedIdoms.at(cfgNode);
        if (tree->immediateDominator(cfgNode) != expected)
        {
            return fail("immediate dominator does not match independent fixed-point analysis");
        }
    }

    std::unordered_map<const DominatorTreeNode*, std::unordered_set<const DominatorTreeNode*>> expectedChildren;
    for (const DominatorTreeNode* node : tree->nodes())
    {
        expectedChildren.emplace(node, std::unordered_set<const DominatorTreeNode*>{});
    }
    for (const DominatorTreeNode* node : tree->nodes())
    {
        if (node == tree->root())
        {
            continue;
        }
        const cfg::CfgNode* expectedParentNode = expectedIdoms.at(node->cfgNode());
        const DominatorTreeNode* expectedParent = tree->findNode(expectedParentNode);
        if (expectedParent == nullptr)
        {
            return fail("independent immediate dominator is absent from the materialized tree");
        }
        expectedChildren[expectedParent].insert(node);
    }

    for (const DominatorTreeNode* node : tree->nodes())
    {
        const cfg::CfgNode* cfgNode = node->cfgNode();
        const cfg::CfgNode* expectedParent = cfgNode == entry ? nullptr : expectedIdoms.at(cfgNode);
        const DominatorTreeNode* parent = node->immediateDominator();
        if ((parent == nullptr ? nullptr : parent->cfgNode()) != expectedParent)
        {
            return fail("dominator tree parent does not match immediate dominator");
        }
        if (parent != nullptr && node->depth() != parent->depth() + 1U)
        {
            return fail("dominator tree depth is inconsistent with its parent");
        }
        if (parent != nullptr &&
            (parent->preorderNumber() > node->preorderNumber() ||
             parent->postorderNumber() < node->postorderNumber()))
        {
            return fail("dominator tree child interval must be nested inside its parent interval");
        }

        const auto& childrenExpected = expectedChildren.at(node);
        if (node->children().size() != childrenExpected.size())
        {
            return fail("dominator tree child list does not match immediate-dominator relations");
        }
        std::unordered_set<const DominatorTreeNode*> actualChildren;
        for (const DominatorTreeNode* child : node->children())
        {
            if (child == nullptr || !childrenExpected.contains(child) || !actualChildren.insert(child).second)
            {
                return fail("dominator tree child registration is invalid or duplicated");
            }
        }
        std::vector<const DominatorTreeNode*> orderedChildren(node->children().begin(), node->children().end());
        std::sort(orderedChildren.begin(), orderedChildren.end(),
            [](const DominatorTreeNode* first, const DominatorTreeNode* second)
            {
                return first->preorderNumber() < second->preorderNumber();
            });
        for (std::size_t index = 1; index < orderedChildren.size(); ++index)
        {
            if (orderedChildren[index - 1]->postorderNumber() >= orderedChildren[index]->preorderNumber())
            {
                return fail("sibling dominator-tree traversal intervals must not overlap");
            }
        }
    }

    for (const cfg::CfgNode* node : rpo)
    {
        if (!tree->dominates(entry, node) || !tree->dominates(node, node) ||
            tree->strictlyDominates(node, node))
        {
            return fail("dominance query violates root, identity or strict-identity contracts");
        }
        if (node != entry)
        {
            const cfg::CfgNode* parent = expectedIdoms.at(node);
            if (!tree->dominates(parent, node) || !tree->strictlyDominates(parent, node) ||
                tree->nearestCommonDominator(parent, node) != parent)
            {
                return fail("dominance queries disagree with the immediate-dominator relation");
            }
        }
    }

    constexpr std::size_t exhaustiveQueryLimit = 64U;
    if (rpo.size() <= exhaustiveQueryLimit)
    {
        for (const cfg::CfgNode* dominator : rpo)
        {
            for (const cfg::CfgNode* node : rpo)
            {
                const bool expected = expectedDominates(dominator, node, entry, expectedIdoms);
                if (tree->dominates(dominator, node) != expected ||
                    tree->strictlyDominates(dominator, node) != (expected && dominator != node))
                {
                    return fail("dominance query disagrees with independent immediate-dominator ancestry");
                }
            }

            std::vector<const cfg::CfgNode*> expectedChain;
            const cfg::CfgNode* current = dominator;
            while (true)
            {
                expectedChain.push_back(current);
                if (current == entry)
                {
                    break;
                }
                current = expectedIdoms.at(current);
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
                const cfg::CfgNode* expected =
                    intersectImmediateDominators(first, second, expectedIdoms, rpoIndex);
                if (tree->nearestCommonDominator(first, second) != expected)
                {
                    return fail("nearest common dominator query is inconsistent");
                }
            }
        }
    }
    else
    {
        const cfg::CfgNode* tail = rpo.back();
        std::vector<const cfg::CfgNode*> expectedTailChain;
        const cfg::CfgNode* current = tail;
        while (true)
        {
            expectedTailChain.push_back(current);
            if (current == entry)
            {
                break;
            }
            current = expectedIdoms.at(current);
        }
        std::reverse(expectedTailChain.begin(), expectedTailChain.end());
        if (tree->dominators(tail) != expectedTailChain)
        {
            return fail("large dominator chain query does not match immediate-dominator ancestry");
        }
    }

    std::unordered_map<const cfg::CfgNode*, NodeSet> expectedFrontiers;
    for (const cfg::CfgNode* owner : rpo)
    {
        expectedFrontiers.emplace(owner, NodeSet{});
    }
    for (const cfg::CfgNode* candidate : rpo)
    {
        std::vector<const cfg::CfgNode*> reachablePredecessors;
        for (const cfg::CfgNode* predecessor : candidate->predecessors())
        {
            if (predecessor != nullptr && predecessor->reachable())
            {
                reachablePredecessors.push_back(predecessor);
            }
        }
        if (reachablePredecessors.size() < 2U)
        {
            continue;
        }
        const cfg::CfgNode* stop = candidate == entry ? entry : expectedIdoms.at(candidate);
        for (const cfg::CfgNode* predecessor : reachablePredecessors)
        {
            const cfg::CfgNode* runner = predecessor;
            for (std::size_t step = 0; runner != stop && step <= rpo.size(); ++step)
            {
                if (runner == nullptr || !expectedFrontiers.contains(runner))
                {
                    return fail("frontier oracle could not reach the candidate immediate dominator");
                }
                expectedFrontiers[runner].insert(candidate);
                runner = runner == entry ? nullptr : expectedIdoms.at(runner);
            }
            if (runner != stop)
            {
                return fail("frontier oracle exceeded the bounded immediate-dominator chain");
            }
        }
    }

    for (const cfg::CfgNode* owner : rpo)
    {
        const std::vector<const cfg::CfgNode*>& actual = frontier->frontier(owner);
        if (!vectorMatchesSet(actual, expectedFrontiers.at(owner)))
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
