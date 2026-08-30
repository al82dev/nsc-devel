#include "nativesharp/CFG/CfgDumper.h"

#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgEdgeKind.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/Sema/Symbol.h"

#include <ostream>

namespace nativesharp::cfg
{
namespace
{
void dumpBlockRef(const CfgNode* node, std::ostream& output)
{
    if (node == nullptr || node->block() == nullptr)
    {
        output << "<null>";
        return;
    }
    output << "bb" << node->block()->id();
}
}

void CfgDumper::dump(const CfgModule* module, std::ostream& output) const
{
    output << "cfg.module\n";
    if (module == nullptr)
    {
        output << "<null>\n";
        return;
    }

    for (const CfgFunction* function : module->functions())
    {
        dumpFunction(function, output);
    }
}

void CfgDumper::dumpFunction(const CfgFunction* function, std::ostream& output) const
{
    if (function == nullptr || function->mirFunction() == nullptr ||
        function->mirFunction()->symbol() == nullptr)
    {
        output << "function <invalid>\n";
        return;
    }

    output << "function @" << function->mirFunction()->symbol()->name() << '\n';
    output << "  nodes: " << function->nodes().size()
           << " edges: " << function->edges().size()
           << " reachable: " << function->reversePostOrder().size()
           << " unreachable: "
           << (function->nodes().size() - function->reversePostOrder().size()) << '\n';
    output << "  entry: ";
    dumpBlockRef(function->entryNode(), output);
    output << '\n';

    output << "  rpo:";
    for (const CfgNode* node : function->reversePostOrder())
    {
        output << ' ';
        dumpBlockRef(node, output);
    }
    output << '\n';

    output << "  exits:";
    if (function->exitNodes().empty())
    {
        output << " -";
    }
    else
    {
        for (const CfgNode* node : function->exitNodes())
        {
            output << ' ';
            dumpBlockRef(node, output);
        }
    }
    output << '\n';

    output << "  reachable-exits:";
    if (function->reachableExitNodes().empty())
    {
        output << " -";
    }
    else
    {
        for (const CfgNode* node : function->reachableExitNodes())
        {
            output << ' ';
            dumpBlockRef(node, output);
        }
    }
    output << '\n';

    for (const CfgNode* node : function->nodes())
    {
        dumpNode(node, output);
    }
}

void CfgDumper::dumpNode(const CfgNode* node, std::ostream& output) const
{
    if (node == nullptr || node->block() == nullptr)
    {
        output << "<invalid-node>\n";
        return;
    }

    output << "bb" << node->block()->id() << ' ' << node->block()->name()
           << " [" << (node->reachable() ? "reachable" : "unreachable") << "]\n";

    output << "  pred:";
    const std::vector<CfgNode*> predecessors = node->predecessors();
    if (predecessors.empty())
    {
        output << " -";
    }
    else
    {
        for (const CfgNode* predecessor : predecessors)
        {
            output << ' ';
            dumpBlockRef(predecessor, output);
        }
    }
    output << '\n';

    output << "  succ:";
    if (node->outgoingEdges().empty())
    {
        output << " -";
    }
    else
    {
        for (const CfgEdge* edge : node->outgoingEdges())
        {
            output << ' ';
            dumpEdge(edge, output);
        }
    }
    output << '\n';
}

void CfgDumper::dumpEdge(const CfgEdge* edge, std::ostream& output) const
{
    if (edge == nullptr)
    {
        output << "<null-edge>";
        return;
    }
    output << cfgEdgeKindName(edge->kind()) << "->";
    dumpBlockRef(edge->target(), output);
}
}
