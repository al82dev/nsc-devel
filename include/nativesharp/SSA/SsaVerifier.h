#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::cfg
{
class CfgNode;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirInstruction;
class MirValue;
}

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::ssa
{
class SsaFunction;
class SsaModule;

class SsaVerifier final
{
public:
    bool verify(const SsaModule* module);
    const std::string& error() const noexcept;

private:
    struct Definition
    {
        const mir::MirBlock* block = nullptr;
        std::size_t instructionIndex = 0;
    };

    bool verifyFunction(const SsaFunction* function);
    bool verifyPromotionSet(const SsaFunction* function);
    bool verifyBlockMapping(const SsaFunction* function);
    bool collectDefinitions(const SsaFunction* function);
    bool verifyInstructions(const SsaFunction* function);
    bool verifyInstruction(
        const SsaFunction* function,
        const mir::MirBlock* block,
        const mir::MirInstruction* instruction,
        std::size_t instructionIndex);
    bool verifyPhi(
        const SsaFunction* function,
        const mir::MirBlock* block,
        const mir::MirInstruction* instruction);
    bool verifyUseDominance(
        const SsaFunction* function,
        const mir::MirValue* value,
        const mir::MirBlock* useBlock,
        std::size_t useInstructionIndex,
        bool edgeUse);
    bool verifyExpectedSymbolPhis(const SsaFunction* function);
    bool verifyVirtualMergeCounts(const SsaFunction* function);

    std::vector<const cfg::CfgNode*> expectedPhiNodes(
        const SsaFunction* function,
        const std::vector<const cfg::CfgNode*>& definitionNodes) const;
    std::vector<const cfg::CfgNode*> symbolDefinitionNodes(
        const SsaFunction* function,
        const sema::Symbol* symbol) const;
    std::size_t sourceMultiDefinedValueCount(const SsaFunction* function) const;
    std::size_t expectedVirtualPhiCount(const SsaFunction* function) const;

    const cfg::CfgNode* sourceCfgNode(
        const SsaFunction* function,
        const mir::MirBlock* outputBlock) const noexcept;
    bool isPromoted(const SsaFunction* function, const sema::Symbol* symbol) const noexcept;
    bool containsNode(
        const std::vector<const cfg::CfgNode*>& nodes,
        const cfg::CfgNode* node) const noexcept;
    bool fail(std::string message);

    std::unordered_map<const mir::MirValue*, Definition> definitions_;
    std::unordered_set<const mir::MirValue*> duplicateDefinitions_;
    std::string error_;
};
}
