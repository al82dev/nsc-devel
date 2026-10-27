#include "nativesharp/LLVM/LlvmModuleInspection.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include "LlvmCApi.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
using llvm_api::LLVMBasicBlockRef;
using llvm_api::LLVMModuleRef;
using llvm_api::LLVMValueRef;

constexpr std::size_t kNoBlock = std::numeric_limits<std::size_t>::max();

// EN: The control-flow graph of one defined LLVM function, indexed by layout position.
// ES: El grafo de flujo de control de una funcion LLVM definida, indexado por posicion de disposicion.
struct FunctionGraph final
{
    LLVMValueRef function = nullptr;
    std::string name;
    std::vector<LLVMBasicBlockRef> blocks;
    std::unordered_map<LLVMBasicBlockRef, std::size_t> indices;
    std::vector<std::vector<std::size_t>> successors;
    std::vector<std::vector<std::size_t>> predecessors;
    std::vector<bool> terminated;
    std::vector<std::size_t> foreignSuccessors;
    std::vector<std::size_t> reversePostorder;
    std::vector<std::size_t> rpoNumber;
    std::vector<std::size_t> idom;
    std::vector<std::size_t> depth;
};

std::string valueName(LLVMValueRef value)
{
    std::size_t length = 0;
    const char* name = llvm_api::LLVMGetValueName2(value, &length);
    return name == nullptr ? std::string() : std::string(name, length);
}

std::string blockLabel(const FunctionGraph& graph, std::size_t index)
{
    std::string label = "bb" + std::to_string(index);
    const char* name = llvm_api::LLVMGetBasicBlockName(graph.blocks[index]);
    if (name != nullptr && *name != '\0') label += " \"" + std::string(name) + "\"";
    return label;
}

std::string blockList(const std::vector<std::size_t>& indices)
{
    std::string text = "[";
    for (std::size_t position = 0; position < indices.size(); ++position)
    {
        if (position != 0) text += ", ";
        text += "bb" + std::to_string(indices[position]);
    }
    return text + "]";
}

// EN: Reverse postorder of the blocks reachable from the entry, with an explicit stack.
// ES: Postorden inverso de los bloques alcanzables desde la entrada, con pila explicita.
void computeReversePostorder(FunctionGraph& graph)
{
    const std::size_t count = graph.blocks.size();
    graph.rpoNumber.assign(count, kNoBlock);
    graph.reversePostorder.clear();
    if (count == 0) return;
    std::vector<bool> visited(count, false);
    std::vector<std::pair<std::size_t, std::size_t>> stack = {{0, 0}};
    visited[0] = true;
    std::vector<std::size_t> postorder;
    while (!stack.empty())
    {
        auto& [block, next] = stack.back();
        if (next < graph.successors[block].size())
        {
            const std::size_t successor = graph.successors[block][next++];
            if (!visited[successor])
            {
                visited[successor] = true;
                stack.emplace_back(successor, 0);
            }
            continue;
        }
        postorder.push_back(block);
        stack.pop_back();
    }
    graph.reversePostorder.assign(postorder.rbegin(), postorder.rend());
    for (std::size_t position = 0; position < graph.reversePostorder.size(); ++position)
        graph.rpoNumber[graph.reversePostorder[position]] = position;
}

bool buildGraph(LLVMValueRef function, FunctionGraph& graph)
{
    graph.function = function;
    graph.name = valueName(function);
    for (LLVMBasicBlockRef block = llvm_api::LLVMGetFirstBasicBlock(function); block != nullptr;
         block = llvm_api::LLVMGetNextBasicBlock(block))
    {
        graph.indices.emplace(block, graph.blocks.size());
        graph.blocks.push_back(block);
    }
    const std::size_t count = graph.blocks.size();
    graph.successors.assign(count, {});
    graph.predecessors.assign(count, {});
    graph.terminated.assign(count, false);
    graph.foreignSuccessors.assign(count, 0);
    for (std::size_t index = 0; index < count; ++index)
    {
        const LLVMValueRef terminator = llvm_api::LLVMGetBasicBlockTerminator(graph.blocks[index]);
        if (terminator == nullptr) continue;
        graph.terminated[index] = true;
        const unsigned successorCount = llvm_api::LLVMGetNumSuccessors(terminator);
        for (unsigned position = 0; position < successorCount; ++position)
        {
            const auto target = graph.indices.find(llvm_api::LLVMGetSuccessor(terminator, position));
            if (target == graph.indices.end())
            {
                ++graph.foreignSuccessors[index];
                continue;
            }
            std::vector<std::size_t>& successors = graph.successors[index];
            if (std::find(successors.begin(), successors.end(), target->second) != successors.end()) continue;
            successors.push_back(target->second);
            graph.predecessors[target->second].push_back(index);
        }
    }
    for (std::vector<std::size_t>& predecessors : graph.predecessors)
        std::sort(predecessors.begin(), predecessors.end());
    computeReversePostorder(graph);
    return count != 0;
}

