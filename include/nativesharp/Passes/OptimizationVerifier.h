#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace nativesharp::cfg
{
class CfgFunction;
class CfgModule;
class CfgNode;
}

namespace nativesharp::dominance
{
class DominanceFunction;
class DominanceModule;
}

namespace nativesharp::mir
{
class MirBlock;
class MirInstruction;
class MirValue;
}

namespace nativesharp::passes
{
class AnalysisManager;
class OptimizationModule;

class OptimizationVerifier final
{
public:
    bool verify(
        OptimizationModule* module,
        AnalysisManager& analyses);
    bool verify(const OptimizationModule* module);

    const std::string& error() const noexcept;

private:
    bool verifyWithAnalyses(
        const OptimizationModule* module,
        const cfg::CfgModule* cfgModule,
        const dominance::DominanceModule* dominanceModule);

    struct Definition
    {
        const mir::MirBlock* block = nullptr;
        std::size_t instructionIndex = 0;
    };

    bool verifyFunction(
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

    bool collectDefinitions(const cfg::CfgFunction* cfgFunction);
    bool verifyUses(
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

    bool verifyOrdinaryUse(
        const mir::MirValue* value,
        const mir::MirBlock* useBlock,
        std::size_t useIndex,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

    bool verifyPhi(
        const mir::MirInstruction* instruction,
        const mir::MirBlock* block,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

    bool definitionDominatesBlock(
        const Definition& definition,
        const mir::MirBlock* block,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction) const noexcept;

    bool fail(std::string message);

    std::unordered_map<const mir::MirValue*, Definition> definitions_;
    std::string error_;
};
}
