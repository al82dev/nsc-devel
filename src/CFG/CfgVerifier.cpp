#include "nativesharp/CFG/CfgVerifier.h"

#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgEdgeKind.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgTraversal.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace nativesharp::cfg
{
namespace
{
// `containsPointer` vivia aqui y no lo usaba nadie. Lo sustituyo `countPointer`, que hace la
// comprobacion FUERTE que este verificador necesita de verdad: una arista tiene que aparecer
// EXACTAMENTE UNA VEZ en las listas de sus dos extremos (ver verifyEdge), no "al menos una". Un
// duplicado es un grafo mal construido, y `contains` lo habria dado por bueno.
//
// Se elimina en vez de silenciarse: clang 23 lo caza con -Wunused-template y clang 22 no, pero el
// warning tiene razon en las dos versiones. `std::find` se iba con el; `<algorithm>` se queda por
// `std::count` y `std::equal`.
std::size_t countPointer(const std::vector<CfgEdge*>& values, const CfgEdge* value)
{
    return static_cast<std::size_t>(std::count(values.begin(), values.end(), value));
}

bool sameNodes(const std::vector<CfgNode*>& left, const std::vector<CfgNode*>& right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}
}

bool CfgVerifier::verify(const CfgModule* module)
{
    error_.clear();
    seenFunctions_.clear();
    if (module == nullptr || module->mirModule() == nullptr)
    {
        return fail("CFG module must reference its source MIR module");
    }

    std::size_t definedMirFunctions = 0;
    for (const mir::MirFunction* mirFunction : module->mirModule()->functions())
    {
        if (mirFunction != nullptr && mirFunction->defined())
        {
            ++definedMirFunctions;
            if (module->findFunction(mirFunction) == nullptr)
            {
                return fail("defined MIR function is missing its CFG");
            }
        }
        else if (mirFunction != nullptr && module->findFunction(mirFunction) != nullptr)
        {
            return fail("MIR prototype must not have a CFG");
        }
    }

    if (module->functions().size() != definedMirFunctions)
    {
        return fail("CFG module function count does not match defined MIR functions");
    }

    for (const CfgFunction* function : module->functions())
    {
        if (!verifyFunction(function))
        {
            return false;
        }
    }
    return true;
}

const std::string& CfgVerifier::error() const noexcept { return error_; }

bool CfgVerifier::verifyFunction(const CfgFunction* function)
{
    if (function == nullptr || function->mirFunction() == nullptr || !function->mirFunction()->defined())
    {
        return fail("CFG function must reference a defined MIR function");
    }
    if (!seenFunctions_.insert(function->mirFunction()).second)
    {
        return fail("duplicate CFG function for the same MIR function");
    }

    const std::vector<mir::MirBlock*>& mirBlocks = function->mirFunction()->blocks();
    if (function->nodes().size() != mirBlocks.size() || function->nodes().empty())
    {
        return fail("CFG nodes must correspond one-to-one with MIR blocks");
    }

    nodes_.clear();
    edges_.clear();
    for (std::size_t index = 0; index < function->nodes().size(); ++index)
    {
        const CfgNode* node = function->nodes()[index];
        if (node == nullptr || node->block() != mirBlocks[index])
        {
            return fail("CFG node order must preserve MIR block order");
        }
        if (!nodes_.insert(node).second)
        {
            return fail("CFG contains duplicate node pointers");
        }
    }

    if (function->entryNode() != function->nodes().front() ||
        function->entryNode()->block() != function->mirFunction()->entryBlock())
    {
        return fail("CFG entry must correspond to MIR entry block");
    }

    for (const CfgEdge* edge : function->edges())
    {
        if (edge == nullptr || !edges_.insert(edge).second)
        {
            return fail("CFG contains null or duplicate edge pointers");
        }
    }

    for (const CfgNode* node : function->nodes())
    {
        if (!verifyNode(node, function) || !verifyTerminatorEdges(node))
        {
            return false;
        }
    }
    for (const CfgEdge* edge : function->edges())
    {
        if (!verifyEdge(edge, function))
        {
            return false;
        }
    }

    return verifyTraversal(function) && verifyExitSets(function);
}

bool CfgVerifier::verifyNode(const CfgNode* node, const CfgFunction* function)
{
    if (node == nullptr || node->block() == nullptr || function->findNode(node->block()) != node)
    {
        return fail("CFG node must wrap one block from its MIR function");
    }

    std::unordered_set<const CfgEdge*> localIncoming;
    for (const CfgEdge* edge : node->incomingEdges())
    {
        if (edge == nullptr || !edges_.contains(edge) || edge->target() != node ||
            !localIncoming.insert(edge).second)
        {
            return fail("CFG incoming edge registration is inconsistent");
        }
    }

    std::unordered_set<const CfgEdge*> localOutgoing;
    for (const CfgEdge* edge : node->outgoingEdges())
    {
        if (edge == nullptr || !edges_.contains(edge) || edge->source() != node ||
            !localOutgoing.insert(edge).second)
        {
            return fail("CFG outgoing edge registration is inconsistent");
        }
    }
    return true;
}

bool CfgVerifier::verifyEdge(const CfgEdge* edge, const CfgFunction* function)
{
    if (edge == nullptr || edge->source() == nullptr || edge->target() == nullptr ||
        !nodes_.contains(edge->source()) || !nodes_.contains(edge->target()))
    {
        return fail("CFG edge endpoints must belong to the same CFG function");
    }
    if (function->findNode(edge->source()->block()) != edge->source() ||
        function->findNode(edge->target()->block()) != edge->target())
    {
        return fail("CFG edge endpoint lookup is inconsistent");
    }
    if (countPointer(edge->source()->outgoingEdges(), edge) != 1 ||
        countPointer(edge->target()->incomingEdges(), edge) != 1)
    {
        return fail("CFG edge must be registered exactly once at both endpoints");
    }
    return true;
}

bool CfgVerifier::verifyTerminatorEdges(const CfgNode* node)
{
    const mir::MirBlock* block = node->block();
    if (block->instructions().empty())
    {
        return fail("CFG node wraps MIR block without terminator");
    }
    const mir::MirInstruction* terminator = block->instructions().back();
    if (terminator == nullptr)
    {
        return fail("CFG node wraps MIR block with null terminator");
    }

    const std::vector<CfgEdge*>& outgoing = node->outgoingEdges();
    switch (terminator->opcode())
    {
        case mir::MirOpcode::Jump:
            if (outgoing.size() != 1 || outgoing[0]->kind() != CfgEdgeKind::Jump ||
                outgoing[0]->target()->block() != terminator->firstTarget())
            {
                return fail("jump terminator must map to one matching CFG jump edge");
            }
            return true;

        case mir::MirOpcode::Branch:
        {
            if (outgoing.size() != 2)
            {
                return fail("branch terminator must map to exactly two CFG edges");
            }
            const CfgEdge* trueEdge = nullptr;
            const CfgEdge* falseEdge = nullptr;
            for (const CfgEdge* edge : outgoing)
            {
                if (edge->kind() == CfgEdgeKind::BranchTrue)
                {
                    trueEdge = edge;
                }
                else if (edge->kind() == CfgEdgeKind::BranchFalse)
                {
                    falseEdge = edge;
                }
            }
            if (trueEdge == nullptr || falseEdge == nullptr ||
                trueEdge->target()->block() != terminator->firstTarget() ||
                falseEdge->target()->block() != terminator->secondTarget())
            {
                return fail("branch CFG edges must preserve MIR true/false targets");
            }
            return true;
        }

        case mir::MirOpcode::Return:
        case mir::MirOpcode::Unreachable:
            return outgoing.empty()
                ? true
                : fail("return/unreachable MIR block must have no CFG successors");

        default:
            return fail("CFG node source MIR block has non-control-flow terminator");
    }
}

bool CfgVerifier::verifyTraversal(const CfgFunction* function)
{
    const std::vector<CfgNode*> expectedRpo = CfgTraversal::reversePostOrder(function->entryNode());
    if (!sameNodes(function->reversePostOrder(), expectedRpo))
    {
        return fail("CFG reverse postorder does not match graph traversal");
    }

    std::unordered_set<const CfgNode*> reachable(expectedRpo.begin(), expectedRpo.end());
    for (const CfgNode* node : function->nodes())
    {
        if (node->reachable() != reachable.contains(node))
        {
            return fail("CFG reachable flag does not match entry traversal");
        }
    }
    return true;
}

bool CfgVerifier::verifyExitSets(const CfgFunction* function)
{
    std::vector<CfgNode*> expectedExits;
    std::vector<CfgNode*> expectedReachableExits;
    for (CfgNode* node : function->nodes())
    {
        if (node->outgoingEdges().empty())
        {
            expectedExits.push_back(node);
            if (node->reachable())
            {
                expectedReachableExits.push_back(node);
            }
        }
    }

    if (!sameNodes(function->exitNodes(), expectedExits))
    {
        return fail("CFG exit node set is inconsistent");
    }
    if (!sameNodes(function->reachableExitNodes(), expectedReachableExits))
    {
        return fail("CFG reachable exit node set is inconsistent");
    }
    return true;
}

bool CfgVerifier::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
