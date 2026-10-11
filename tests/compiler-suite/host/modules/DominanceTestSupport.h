#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define private public
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceDumper.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#undef private

namespace nsc::suite::dominance_support
{
using namespace nativesharp;

using NodeSet = std::unordered_set<const cfg::CfgNode*>;
using DominatorSets = std::unordered_map<const cfg::CfgNode*, NodeSet>;
using ContractFunction = std::function<void(TestContext&, std::size_t)>;

/** Produces the stable case identifier for one of the eight variants of a contract. */
inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1U) * 8U + variant;
    std::ostringstream stream;
    stream << "TF-DOM-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

/** Produces the stable contract identifier from its one-based ordinal. */
inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-DOM-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

/** Registers all eight canonical variants without changing their layer or execution profile. */
inline void addDominanceContract(
    TestRegistry& registry,
    std::size_t contractOrdinal,
    std::string layer,
    const ContractFunction& function,
    std::vector<std::size_t> excludedVariants = {},
    std::vector<std::size_t> productDefectVariants = {},
    std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        if (std::find(excludedVariants.begin(), excludedVariants.end(), variant) !=
            excludedVariants.end())
        {
            continue;
        }
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("dominance");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool knownProductDefect = std::find(productDefectVariants.begin(),
            productDefectVariants.end(), variant) != productDefectVariants.end();
        if (knownProductDefect)
        {
            caseTags.push_back("known-product-defect");
        }
        registry.add(TestCase{
            caseId(contractOrdinal, variant),
            contractId(contractOrdinal),
            "Host.Dominance",
            layer,
            variant == 8 ? "NIGHTLY" : "PR",
            std::move(caseTags),
            [function, variant](TestContext& context)
            {
                function(context, variant);
            },
            knownProductDefect ? TestDisposition::KnownProductDefect
                               : TestDisposition::Required});
    }
}

struct FunctionGraph final
{
    cfg::CfgFunction* function = nullptr;
    std::vector<cfg::CfgNode*> nodes;
};

/**
 * Owns a MIR module, a manually assembled CFG, and dominance objects for one test.
 * Pointer identity remains stable for the lifetime of the fixture.
 */
class GraphFixture final
{
public:
    GraphFixture()
    {
        mirModule_ = mirArena_.makeModule();
        cfgModule_ = cfgArena_.makeModule(mirModule_);
    }

    /** Adds one defined function with reachable CFG nodes in natural RPO order. */
    FunctionGraph addFunction(std::string name, std::size_t nodeCount)
    {
        auto symbol = std::make_unique<sema::Symbol>(
            sema::SymbolKind::Function,
            std::move(name),
            types_.functionType(types_.builtin(sema::TypeKind::Int), {}),
            nullptr);
        mir::MirFunction* mirFunction = mirArena_.makeFunction(symbol.get());
        mirFunction->setDefined(true);
        mirModule_->addFunction(mirFunction);
        symbols_.push_back(std::move(symbol));

        FunctionGraph graph;
        graph.function = cfgArena_.makeFunction(mirFunction);
        cfgModule_->addFunction(graph.function);
        for (std::size_t index = 0; index < nodeCount; ++index)
        {
            mir::MirBlock* block = mirArena_.makeBlock("block." + std::to_string(index));
            block->addInstruction(mirArena_.makeInstruction(mir::MirOpcode::Return, {}));
            mirFunction->addBlock(block);
            cfg::CfgNode* node = cfgArena_.makeNode(block);
            node->setReachable(true);
            graph.function->addNode(node);
            graph.nodes.push_back(node);
        }
        graph.function->setReversePostOrder(graph.nodes);
        if (!graph.nodes.empty())
        {
            graph.function->addExitNode(graph.nodes.back());
            graph.function->addReachableExitNode(graph.nodes.back());
        }
        return graph;
    }

    /** Adds a directed CFG edge and updates both endpoint adjacency lists. */
    cfg::CfgEdge* connect(
        FunctionGraph& graph,
        std::size_t source,
        std::size_t target,
        cfg::CfgEdgeKind kind = cfg::CfgEdgeKind::Jump)
    {
        cfg::CfgEdge* edge = cfgArena_.makeEdge(graph.nodes[source], graph.nodes[target], kind);
        graph.function->addEdge(edge);
        graph.nodes[source]->addOutgoing(edge);
        graph.nodes[target]->addIncoming(edge);
        return edge;
    }

    /** Adds a node outside the reachable RPO universe. */
    cfg::CfgNode* addUnreachable(FunctionGraph& graph, std::string name)
    {
        mir::MirBlock* block = mirArena_.makeBlock(std::move(name));
        block->addInstruction(mirArena_.makeInstruction(mir::MirOpcode::Unreachable, {}));
        const_cast<mir::MirFunction*>(graph.function->mirFunction())->addBlock(block);
        cfg::CfgNode* node = cfgArena_.makeNode(block);
        graph.function->addNode(node);
        return node;
    }

