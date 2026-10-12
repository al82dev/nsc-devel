#include "nativesharp/CFG/CfgBuilder.h"

#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgTraversal.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"

#include <utility>
#include <vector>

namespace nativesharp::cfg
{
namespace
{
constexpr char kInvalidFunctionError[] = "CFG builder received an invalid MIR function definition";
}

CfgBuilder::CfgBuilder(CfgArena& arena) : arena_(arena) {}

CfgModule* CfgBuilder::build(const mir::MirModule* module)
{
    if (module == nullptr)
    {
        fail("CFG builder requires a MIR module");
        return nullptr;
    }

    for (const mir::MirFunction* function : module->functions())
    {
        if (function == nullptr)
        {
            fail(kInvalidFunctionError);
            return nullptr;
        }
    }

    CfgModule* result = arena_.makeModule(module);
    for (const mir::MirFunction* function : module->functions())
    {
        if (function->defined() && !buildFunction(function, result))
        {
            return nullptr;
        }
    }
    return result;
}

bool CfgBuilder::succeeded() const noexcept { return error_.empty(); }
const std::string& CfgBuilder::error() const noexcept { return error_; }

bool CfgBuilder::buildFunction(const mir::MirFunction* function, CfgModule* module)
{
    if (function == nullptr || !function->defined())
    {
        return fail(kInvalidFunctionError);
    }

    CfgFunction* cfgFunction = arena_.makeFunction(function);
    module->addFunction(cfgFunction);
    for (const mir::MirBlock* block : function->blocks())
    {
        if (block == nullptr)
        {
            return fail("CFG builder encountered a null MIR block");
        }
        cfgFunction->addNode(arena_.makeNode(block));
    }

    if (cfgFunction->entryNode() == nullptr)
    {
        return fail("defined MIR function has no entry block");
    }
    if (!buildEdges(cfgFunction))
    {
        return false;
    }
    computeTraversal(cfgFunction);
    return true;
}

bool CfgBuilder::buildEdges(CfgFunction* function)
{
    for (CfgNode* node : function->nodes())
    {
        const mir::MirBlock* block = node->block();
        if (block == nullptr || block->instructions().empty())
        {
            return fail("CFG source block has no terminator");
        }
        const mir::MirInstruction* terminator = block->instructions().back();
        if (terminator == nullptr)
        {
            return fail("CFG source block has null terminator");
        }

        switch (terminator->opcode())
        {
            case mir::MirOpcode::Jump:
                if (!addEdge(function, node, terminator->firstTarget(), CfgEdgeKind::Jump))
                {
                    return false;
                }
                break;

            case mir::MirOpcode::Branch:
                if (!addEdge(function, node, terminator->firstTarget(), CfgEdgeKind::BranchTrue) ||
                    !addEdge(function, node, terminator->secondTarget(), CfgEdgeKind::BranchFalse))
                {
                    return false;
                }
                break;

            case mir::MirOpcode::Return:
            case mir::MirOpcode::Unreachable:
                break;

            default:
                return fail("CFG source block does not end in a control-flow MIR terminator");
        }
    }
    return true;
}

bool CfgBuilder::addEdge(
    CfgFunction* function,
    CfgNode* source,
    const mir::MirBlock* targetBlock,
    CfgEdgeKind kind)
{
    if (source == nullptr || targetBlock == nullptr)
    {
        return fail("CFG edge has null source or target");
    }
    CfgNode* target = function->findNode(targetBlock);
    if (target == nullptr)
    {
        return fail("CFG edge target does not belong to the current MIR function");
    }

    CfgEdge* edge = arena_.makeEdge(source, target, kind);
    function->addEdge(edge);
    source->addOutgoing(edge);
    target->addIncoming(edge);
    return true;
}

void CfgBuilder::computeTraversal(CfgFunction* function)
{
    for (CfgNode* node : function->nodes())
    {
        node->setReachable(false);
    }

    std::vector<CfgNode*> reversePostOrder = CfgTraversal::reversePostOrder(function->entryNode());
    for (CfgNode* node : reversePostOrder)
    {
        node->setReachable(true);
    }
    function->setReversePostOrder(std::move(reversePostOrder));

    for (CfgNode* node : function->nodes())
    {
        if (node->outgoingEdges().empty())
        {
            function->addExitNode(node);
            if (node->reachable())
            {
                function->addReachableExitNode(node);
            }
        }
    }
}

bool CfgBuilder::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