// EN: Immediate dominators (Cooper, Harvey and Kennedy) and depths over the reachable blocks.
// ES: Dominadores inmediatos (Cooper, Harvey y Kennedy) y profundidades sobre los bloques alcanzables.
void computeDominators(FunctionGraph& graph)
{
    const std::size_t count = graph.blocks.size();
    graph.idom.assign(count, kNoBlock);
    graph.depth.assign(count, 0);
    if (count == 0) return;
    graph.idom[0] = 0;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const std::size_t block : graph.reversePostorder)
        {
            if (block == 0) continue;
            std::size_t candidate = kNoBlock;
            for (const std::size_t predecessor : graph.predecessors[block])
            {
                if (graph.idom[predecessor] == kNoBlock) continue;
                if (candidate == kNoBlock)
                {
                    candidate = predecessor;
                    continue;
                }
                std::size_t left = predecessor;
                std::size_t right = candidate;
                while (left != right)
                {
                    while (graph.rpoNumber[left] > graph.rpoNumber[right]) left = graph.idom[left];
                    while (graph.rpoNumber[right] > graph.rpoNumber[left]) right = graph.idom[right];
                }
                candidate = left;
            }
            if (candidate != kNoBlock && graph.idom[block] != candidate)
            {
                graph.idom[block] = candidate;
                changed = true;
            }
        }
    }
    for (const std::size_t block : graph.reversePostorder)
        graph.depth[block] = block == 0 ? 0 : graph.depth[graph.idom[block]] + 1;
}

bool reachable(const FunctionGraph& graph, std::size_t block)
{
    return graph.rpoNumber[block] != kNoBlock;
}

// EN: True when `dominator` dominates `block` (both reachable); walks the dominator tree upwards.
// ES: Verdadero si `dominator` domina a `block` (ambos alcanzables); sube por el arbol.
bool dominates(const FunctionGraph& graph, std::size_t dominator, std::size_t block)
{
    while (graph.depth[block] > graph.depth[dominator]) block = graph.idom[block];
    return block == dominator;
}

std::size_t nearestCommonDominator(const FunctionGraph& graph, std::size_t left, std::size_t right)
{
    while (graph.depth[left] > graph.depth[right]) left = graph.idom[left];
    while (graph.depth[right] > graph.depth[left]) right = graph.idom[right];
    while (left != right)
    {
        left = graph.idom[left];
        right = graph.idom[right];
    }
    return left;
}

std::vector<std::vector<std::size_t>> dominanceFrontiers(const FunctionGraph& graph)
{
    std::vector<std::set<std::size_t>> frontiers(graph.blocks.size());
    for (const std::size_t block : graph.reversePostorder)
    {
        std::vector<std::size_t> predecessors;
        for (const std::size_t predecessor : graph.predecessors[block])
            if (reachable(graph, predecessor)) predecessors.push_back(predecessor);
        if (predecessors.size() < 2) continue;
        for (std::size_t runner : predecessors)
        {
            while (runner != graph.idom[block])
            {
                frontiers[runner].insert(block);
                if (runner == 0) break;
                runner = graph.idom[runner];
            }
        }
    }
    std::vector<std::vector<std::size_t>> result;
    for (const std::set<std::size_t>& frontier : frontiers) result.emplace_back(frontier.begin(), frontier.end());
    return result;
}

LLVMModuleRef moduleHandle(void* module, std::string& error)
{
    if (!verifyLoadedLlvmVersion(error)) return nullptr;
    if (module == nullptr) error = "LLVM module is null";
    return static_cast<LLVMModuleRef>(module);
}

// EN: Builds the graph of every defined function of `module`, in module order.
// ES: Construye el grafo de cada funcion definida de `module`, en orden del modulo.
std::vector<FunctionGraph> functionGraphs(LLVMModuleRef module, bool withDominators)
{
    std::vector<FunctionGraph> graphs;
    for (LLVMValueRef function = llvm_api::LLVMGetFirstFunction(module); function != nullptr;
         function = llvm_api::LLVMGetNextFunction(function))
    {
        if (llvm_api::LLVMIsDeclaration(function)) continue;
        FunctionGraph graph;
        if (!buildGraph(function, graph)) continue;
        if (withDominators) computeDominators(graph);
        graphs.push_back(std::move(graph));
    }
    return graphs;
}