    /** Replaces RPO with an explicit node-index sequence. */
    void setReversePostOrder(FunctionGraph& graph, const std::vector<std::size_t>& indices)
    {
        std::vector<cfg::CfgNode*> order;
        order.reserve(indices.size());
        for (const std::size_t index : indices)
        {
            order.push_back(graph.nodes[index]);
        }
        graph.function->setReversePostOrder(std::move(order));
    }

    /** Builds all dominance analyses with the product builder. */
    dominance::DominanceModule* build()
    {
        builder_ = std::make_unique<dominance::DominanceBuilder>(dominanceArena_);
        dominanceModule_ = builder_->build(cfgModule_);
        return dominanceModule_;
    }

    mir::MirModule* mirModule() const noexcept { return mirModule_; }
    cfg::CfgModule* cfgModule() const noexcept { return cfgModule_; }
    dominance::DominanceArena& dominanceArena() noexcept { return dominanceArena_; }
    dominance::DominanceModule* dominanceModule() const noexcept { return dominanceModule_; }
    dominance::DominanceBuilder* builder() const noexcept { return builder_.get(); }

private:
    sema::TypeContext types_;
    std::vector<std::unique_ptr<sema::Symbol>> symbols_;
    mir::MirArena mirArena_;
    cfg::CfgArena cfgArena_;
    dominance::DominanceArena dominanceArena_;
    mir::MirModule* mirModule_ = nullptr;
    cfg::CfgModule* cfgModule_ = nullptr;
    dominance::DominanceModule* dominanceModule_ = nullptr;
    std::unique_ptr<dominance::DominanceBuilder> builder_;
};

/** Creates an entry-only function. */
inline FunctionGraph makeSingle(GraphFixture& fixture, std::string name = "Single")
{
    return fixture.addFunction(std::move(name), 1);
}

/** Creates a linear function with the requested number of nodes. */
inline FunctionGraph makeLinear(GraphFixture& fixture, std::size_t count, std::string name = "Linear")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), count);
    for (std::size_t index = 1; index < count; ++index)
    {
        fixture.connect(graph, index - 1, index);
    }
    return graph;
}

/** Creates a four-node diamond in entry, left, right, join RPO order. */
inline FunctionGraph makeDiamond(GraphFixture& fixture, std::string name = "Diamond")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), 4);
    fixture.connect(graph, 0, 1, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 0, 2, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 1, 3);
    fixture.connect(graph, 2, 3);
    return graph;
}

/** Creates entry, loop-header, body, and exit with one back edge. */
inline FunctionGraph makeLoop(GraphFixture& fixture, std::string name = "Loop")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), 4);
    fixture.connect(graph, 0, 1);
    fixture.connect(graph, 1, 2, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 1, 3, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 2, 1);
    return graph;
}

/** Creates two joins so frontier ordering contains more than one candidate. */
inline FunctionGraph makeMultipleJoins(GraphFixture& fixture, std::string name = "MultipleJoins")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), 7);
    fixture.connect(graph, 0, 1, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 0, 2, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 1, 3);
    fixture.connect(graph, 2, 3);
    fixture.connect(graph, 3, 4, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 3, 5, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 4, 6);
    fixture.connect(graph, 5, 6);
    return graph;
}

/** Creates two sibling joins so one owner has a two-element ordered frontier. */
inline FunctionGraph makeTwinJoins(GraphFixture& fixture, std::string name = "TwinJoins")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), 6);
    fixture.connect(graph, 0, 1, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 0, 2, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 1, 3);
    fixture.connect(graph, 2, 3);
    fixture.connect(graph, 1, 4);
    fixture.connect(graph, 2, 4);
    fixture.connect(graph, 3, 5);
    fixture.connect(graph, 4, 5);
    return graph;
}

/** Creates a two-entry strongly connected region followed by an exit. */
inline FunctionGraph makeIrreducible(GraphFixture& fixture, std::string name = "Irreducible")
{
    FunctionGraph graph = fixture.addFunction(std::move(name), 4);
    fixture.connect(graph, 0, 1, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 0, 2, cfg::CfgEdgeKind::BranchFalse);
    fixture.connect(graph, 1, 2);
    fixture.connect(graph, 2, 1, cfg::CfgEdgeKind::BranchTrue);
    fixture.connect(graph, 2, 3, cfg::CfgEdgeKind::BranchFalse);
    return graph;
}

/** Returns the function analysis associated with the supplied CFG function. */
inline dominance::DominanceFunction* analysis(
    const GraphFixture& fixture,
    const FunctionGraph& graph)
{
    return fixture.dominanceModule() == nullptr ? nullptr :
        fixture.dominanceModule()->findFunction(graph.function);
}

