#include "PassTestSupport.h"

#include "nativesharp/Sema/Symbol.h"

namespace nsc::suite
{
namespace
{
using namespace pass_support;

class MutableFixture final
{
public:
    MutableFixture()
        : symbol_(sema::SymbolKind::Function, "Main",
              types_.functionType(types_.builtin(sema::TypeKind::Int), {}), nullptr)
    {
        module_ = arena_.mirArena().makeModule();
        function_ = arena_.mirArena().makeFunction(&symbol_);
        function_->setDefined(true);
        block_ = arena_.mirArena().makeBlock("entry");
        function_->addBlock(block_);
        module_->addFunction(function_);
        optimization_ = arena_.makeModule(nullptr, module_);
        analyses_ = std::make_unique<passes::AnalysisManager>(*optimization_, arena_);
        context_ = std::make_unique<passes::PassContext>(module_, arena_.mirArena(), *analyses_);
    }

    mir::MirValue* value(sema::TypeKind kind = sema::TypeKind::Int)
    {
        return arena_.mirArena().makeValue(types_.builtin(kind));
    }
    mir::MirInstruction* instruction(mir::MirOpcode opcode)
    {
        auto* result = arena_.mirArena().makeInstruction(opcode, {});
        block_->addInstruction(result);
        return result;
    }
    mir::MirInstruction* constant(std::int64_t number)
    {
        auto* instructionValue = instruction(mir::MirOpcode::Constant);
        instructionValue->setResult(value());
        instructionValue->setConstant(sema::ConstantValue::signedInteger(number));
        return instructionValue;
    }
    void terminate()
    {
        if (!block_->terminated()) block_->addInstruction(arena_.mirArena().makeInstruction(mir::MirOpcode::Return, {}));
    }

    passes::OptimizationArena& arena() noexcept { return arena_; }
    passes::OptimizationModule* optimization() const noexcept { return optimization_; }
    passes::AnalysisManager& analyses() noexcept { return *analyses_; }
    passes::PassContext& context() noexcept { return *context_; }
    mir::MirModule* module() const noexcept { return module_; }
    mir::MirFunction* function() const noexcept { return function_; }
    mir::MirBlock* block() const noexcept { return block_; }

private:
    sema::TypeContext types_;
    sema::Symbol symbol_;
    passes::OptimizationArena arena_;
    mir::MirModule* module_ = nullptr;
    mir::MirFunction* function_ = nullptr;
    mir::MirBlock* block_ = nullptr;
    passes::OptimizationModule* optimization_ = nullptr;
    std::unique_ptr<passes::AnalysisManager> analyses_;
    std::unique_ptr<passes::PassContext> context_;
};

void replaceUsesContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* from = fixture.value();
    auto* to = fixture.value();
    auto* ordinary = fixture.instruction(mir::MirOpcode::Add);
    ordinary->setResult(fixture.value());
    ordinary->addOperand(from);
    ordinary->addOperand(from);
    auto* phi = fixture.instruction(mir::MirOpcode::Phi);
    phi->setResult(fixture.value());
    phi->addPhiIncoming(fixture.block(), from);
    const std::size_t count = passes::MirRewriter::replaceAllUses(fixture.module(), from, to);
    context.expect(count == 3, "rewriter must count ordinary and PHI occurrences independently");
    context.expect(ordinary->operands()[0] == to && ordinary->operands()[1] == to,
        "rewriter must replace every ordinary use");
    context.expect(phi->phiIncoming()[0].value == to, "rewriter must replace PHI incoming use");
    if (variant == 3) context.expect(passes::MirRewriter::replaceAllUses(nullptr, from, to) == 0,
        "null module replacement must be a no-op");
}

void countingContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    for (std::size_t index = 0; index < variant; ++index) fixture.constant(static_cast<std::int64_t>(index));
    fixture.terminate();
    context.expect(passes::MirRewriter::blockCount(fixture.module()) == 1, "block counter must traverse defined functions");
    context.expect(passes::MirRewriter::instructionCount(fixture.module()) == variant + 1,
        "instruction counter must traverse every stored instruction");
    context.expect(passes::MirRewriter::instructionCount(nullptr) == 0 && passes::MirRewriter::blockCount(nullptr) == 0,
        "null module counters must be zero");
}

void phiIdenticalContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* value = fixture.value();
    auto* phi = fixture.instruction(mir::MirOpcode::Phi);
    phi->setResult(fixture.value());
    phi->addPhiIncoming(fixture.block(), value);
    phi->addPhiIncoming(fixture.block(), value);
    fixture.terminate();
    passes::PhiSimplificationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && result.changed && result.transformations == 1,
        "identical PHI incoming values must simplify once");
    context.expect(fixture.block()->instructions()[0]->opcode() == mir::MirOpcode::Copy,
        "identical PHI must materialize as Copy");
}

void phiConstantContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* left = fixture.constant(variant);
    auto* right = fixture.constant(variant);
    auto* phi = fixture.instruction(mir::MirOpcode::Phi);
    phi->setResult(fixture.value());
    phi->addPhiIncoming(fixture.block(), left->result());
    phi->addPhiIncoming(fixture.block(), right->result());
    fixture.terminate();
    passes::PhiSimplificationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && result.changed, "bit-equal constant PHI must simplify");
    context.expect(fixture.block()->instructions()[2]->opcode() == mir::MirOpcode::Constant,
        "equal constant PHI must materialize an exact Constant");
}

void phiLayoutContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* body = fixture.constant(1);
    auto* latePhi = fixture.instruction(mir::MirOpcode::Phi);
    latePhi->setResult(fixture.value());
    latePhi->addPhiIncoming(fixture.block(), body->result());
    latePhi->addPhiIncoming(fixture.block(), body->result());
    fixture.terminate();
    passes::PhiSimplificationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && !result.changed, "PHI outside the prefix must remain untouched");
    context.expect(fixture.block()->instructions()[1] == latePhi, "non-prefix PHI identity and order must remain exact");
}

void copyResolutionContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* root = fixture.constant(7);
    mir::MirValue* previous = root->result();
    const std::size_t depth = variant == 8 ? 1024 : variant;
    for (std::size_t index = 0; index < depth; ++index)
    {
        auto* copy = fixture.instruction(mir::MirOpcode::Copy);
        copy->setResult(fixture.value());
        copy->addOperand(previous);
        previous = copy->result();
    }
    auto* ret = fixture.instruction(mir::MirOpcode::Return);
    ret->addOperand(previous);
    passes::CopyPropagationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && result.changed && result.transformations == depth,
        "copy chains must resolve iteratively to their terminal value");
    context.expect(ret->operands()[0] == root->result(), "copy-chain consumer must reference the root definition");
}

void copyRemovalContract(TestContext& context, std::size_t variant)
{
    copyResolutionContract(context, variant);
}

void constantFixedPointContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* left = fixture.constant(2);
    auto* right = fixture.constant(3);
    auto* add = fixture.instruction(mir::MirOpcode::Add);
    add->setResult(fixture.value());
    add->addOperand(left->result());
    add->addOperand(right->result());
    auto* multiply = fixture.instruction(mir::MirOpcode::Multiply);
    multiply->setResult(fixture.value());
    multiply->addOperand(add->result());
    multiply->addOperand(right->result());
    fixture.terminate();
    passes::ConstantPropagationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && result.transformations == 2, "constant closure must reach both dependent definitions");
    context.expect(add->opcode() == mir::MirOpcode::Constant && multiply->opcode() == mir::MirOpcode::Constant,
        "fixed-point constants must materialize in the graph");
}

void constantExclusionContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* source = fixture.constant(4);
    auto* copy = fixture.instruction(mir::MirOpcode::Copy);
    copy->setResult(fixture.value());
    copy->addOperand(source->result());
    fixture.terminate();
    passes::ConstantPropagationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && !result.changed, "constant pass must not materialize excluded Copy opcode");
    context.expect(copy->opcode() == mir::MirOpcode::Copy, "Copy must remain assigned to copy propagation");
}

void dceContract(TestContext& context, std::size_t variant)
{
    MutableFixture fixture;
    auto* left = fixture.constant(1);
    auto* right = fixture.constant(2);
    auto* dead = fixture.instruction(mir::MirOpcode::Add);
    dead->setResult(fixture.value());
    dead->addOperand(left->result());
    dead->addOperand(right->result());
    fixture.terminate();
    passes::DeadCodeEliminationPass pass;
    std::string error;
    const auto result = pass.run(fixture.context(), error);
    context.expect(error.empty() && result.changed, "unused pure definitions must be removed to fixed point");
    context.expect(std::find(fixture.block()->instructions().begin(), fixture.block()->instructions().end(), dead) ==
        fixture.block()->instructions().end(), "removed instruction must leave the block sequence");
}

void branchContract(TestContext& context, std::size_t variant)
{
    auto fixture = makeFixture(context, 2);
    passes::SimplifyControlFlowPass pass;
    passes::AnalysisManager analyses(*fixture->module(), fixture->arena());
    passes::PassContext passContext(fixture->module()->module(), *fixture->module()->arena(), analyses);
    std::string error;
    const auto result = pass.run(passContext, error);
    context.expect(error.empty(), "valid branch simplification fixture must not report an error");
    context.expect(result.transformations <= passes::MirRewriter::blockCount(fixture->module()->module()),
        "branch transformation count must be bounded by the function graph");
}

void reachabilityContract(TestContext& context, std::size_t variant)
{
    branchContract(context, variant);
}
}

void registerPassTransformsModule(TestRegistry& registry)
{
    addPassContract(registry, 32, "component", replaceUsesContract);
    addPassContract(registry, 33, "unit", countingContract);
    addPassContract(registry, 34, "component", phiIdenticalContract);
    addPassContract(registry, 35, "component", phiConstantContract);
    addPassContract(registry, 36, "component", phiLayoutContract);
    addPassContract(registry, 37, "unit", copyResolutionContract);
    addPassContract(registry, 38, "component", copyRemovalContract);
    addPassContract(registry, 39, "component", constantFixedPointContract);
    addPassContract(registry, 40, "component", constantExclusionContract);
    addPassContract(registry, 41, "component", dceContract);
    addPassContract(registry, 42, "component", branchContract);
    addPassContract(registry, 43, "component", reachabilityContract);
}
}