std::string llvmMessage(char* message)
{
    if (message == nullptr) return {};
    std::string text(message);
    llvm_api::LLVMDisposeMessage(message);
    return text;
}

// EN: A private copy of `module` in SSA form: mem2reg promotes every promotable alloca. The caller
//     disposes of the copy with LLVMDisposeModule.
// ES: Una copia privada de `module` en forma SSA: mem2reg promueve todo alloca promovible. El
//     llamador libera la copia con LLVMDisposeModule.
LLVMModuleRef ssaCopy(LLVMModuleRef module, std::string& error)
{
    LLVMModuleRef copy = llvm_api::LLVMCloneModule(module);
    if (copy == nullptr)
    {
        error = "LLVMCloneModule failed";
        return nullptr;
    }
    llvm_api::LLVMPassBuilderOptionsRef options = llvm_api::LLVMCreatePassBuilderOptions();
    llvm_api::LLVMErrorRef failure = llvm_api::LLVMRunPasses(copy, "mem2reg", nullptr, options);
    llvm_api::LLVMDisposePassBuilderOptions(options);
    if (failure == nullptr) return copy;
    char* message = llvm_api::LLVMGetErrorMessage(failure);
    error = "mem2reg failed: " + std::string(message == nullptr ? "" : message);
    if (message != nullptr) llvm_api::LLVMDisposeErrorMessage(message);
    llvm_api::LLVMDisposeModule(copy);
    return nullptr;
}

struct SsaCounts final
{
    std::size_t phis = 0;
    std::size_t allocas = 0;
};

SsaCounts countSsa(const FunctionGraph& graph)
{
    SsaCounts counts;
    for (const LLVMBasicBlockRef block : graph.blocks)
    {
        for (LLVMValueRef instruction = llvm_api::LLVMGetFirstInstruction(block); instruction != nullptr;
             instruction = llvm_api::LLVMGetNextInstruction(instruction))
        {
            if (llvm_api::LLVMIsAPHINode(instruction) != nullptr) ++counts.phis;
            if (llvm_api::LLVMIsAAllocaInst(instruction) != nullptr) ++counts.allocas;
        }
    }
    return counts;
}

// EN: Position of every instruction of `graph`: its block and its index inside the block.
// ES: Posicion de cada instruccion de `graph`: su bloque y su indice dentro del bloque.
std::unordered_map<LLVMValueRef, std::pair<std::size_t, std::size_t>> instructionPositions(
    const FunctionGraph& graph)
{
    std::unordered_map<LLVMValueRef, std::pair<std::size_t, std::size_t>> positions;
    for (std::size_t block = 0; block < graph.blocks.size(); ++block)
    {
        std::size_t index = 0;
        for (LLVMValueRef instruction = llvm_api::LLVMGetFirstInstruction(graph.blocks[block]);
             instruction != nullptr; instruction = llvm_api::LLVMGetNextInstruction(instruction))
            positions.emplace(instruction, std::make_pair(block, index++));
    }
    return positions;
}

