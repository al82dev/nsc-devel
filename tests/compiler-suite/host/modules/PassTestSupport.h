#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/AnalysisManager.h"
#include "nativesharp/Passes/ConstantFolder.h"
#include "nativesharp/Passes/ConstantPropagationPass.h"
#include "nativesharp/Passes/CopyPropagationPass.h"
#include "nativesharp/Passes/DeadCodeEliminationPass.h"
#include "nativesharp/Passes/MirEffectClassifier.h"
#include "nativesharp/Passes/MirPass.h"
#include "nativesharp/Passes/MirRewriter.h"
#include "nativesharp/Passes/OptimizationArena.h"
#include "nativesharp/Passes/OptimizationBuilder.h"
#include "nativesharp/Passes/OptimizationDumper.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Passes/PassContext.h"
#include "nativesharp/Passes/PassManager.h"
#include "nativesharp/Passes/PassPipelineDumper.h"
#include "nativesharp/Passes/PhiSimplificationPass.h"
#include "nativesharp/Passes/SimplifyControlFlowPass.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/TypeContext.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nsc::suite::pass_support
{
using namespace nativesharp;

inline std::string caseId(std::size_t contract, std::size_t variant)
{
    std::ostringstream output;
    output << "TF-PAS-" << std::setw(3) << std::setfill('0') << ((contract - 1) * 8 + variant);
    return output.str();
}

inline std::string contractId(std::size_t contract)
{
    std::ostringstream output;
    output << "CF-PAS-" << std::setw(3) << std::setfill('0') << contract;
    return output.str();
}

inline void addPassContract(TestRegistry& registry, std::size_t contract, std::string layer,
    const std::function<void(TestContext&, std::size_t)>& function,
    std::vector<std::size_t> knownVariants = {}, std::vector<std::string> tags = {})
{
    for (std::size_t variant = 1; variant <= 8; ++variant)
    {
        std::vector<std::string> caseTags = tags;
        caseTags.push_back("passes");
        caseTags.push_back("variant-" + std::to_string(variant));
        const bool known = std::find(knownVariants.begin(), knownVariants.end(), variant) != knownVariants.end();
        if (known) caseTags.push_back("infrastructure-special");
        registry.add(TestCase{caseId(contract, variant), contractId(contract), "Host.Passes", layer,
            variant == 8 ? "NIGHTLY" : "PR", std::move(caseTags),
            [function, variant](TestContext& context) { function(context, variant); },
            known ? TestDisposition::InfrastructurePending : TestDisposition::Required});
    }
}

class PassFixture final
{
public:
    bool compile(std::size_t variant)
    {
        std::string source = "int Main() { int value = 1 + 2; return value; }";
        if (variant == 2 || variant == 6)
            source = "int Choose(bool flag) { if (flag) { return 7; } return 9; } int Main() { return Choose(true); }";
        if (variant == 7)
            source = "int Main() { int a = 1; int b = a; int c = b; return c; }";
        if (variant == 8)
            source = "int Main() { int total = 0; for (int i = 0; i < 64; i = i + 1) { total = total + i; } return total; }";
        return compilation_.compileBuffer(std::filesystem::path("pass-fixture.ns"), std::move(source), false);
    }

    passes::OptimizationModule* build()
    {
        builder_ = std::make_unique<passes::OptimizationBuilder>(arena_);
        module_ = builder_->build(compilation_.ssaModule());
        return module_;
    }

    frontend::FrontendCompilation& compilation() noexcept { return compilation_; }
    passes::OptimizationArena& arena() noexcept { return arena_; }
    passes::OptimizationModule* module() const noexcept { return module_; }
    passes::OptimizationBuilder* builder() const noexcept { return builder_.get(); }

private:
    frontend::FrontendCompilation compilation_;
    passes::OptimizationArena arena_;
    passes::OptimizationModule* module_ = nullptr;
    std::unique_ptr<passes::OptimizationBuilder> builder_;
};

inline std::unique_ptr<PassFixture> makeFixture(TestContext& context, std::size_t variant)
{
    auto fixture = std::make_unique<PassFixture>();
    context.expect(fixture->compile(variant), "Host.Passes fixture must reach verified SSA");
    context.expect(fixture->build() != nullptr, "Host.Passes fixture must clone verified SSA");
    return fixture;
}

class ScriptedPass final : public passes::MirPass
{
public:
    ScriptedPass(std::string name, passes::PassRequirements requirements,
        passes::PreservedAnalyses preserved, std::vector<passes::PassRunResult> results,
        bool fail = false)
        : name_(std::move(name)), requirements_(requirements), preserved_(preserved),
          results_(std::move(results)), fail_(fail) {}

    std::string_view name() const noexcept override { return name_; }
    passes::PassRequirements requirements() const noexcept override { return requirements_; }
    passes::PreservedAnalyses preservedAnalyses() const noexcept override { return preserved_; }
    passes::PassRunResult run(passes::PassContext&, std::string& error) override
    {
        ++calls_;
        if (fail_)
        {
            error = "scripted failure";
            return {};
        }
        error.clear();
        if (results_.empty()) return {};
        const std::size_t index = std::min(calls_ - 1, results_.size() - 1);
        return results_[index];
    }
    std::size_t calls() const noexcept { return calls_; }

private:
    std::string name_;
    passes::PassRequirements requirements_;
    passes::PreservedAnalyses preserved_;
    std::vector<passes::PassRunResult> results_;
    bool fail_ = false;
    std::size_t calls_ = 0;
};

inline std::string dumpModule(const passes::OptimizationModule* module)
{
    std::ostringstream output;
    passes::OptimizationDumper{}.dump(module, output);
    return output.str();
}

inline std::string dumpPipeline(const passes::OptimizationModule* module)
{
    std::ostringstream output;
    passes::PassPipelineDumper{}.dump(module, output);
    return output.str();
}
}
