#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgDumper.h"
#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgTraversal.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <iomanip>
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::cfg_support
{
using namespace nativesharp;

inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1) * 8 + variant;
    std::ostringstream stream;
    stream << "TF-CFG-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-CFG-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

inline void addCfgContract(
    TestRegistry& registry,
    std::size_t contractOrdinal,
    std::string layer,
    const std::function<void(TestContext&, std::size_t)>& function,
    std::vector<std::size_t> knownVariants = {},
    std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("cfg");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool known = std::find(knownVariants.begin(), knownVariants.end(), variant) != knownVariants.end();
        if (known) caseTags.push_back("known-limitation");
        registry.add(TestCase{caseId(contractOrdinal, variant), contractId(contractOrdinal),
            "Host.CFG", layer, variant == 8 ? "NIGHTLY" : "PR", std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            known ? TestDisposition::KnownProductDefect : TestDisposition::Required});
    }
}

class GraphFixture final
{
public:
    GraphFixture()
        : functionSymbol_(sema::SymbolKind::Function, "Main",
              types_.functionType(types_.builtin(sema::TypeKind::Int), {}), nullptr)
    {
        module_ = mirArena_.makeModule();
        function_ = mirArena_.makeFunction(&functionSymbol_);
        function_->setDefined(true);
        module_->addFunction(function_);
    }

    mir::MirBlock* addReturnBlock(std::string name)
    {
        mir::MirBlock* block = mirArena_.makeBlock(std::move(name));
        block->addInstruction(mirArena_.makeInstruction(mir::MirOpcode::Return, {}));
        function_->addBlock(block);
        return block;
    }

    mir::MirBlock* addUnreachableBlock(std::string name)
    {
        mir::MirBlock* block = mirArena_.makeBlock(std::move(name));
        block->addInstruction(mirArena_.makeInstruction(mir::MirOpcode::Unreachable, {}));
        function_->addBlock(block);
        return block;
    }

    mir::MirBlock* addJumpBlock(std::string name, mir::MirBlock* target)
    {
        mir::MirBlock* block = mirArena_.makeBlock(std::move(name));
        mir::MirInstruction* jump = mirArena_.makeInstruction(mir::MirOpcode::Jump, {});
        jump->setFirstTarget(target);
        block->addInstruction(jump);
        function_->addBlock(block);
        return block;
    }

    mir::MirBlock* addBranchBlock(std::string name, mir::MirBlock* whenTrue, mir::MirBlock* whenFalse)
    {
        mir::MirBlock* block = mirArena_.makeBlock(std::move(name));
        mir::MirInstruction* branch = mirArena_.makeInstruction(mir::MirOpcode::Branch, {});
        branch->setFirstTarget(whenTrue);
        branch->setSecondTarget(whenFalse);
        block->addInstruction(branch);
        function_->addBlock(block);
        return block;
    }

    cfg::CfgModule* build()
    {
        builder_ = std::make_unique<cfg::CfgBuilder>(cfgArena_);
        cfgModule_ = builder_->build(module_);
        return cfgModule_;
    }

    cfg::CfgModule* makeManualModule() { return cfgArena_.makeModule(module_); }
    cfg::CfgFunction* makeManualFunction() { return cfgArena_.makeFunction(function_); }
    cfg::CfgNode* makeNode(const mir::MirBlock* block) { return cfgArena_.makeNode(block); }
    cfg::CfgEdge* makeEdge(cfg::CfgNode* source, cfg::CfgNode* target, cfg::CfgEdgeKind kind)
    {
        return cfgArena_.makeEdge(source, target, kind);
    }

    mir::MirArena& mirArena() noexcept { return mirArena_; }
    cfg::CfgArena& cfgArena() noexcept { return cfgArena_; }
    mir::MirModule* module() const noexcept { return module_; }
    mir::MirFunction* function() const noexcept { return function_; }
    cfg::CfgModule* cfgModule() const noexcept { return cfgModule_; }
    cfg::CfgBuilder* builder() const noexcept { return builder_.get(); }

private:
    sema::TypeContext types_;
    sema::Symbol functionSymbol_;
    mir::MirArena mirArena_;
    cfg::CfgArena cfgArena_;
    mir::MirModule* module_ = nullptr;
    mir::MirFunction* function_ = nullptr;
    cfg::CfgModule* cfgModule_ = nullptr;
    std::unique_ptr<cfg::CfgBuilder> builder_;
};

inline std::unique_ptr<GraphFixture> makeLinearGraph()
{
    auto fixture = std::make_unique<GraphFixture>();
    mir::MirBlock* exit = fixture->addReturnBlock("exit");
    mir::MirBlock* entry = fixture->addJumpBlock("entry", exit);
    fixture->function()->replaceBlocks({entry, exit});
    fixture->build();
    return fixture;
}

inline std::unique_ptr<GraphFixture> makeBranchGraph()
{
    auto fixture = std::make_unique<GraphFixture>();
    mir::MirBlock* left = fixture->addReturnBlock("left");
    mir::MirBlock* right = fixture->addReturnBlock("right");
    mir::MirBlock* entry = fixture->addBranchBlock("entry", left, right);
    fixture->function()->replaceBlocks({entry, left, right});
    fixture->build();
    return fixture;
}

inline std::string dump(const cfg::CfgModule* module)
{
    std::ostringstream output;
    cfg::CfgDumper{}.dump(module, output);
    return output.str();
}
}