// EN: Checks the phis of one reachable block: grouped at its start and with one incoming block per
//     distinct predecessor; their incoming values must dominate the end of the incoming block.
// ES: Comprueba los phis de un bloque alcanzable: agrupados al inicio y con un bloque entrante por
//     cada predecesor distinto; sus valores entrantes deben dominar el final del bloque entrante.
bool verifyBlockSsa(
    const FunctionGraph& graph,
    std::size_t block,
    const std::unordered_map<LLVMValueRef, std::pair<std::size_t, std::size_t>>& positions,
    std::string& error)
{
    const std::string where = "@" + graph.name + " " + blockLabel(graph, block);
    bool phiRegion = true;
    std::size_t index = 0;
    for (LLVMValueRef instruction = llvm_api::LLVMGetFirstInstruction(graph.blocks[block]);
         instruction != nullptr; instruction = llvm_api::LLVMGetNextInstruction(instruction), ++index)
    {
        const bool phi = llvm_api::LLVMIsAPHINode(instruction) != nullptr;
        if (phi && !phiRegion)
        {
            error = where + ": a phi follows a non-phi instruction";
            return false;
        }
        if (!phi)
        {
            phiRegion = false;
            const int operandCount = llvm_api::LLVMGetNumOperands(instruction);
            for (int operand = 0; operand < operandCount; ++operand)
            {
                const auto definition =
                    positions.find(llvm_api::LLVMGetOperand(instruction, static_cast<unsigned>(operand)));
                if (definition == positions.end()) continue;
                const auto [definitionBlock, definitionIndex] = definition->second;
                const bool valid = definitionBlock == block
                    ? definitionIndex < index
                    : reachable(graph, definitionBlock) && definitionBlock != block &&
                        dominates(graph, definitionBlock, block);
                if (valid) continue;
                error = where + ": instruction " + std::to_string(index) + " uses a value whose definition "
                    "does not dominate it";
                return false;
            }
            continue;
        }
        std::set<std::size_t> incoming;
        const unsigned incomingCount = llvm_api::LLVMCountIncoming(instruction);
        for (unsigned position = 0; position < incomingCount; ++position)
        {
            const auto source = graph.indices.find(llvm_api::LLVMGetIncomingBlock(instruction, position));
            if (source == graph.indices.end())
            {
                error = where + ": a phi names an incoming block outside the function";
                return false;
            }
            incoming.insert(source->second);
            if (!reachable(graph, source->second)) continue;
            const auto definition = positions.find(llvm_api::LLVMGetIncomingValue(instruction, position));
            if (definition == positions.end()) continue;
            const std::size_t definitionBlock = definition->second.first;
            if (reachable(graph, definitionBlock) && dominates(graph, definitionBlock, source->second)) continue;
            error = where + ": a phi value does not dominate the end of incoming " +
                blockLabel(graph, source->second);
            return false;
        }
        const std::set<std::size_t> predecessors(graph.predecessors[block].begin(), graph.predecessors[block].end());
        if (incoming != predecessors)
        {
            error = where + ": phi incoming blocks " + blockList({incoming.begin(), incoming.end()}) +
                " differ from predecessors " + blockList(graph.predecessors[block]);
            return false;
        }
    }
    return true;
}
}

void* LlvmModuleInspector::moduleOf(const LlvmModule& module) noexcept
{
    return module.module_;
}

bool LlvmModuleInspector::dumpControlFlow(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    output << "cfg.module " << kLlvmCfgFormat << '\n';
    for (const FunctionGraph& graph : functionGraphs(handle, false))
    {
        std::size_t edges = 0;
        for (const std::vector<std::size_t>& successors : graph.successors) edges += successors.size();
        output << "function @" << graph.name << " blocks=" << graph.blocks.size() << " edges=" << edges << '\n'
               << "  rpo: " << blockList(graph.reversePostorder) << '\n';
        for (std::size_t block = 0; block < graph.blocks.size(); ++block)
        {
            output << "  " << blockLabel(graph, block) << " preds=" << blockList(graph.predecessors[block])
                   << " succs=" << blockList(graph.successors[block]);
            if (block == 0) output << " [entry]";
            if (!reachable(graph, block)) output << " [unreachable]";
            output << '\n';
        }
    }
    return true;
}

bool LlvmModuleInspector::verifyControlFlow(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    const std::vector<FunctionGraph> graphs = functionGraphs(handle, false);
    std::size_t blocks = 0;
    std::size_t edges = 0;
    std::size_t unreachableBlocks = 0;
    for (const FunctionGraph& graph : graphs)
    {
        if (!graph.predecessors[0].empty())
        {
            error = "@" + graph.name + ": the entry block has predecessors";
            return false;
        }
        for (std::size_t block = 0; block < graph.blocks.size(); ++block)
        {
            const std::string where = "@" + graph.name + " " + blockLabel(graph, block);
            if (!graph.terminated[block])
            {
                error = where + ": the block does not end in a terminator";
                return false;
            }
            if (graph.foreignSuccessors[block] != 0)
            {
                error = where + ": an edge leaves the function";
                return false;
            }
            edges += graph.successors[block].size();
            if (!reachable(graph, block)) ++unreachableBlocks;
        }
        blocks += graph.blocks.size();
    }
    output << "CFG verification: ok\n"
           << "format: " << kLlvmCfgFormat << '\n'
           << "functions: " << graphs.size() << '\n'
           << "blocks: " << blocks << '\n'
           << "edges: " << edges << '\n'
           << "unreachable: " << unreachableBlocks << '\n';
    return true;
}

