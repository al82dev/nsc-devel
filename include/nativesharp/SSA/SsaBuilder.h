#pragma once

#include "nativesharp/SSA/SsaArena.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
class DominatorTreeNode;
}

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirGlobal;
class MirInstruction;
class MirModule;
class MirValue;
}

namespace nativesharp::sema
{
class Symbol;
class Type;
}

namespace nativesharp::ssa
{
class SsaFunction;
class SsaModule;

class SsaBuilder final
{
public:
    explicit SsaBuilder(SsaArena& arena);

    SsaModule* build(
        const mir::MirModule* sourceModule,
        const cfg::CfgModule* cfgModule,
        const dominance::DominanceModule* dominanceModule);

    bool succeeded() const noexcept;
    const std::string& error() const noexcept;

private:
    struct PhiSpec
    {
        sema::Symbol* symbol = nullptr;
        const mir::MirValue* sourceValue = nullptr;
        sema::Type* type = nullptr;
    };

    struct RenameFrame
    {
        dominance::DominatorTreeNode* node = nullptr;
        std::size_t nextChild = 0;
        bool entered = false;
        std::vector<const sema::Symbol*> symbolPushes;
        std::vector<const mir::MirValue*> valuePushes;
    };

    bool cloneGlobal(const mir::MirGlobal* sourceGlobal, mir::MirModule* outputModule);
    bool clonePrototype(const mir::MirFunction* sourceFunction, mir::MirModule* outputModule);
    bool buildFunction(
        const mir::MirFunction* sourceFunction,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction,
        mir::MirModule* outputModule,
        SsaModule* ssaModule);

    void resetFunctionState();
    bool classifyVariables();
    bool createOutputBlocks();
    bool planPhiPlacement();
    bool planSymbolPhis(sema::Symbol* symbol);
    bool planValuePhis(const mir::MirValue* value);
    bool materializePlannedPhis();
    bool placePhiSpecs(
        const std::vector<const cfg::CfgNode*>& definitionNodes,
        PhiSpec spec);
    void collectMultiDefinedValues();
    bool renameFunction();
    bool enterBlock(RenameFrame& frame);
    void leaveBlock(RenameFrame& frame);
    bool emitPlannedPhis(const mir::MirBlock* sourceBlock, RenameFrame& frame);
    bool emitInitialUndefs(const mir::MirBlock* sourceBlock);
    bool cloneBlockInstructions(const mir::MirBlock* sourceBlock, RenameFrame& frame);
    bool cloneInstruction(const mir::MirInstruction* sourceInstruction, RenameFrame& frame);
    bool addPhiIncomingToSuccessors(const mir::MirBlock* sourceBlock);

    mir::MirValue* resolveValue(const mir::MirValue* sourceValue);
    mir::MirValue* currentSymbolValue(const sema::Symbol* symbol);
    mir::MirValue* currentVirtualValue(const mir::MirValue* value);
    mir::MirInstruction* cloneInstructionShell(
        const mir::MirInstruction* sourceInstruction,
        mir::MirBlock* outputBlock);
    mir::MirValue* makeResult(sema::Type* type);

    void pushSymbol(const sema::Symbol* symbol, mir::MirValue* value, RenameFrame* frame);
    void pushValue(const mir::MirValue* source, mir::MirValue* value, RenameFrame* frame);
    void popSymbol(const sema::Symbol* symbol);
    void popValue(const mir::MirValue* source);

    bool isReachableBlock(const mir::MirBlock* block) const noexcept;
    bool isPromotedSymbol(const sema::Symbol* symbol) const noexcept;
    bool isMultiDefinedValue(const mir::MirValue* value) const noexcept;
    bool addPhiIncomingOnce(
        mir::MirInstruction* phi,
        mir::MirBlock* predecessor,
        mir::MirValue* value);
    bool fail(std::string message);

    SsaArena& arena_;
    mir::MirArena* outputArena_ = nullptr;
    const mir::MirModule* sourceModule_ = nullptr;
    const cfg::CfgModule* cfgModule_ = nullptr;
    const dominance::DominanceModule* dominanceModule_ = nullptr;

    const mir::MirFunction* sourceFunction_ = nullptr;
    mir::MirFunction* outputFunction_ = nullptr;
    const cfg::CfgFunction* cfgFunction_ = nullptr;
    const dominance::DominanceFunction* dominanceFunction_ = nullptr;
    SsaFunction* ssaFunction_ = nullptr;

    std::unordered_set<const sema::Symbol*> promotedSymbols_;
    std::vector<sema::Symbol*> promotedSymbolOrder_;
    std::unordered_set<const mir::MirValue*> multiDefinedValues_;
    std::vector<const mir::MirValue*> multiDefinedValueOrder_;
    std::unordered_map<const mir::MirBlock*, std::vector<PhiSpec>> phiSpecs_;
    std::unordered_map<const mir::MirBlock*, std::vector<mir::MirInstruction*>> phiInstructions_;

    std::unordered_map<const sema::Symbol*, std::vector<mir::MirValue*>> symbolStacks_;
    std::unordered_map<const mir::MirValue*, std::vector<mir::MirValue*>> valueStacks_;
    std::unordered_map<const mir::MirValue*, mir::MirValue*> directValues_;
    std::vector<mir::MirValue*> outputParameters_;

    std::string error_;
};
}