/** Builds and resolves one function analysis while converting a missing producer into a test failure. */
inline dominance::DominanceFunction* buildAnalysis(
    TestContext& context,
    GraphFixture& fixture,
    const FunctionGraph& graph,
    std::string message)
{
    dominance::DominanceModule* module = fixture.build();
    dominance::DominanceFunction* function = analysis(fixture, graph);
    context.expect(module != nullptr && function != nullptr, std::move(message));
    return function;
}

/** Compares unordered node sets without depending on their hash iteration order. */
inline bool sameSet(const NodeSet& first, const NodeSet& second)
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

/** Computes mathematical dominator sets with an iterative fixed-point oracle. */
inline DominatorSets computeDominatorSets(const cfg::CfgFunction* function)
{
    DominatorSets sets;
    const std::vector<cfg::CfgNode*>& rpo = function->reversePostOrder();
    if (rpo.empty())
    {
        return sets;
    }
    const NodeSet universe(rpo.begin(), rpo.end());
    sets[rpo.front()] = {rpo.front()};
    for (std::size_t index = 1; index < rpo.size(); ++index)
    {
        sets[rpo[index]] = universe;
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 1; index < rpo.size(); ++index)
        {
            cfg::CfgNode* node = rpo[index];
            NodeSet next;
            bool initialized = false;
            for (cfg::CfgNode* predecessor : node->predecessors())
            {
                if (predecessor == nullptr || !predecessor->reachable() || !sets.contains(predecessor))
                {
                    continue;
                }
                if (!initialized)
                {
                    next = sets.at(predecessor);
                    initialized = true;
                    continue;
                }
                for (auto iterator = next.begin(); iterator != next.end();)
                {
                    iterator = sets.at(predecessor).contains(*iterator) ?
                        std::next(iterator) : next.erase(iterator);
                }
            }
            next.insert(node);
            if (!sameSet(next, sets.at(node)))
            {
                sets[node] = std::move(next);
                changed = true;
            }
        }
    }
    return sets;
}

/** Derives the closest strict dominator from independent sets. */
inline const cfg::CfgNode* expectedImmediateDominator(
    const cfg::CfgNode* node,
    const cfg::CfgNode* entry,
    const DominatorSets& sets)
{
    if (node == entry || !sets.contains(node))
    {
        return nullptr;
    }
    for (const cfg::CfgNode* candidate : sets.at(node))
    {
        if (candidate == node || !sets.contains(candidate))
        {
            continue;
        }
        bool nearest = true;
        for (const cfg::CfgNode* other : sets.at(node))
        {
            if (other != node && other != candidate && !sets.at(candidate).contains(other))
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

/** Finds the nearest common dominator by walking the independently derived idom chains. */
inline const cfg::CfgNode* expectedCommonDominator(
    const cfg::CfgNode* first,
    const cfg::CfgNode* second,
    const cfg::CfgNode* entry,
    const DominatorSets& sets)
{
    std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*> idoms;
    for (const auto& [node, ignored] : sets)
    {
        static_cast<void>(ignored);
        idoms[node] = expectedImmediateDominator(node, entry, sets);
    }
    const cfg::CfgNode* candidate = first;
    while (candidate != nullptr && !sets.at(second).contains(candidate))
    {
        candidate = idoms.at(candidate);
    }
    return candidate;
}

/** Computes the graph-theoretic frontier set independently from production storage. */
inline NodeSet expectedFrontier(
    const cfg::CfgFunction* function,
    const cfg::CfgNode* owner,
    const DominatorSets& sets)
{
    NodeSet result;
    for (const cfg::CfgNode* candidate : function->reversePostOrder())
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
            result.insert(candidate);
        }
    }
    return result;
}

/** Returns a stable structural signature without embedding allocator-dependent addresses. */
inline std::string structuralSignature(const dominance::DominanceFunction* function)
{
    std::ostringstream stream;
    const dominance::DominatorTree* tree = function->dominatorTree();
    for (const dominance::DominatorTreeNode* node : tree->nodes())
    {
        stream << node->reversePostOrderIndex() << ':' << node->depth() << ':'
               << node->preorderNumber() << ':' << node->postorderNumber() << ':';
        const dominance::DominatorTreeNode* parent = node->immediateDominator();
        stream << (parent == nullptr ? -1 : static_cast<int>(parent->reversePostOrderIndex())) << '|';
        for (const cfg::CfgNode* member : function->dominanceFrontier()->frontier(node->cfgNode()))
        {
            stream << tree->findNode(member)->reversePostOrderIndex() << ',';
        }
        stream << ';';
    }
    return stream.str();
}

/** Renders a dominance module through the public dumper. */
inline std::string dump(const dominance::DominanceModule* module)
{
    std::ostringstream stream;
    dominance::DominanceDumper{}.dump(module, stream);
    return stream.str();
}

/** Expects a product-built module to satisfy the independent verifier. */
inline bool verifies(dominance::DominanceModule* module, std::string* error = nullptr)
{
    dominance::DominanceVerifier verifier;
    const bool result = verifier.verify(module);
    if (error != nullptr)
    {
        *error = verifier.error();
    }
    return result;
}
}