bool LlvmModuleInspector::dumpDominance(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    output << "dominance.module " << kLlvmDominanceFormat << '\n';
    for (const FunctionGraph& graph : functionGraphs(handle, true))
    {
        const std::vector<std::vector<std::size_t>> frontiers = dominanceFrontiers(graph);
        output << "function @" << graph.name << " blocks=" << graph.blocks.size()
               << " reachable=" << graph.reversePostorder.size() << '\n';
        for (std::size_t block = 0; block < graph.blocks.size(); ++block)
        {
            output << "  " << blockLabel(graph, block);
            if (!reachable(graph, block))
            {
                output << " [unreachable]\n";
                continue;
            }
            output << " idom=" << (block == 0 ? std::string("-") : "bb" + std::to_string(graph.idom[block]))
                   << " depth=" << graph.depth[block] << " frontier=" << blockList(frontiers[block]) << '\n';
        }
    }
    return true;
}

bool LlvmModuleInspector::verifyDominance(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    const std::vector<FunctionGraph> graphs = functionGraphs(handle, true);
    std::size_t reachableBlocks = 0;
    std::size_t unreachableBlocks = 0;
    for (const FunctionGraph& graph : graphs)
    {
        for (const std::size_t block : graph.reversePostorder)
        {
            ++reachableBlocks;
            if (block == 0) continue;
            const std::string where = "@" + graph.name + " " + blockLabel(graph, block);
            const std::size_t dominator = graph.idom[block];
            if (dominator == kNoBlock || dominator == block || !reachable(graph, dominator) ||
                graph.rpoNumber[dominator] >= graph.rpoNumber[block] ||
                graph.depth[block] != graph.depth[dominator] + 1)
            {
                error = where + ": the immediate dominator is not a reachable block earlier in reverse postorder";
                return false;
            }
            std::size_t common = kNoBlock;
            for (const std::size_t predecessor : graph.predecessors[block])
            {
                if (!reachable(graph, predecessor)) continue;
                common = common == kNoBlock ? predecessor : nearestCommonDominator(graph, common, predecessor);
            }
            if (common != dominator)
            {
                error = where + ": the immediate dominator is not the nearest common dominator of the "
                    "predecessors";
                return false;
            }
        }
        unreachableBlocks += graph.blocks.size() - graph.reversePostorder.size();
    }
    output << "Dominance verification: ok\n"
           << "format: " << kLlvmDominanceFormat << '\n'
           << "functions: " << graphs.size() << '\n'
           << "reachable: " << reachableBlocks << '\n'
           << "unreachable: " << unreachableBlocks << '\n';
    return true;
}

bool LlvmModuleInspector::dumpSsa(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    const LLVMModuleRef copy = ssaCopy(handle, error);
    if (copy == nullptr) return false;
    output << "; ssa.module " << kLlvmSsaFormat << '\n'
           << "; unoptimized LLVM of the implicit project after mem2reg on a copy\n";
    for (const FunctionGraph& graph : functionGraphs(copy, false))
    {
        const SsaCounts counts = countSsa(graph);
        output << "; function @" << graph.name << " phis=" << counts.phis << " allocas=" << counts.allocas
               << '\n';
    }
    output << llvmMessage(llvm_api::LLVMPrintModuleToString(copy));
    llvm_api::LLVMDisposeModule(copy);
    return true;
}

bool LlvmModuleInspector::verifySsa(const LlvmModule& module, std::ostream& output, std::string& error)
{
    const LLVMModuleRef handle = moduleHandle(moduleOf(module), error);
    if (handle == nullptr) return false;
    const LLVMModuleRef copy = ssaCopy(handle, error);
    if (copy == nullptr) return false;
    char* message = nullptr;
    const bool broken = llvm_api::LLVMVerifyModule(copy, llvm_api::LLVMReturnStatusAction, &message) != 0;
    const std::string verifierText = llvmMessage(message);
    if (broken)
    {
        llvm_api::LLVMDisposeModule(copy);
        error = "LLVM verifier rejected the SSA form: " + verifierText;
        return false;
    }
    const std::vector<FunctionGraph> graphs = functionGraphs(copy, true);
    SsaCounts totals;
    bool valid = true;
    for (const FunctionGraph& graph : graphs)
    {
        const auto positions = instructionPositions(graph);
        for (const std::size_t block : graph.reversePostorder)
        {
            valid = verifyBlockSsa(graph, block, positions, error);
            if (!valid) break;
        }
        if (!valid) break;
        const SsaCounts counts = countSsa(graph);
        totals.phis += counts.phis;
        totals.allocas += counts.allocas;
    }
    llvm_api::LLVMDisposeModule(copy);
    if (!valid) return false;
    output << "SSA verification: ok\n"
           << "format: " << kLlvmSsaFormat << '\n'
           << "functions: " << graphs.size() << '\n'
           << "phis: " << totals.phis << '\n'
           << "allocas: " << totals.allocas << '\n';
    return true;
}
}
