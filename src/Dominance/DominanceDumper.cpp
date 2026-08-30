#include "nativesharp/Dominance/DominanceDumper.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/Sema/Symbol.h"

#include <ostream>
#include <vector>

namespace nativesharp::dominance
{
void DominanceDumper::dump(const DominanceModule* module, std::ostream& output) const
{
    output << "dominance.module\n";
    if (module == nullptr)
    {
        output << "<null>\n";
        return;
    }
    for (const DominanceFunction* function : module->functions())
    {
        dumpFunction(function, output);
    }
}

void DominanceDumper::dumpFunction(const DominanceFunction* function, std::ostream& output) const
{
    if (function == nullptr || function->cfgFunction() == nullptr ||
        function->cfgFunction()->mirFunction() == nullptr ||
        function->cfgFunction()->mirFunction()->symbol() == nullptr ||
        function->dominatorTree() == nullptr || function->dominanceFrontier() == nullptr)
    {
        output << "function <invalid>\n";
        return;
    }

    const cfg::CfgFunction* cfgFunction = function->cfgFunction();
    const DominatorTree* tree = function->dominatorTree();
    const DominanceFrontier* frontier = function->dominanceFrontier();

    output << "function @" << cfgFunction->mirFunction()->symbol()->name() << '\n';
    output << "  reachable: " << tree->nodes().size()
           << " cfg-nodes: " << cfgFunction->nodes().size()
           << " excluded-unreachable: " << (cfgFunction->nodes().size() - tree->nodes().size()) << '\n';
    output << "  root: ";
    dumpBlockRef(tree->root() == nullptr ? nullptr : tree->root()->cfgNode(), output);
    output << '\n';

    output << "  tree:\n";
    dumpTree(tree->root(), output);

    output << "  blocks:\n";
    for (const DominatorTreeNode* node : tree->nodes())
    {
        const cfg::CfgNode* cfgNode = node->cfgNode();
        output << "    ";
        dumpBlockRef(cfgNode, output);
        if (cfgNode != nullptr && cfgNode->block() != nullptr)
        {
            output << ' ' << cfgNode->block()->name();
        }
        output << " idom=";
        dumpBlockRef(node->immediateDominator() == nullptr ? nullptr :
            node->immediateDominator()->cfgNode(), output);
        output << " depth=" << node->depth()
               << " interval=[" << node->preorderNumber() << ',' << node->postorderNumber() << "]";

        output << " dom={";
        const std::vector<const cfg::CfgNode*> dominators = tree->dominators(cfgNode);
        for (std::size_t index = 0; index < dominators.size(); ++index)
        {
            if (index != 0) { output << ' '; }
            dumpBlockRef(dominators[index], output);
        }
        output << "} df={";
        const std::vector<const cfg::CfgNode*>& values = frontier->frontier(cfgNode);
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index != 0) { output << ' '; }
            dumpBlockRef(values[index], output);
        }
        output << "}\n";
    }
}

void DominanceDumper::dumpTree(const DominatorTreeNode* root, std::ostream& output) const
{
    if (root == nullptr)
    {
        output << "    -\n";
        return;
    }

    struct Frame
    {
        const DominatorTreeNode* node;
        std::size_t depth;
    };
    std::vector<Frame> stack;
    stack.push_back({root, 0});
    while (!stack.empty())
    {
        const Frame frame = stack.back();
        stack.pop_back();
        dumpIndent(frame.depth + 2, output);
        dumpBlockRef(frame.node->cfgNode(), output);
        output << '\n';

        const std::vector<DominatorTreeNode*>& children = frame.node->children();
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        {
            stack.push_back({*iterator, frame.depth + 1});
        }
    }
}

void DominanceDumper::dumpBlockRef(const cfg::CfgNode* node, std::ostream& output)
{
    if (node == nullptr || node->block() == nullptr)
    {
        output << "<root>";
        return;
    }
    output << "bb" << node->block()->id();
}

void DominanceDumper::dumpIndent(std::size_t depth, std::ostream& output)
{
    for (std::size_t index = 0; index < depth; ++index)
    {
        output << "  ";
    }
}
}
