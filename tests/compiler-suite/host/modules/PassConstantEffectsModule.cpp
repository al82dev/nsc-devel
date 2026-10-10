#include "PassTestSupport.h"

#include <cmath>

namespace nsc::suite
{
namespace
{
using namespace pass_support;

struct FoldFixture final
{
    sema::TypeContext types;
    mir::MirArena arena;
    std::unordered_map<const mir::MirValue*, sema::ConstantValue> known;

    mir::MirInstruction* unary(mir::MirOpcode opcode, sema::TypeKind kind, sema::ConstantValue value)
    {
        auto* type = types.builtin(kind);
        auto* operand = arena.makeValue(type);
        auto* result = arena.makeValue(type);
        auto* instruction = arena.makeInstruction(opcode, {});
        instruction->setResult(result);
        instruction->addOperand(operand);
        known.emplace(operand, std::move(value));
        return instruction;
    }

    mir::MirInstruction* binary(mir::MirOpcode opcode, sema::TypeKind kind,
        sema::ConstantValue leftValue, sema::ConstantValue rightValue)
    {
        auto* type = types.builtin(kind);
        auto* left = arena.makeValue(type);
        auto* right = arena.makeValue(type);
        auto* result = arena.makeValue(type);
        auto* instruction = arena.makeInstruction(opcode, {});
        instruction->setResult(result);
        instruction->addOperand(left);
        instruction->addOperand(right);
        known.emplace(left, std::move(leftValue));
        known.emplace(right, std::move(rightValue));
        return instruction;
    }
};

void equalityContract(TestContext& context, std::size_t variant)
{
    context.expect(passes::ConstantFolder::equal(sema::ConstantValue::signedInteger(-7),
        sema::ConstantValue::signedInteger(-7)), "equal signed constants must compare equal");
    context.expect(!passes::ConstantFolder::equal(sema::ConstantValue::signedInteger(1),
        sema::ConstantValue::unsignedInteger(1)), "different constant kinds must not compare equal");
    context.expect(!passes::ConstantFolder::equal(sema::ConstantValue::floating(0.0),
        sema::ConstantValue::floating(-0.0)), "floating equality must preserve signed-zero bits");
    context.expect(passes::ConstantFolder::equal(sema::ConstantValue::stringValue("bytes"),
        sema::ConstantValue::stringValue("bytes")), "strings must compare by exact bytes");
    if (variant == 8)
        for (std::size_t index = 0; index < 4096; ++index)
            context.expect(passes::ConstantFolder::equal(sema::ConstantValue::unsignedInteger(index),
                sema::ConstantValue::unsignedInteger(index)), "large equality corpus must remain deterministic");
}

void integerModelContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    auto* add = fixture.binary(mir::MirOpcode::Add, sema::TypeKind::Byte,
        sema::ConstantValue::unsignedInteger(255), sema::ConstantValue::unsignedInteger(variant));
    const auto folded = passes::ConstantFolder::fold(add, fixture.known);
    context.expect(folded.has_value() && folded->unsignedInteger() == (255U + variant) % 256U,
        "integer fold must normalize to the declared width");
}

void conversionContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    auto* sourceType = fixture.types.builtin(sema::TypeKind::Double);
    auto* destinationType = fixture.types.builtin(sema::TypeKind::Int);
    auto* source = fixture.arena.makeValue(sourceType);
    auto* result = fixture.arena.makeValue(destinationType);
    auto* convert = fixture.arena.makeInstruction(mir::MirOpcode::Convert, {});
    convert->setResult(result);
    convert->addOperand(source);
    const double value = variant == 3 ? std::numeric_limits<double>::infinity() : 42.75;
    fixture.known.emplace(source, sema::ConstantValue::floating(value));
    const auto folded = passes::ConstantFolder::fold(convert, fixture.known);
    context.expect((variant == 3) == !folded.has_value(), "non-finite conversion alone must be rejected");
    if (variant != 3)
        context.expect(folded->signedInteger() == 42, "floating-to-integer conversion must truncate toward zero");
}

void unaryContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    const mir::MirOpcode opcode = variant % 2 == 0 ? mir::MirOpcode::BitNot : mir::MirOpcode::Negate;
    auto* instruction = fixture.unary(opcode, sema::TypeKind::Int, sema::ConstantValue::signedInteger(7));
    context.expect(passes::ConstantFolder::fold(instruction, fixture.known).has_value(),
        "supported unary operation must fold");
    auto* invalid = fixture.unary(mir::MirOpcode::Load, sema::TypeKind::Int,
        sema::ConstantValue::signedInteger(7));
    context.expect(!passes::ConstantFolder::fold(invalid, fixture.known).has_value(),
        "unsupported unary opcode must not fold");
}

void comparisonContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    auto* compare = fixture.binary(mir::MirOpcode::CompareLess, sema::TypeKind::Int,
        sema::ConstantValue::signedInteger(-1), sema::ConstantValue::signedInteger(variant));
    const auto folded = passes::ConstantFolder::fold(compare, fixture.known);
    context.expect(folded.has_value() && folded->kind() == sema::ConstantKind::Boolean && folded->boolean(),
        "signed comparison must use numeric signed ordering");
}

void arithmeticContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    const mir::MirOpcode opcode = variant == 3 ? mir::MirOpcode::Divide : mir::MirOpcode::Add;
    auto* instruction = fixture.binary(opcode, sema::TypeKind::Int,
        sema::ConstantValue::signedInteger(10), sema::ConstantValue::signedInteger(variant == 3 ? 0 : 5));
    const auto folded = passes::ConstantFolder::fold(instruction, fixture.known);
    context.expect((variant == 3) == !folded.has_value(), "divide by zero must remain a runtime trap");
    if (variant != 3) context.expect(folded->signedInteger() == 15, "integer addition must fold exactly");
}

void shiftContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    const std::int64_t count = variant == 3 ? -1 : variant == 4 ? 32 : static_cast<std::int64_t>(variant % 8);
    auto* shift = fixture.binary(mir::MirOpcode::ShiftLeft, sema::TypeKind::Int,
        sema::ConstantValue::signedInteger(1), sema::ConstantValue::signedInteger(count));
    const auto folded = passes::ConstantFolder::fold(shift, fixture.known);
    context.expect((variant == 3 || variant == 4) == !folded.has_value(),
        "shift counts outside the width must not fold");
}

void dispatchContract(TestContext& context, std::size_t variant)
{
    FoldFixture fixture;
    context.expect(!passes::ConstantFolder::fold(nullptr, fixture.known).has_value(),
        "null instruction dispatch must be rejected");
    auto* invalid = fixture.arena.makeInstruction(mir::MirOpcode::Add, {});
    invalid->setResult(fixture.arena.makeValue(fixture.types.builtin(sema::TypeKind::Int)));
    context.expect(!passes::ConstantFolder::fold(invalid, fixture.known).has_value(),
        "wrong arity must not enter binary folding");
    if (variant == 8)
        for (std::size_t index = 0; index < 2048; ++index)
            context.expect(!passes::ConstantFolder::fold(invalid, fixture.known).has_value(),
                "dispatch rejection must be deterministic at scale");
}

void pureEffectsContract(TestContext& context, std::size_t)
{
    FoldFixture fixture;
    const std::vector<mir::MirOpcode> pure = {mir::MirOpcode::Undef, mir::MirOpcode::Phi,
        mir::MirOpcode::Constant, mir::MirOpcode::Copy, mir::MirOpcode::Add,
        mir::MirOpcode::CompareEqual, mir::MirOpcode::AddressOfSymbol, mir::MirOpcode::ExtractField};
    for (mir::MirOpcode opcode : pure)
    {
        auto* instruction = fixture.arena.makeInstruction(opcode, {});
        const auto effect = passes::MirEffectClassifier::classify(instruction);
        context.expect(!effect.hasSideEffects && !effect.mayTrap && !effect.readsMemory && !effect.writesMemory,
            "pure opcode must retain an empty observable-effect set");
    }
    context.expect(passes::MirEffectClassifier::classify(nullptr).structural,
        "null instruction must classify conservatively as structural");
}

void observableEffectsContract(TestContext& context, std::size_t)
{
    FoldFixture fixture;
    auto* divide = fixture.arena.makeInstruction(mir::MirOpcode::Divide, {});
    auto* load = fixture.arena.makeInstruction(mir::MirOpcode::Load, {});
    auto* store = fixture.arena.makeInstruction(mir::MirOpcode::Store, {});
    auto* call = fixture.arena.makeInstruction(mir::MirOpcode::Call, {});
    context.expect(passes::MirEffectClassifier::classify(divide).mayTrap, "division must retain trap effect");
    context.expect(passes::MirEffectClassifier::classify(load).readsMemory, "load must retain read effect");
    const auto storeEffect = passes::MirEffectClassifier::classify(store);
    context.expect(storeEffect.hasSideEffects && storeEffect.writesMemory, "store must retain write and side effects");
    context.expect(passes::MirEffectClassifier::classify(call).hasSideEffects, "call must remain observable");
}

void removabilityContract(TestContext& context, std::size_t)
{
    FoldFixture fixture;
    auto* value = fixture.arena.makeValue(fixture.types.builtin(sema::TypeKind::Int));
    auto* add = fixture.arena.makeInstruction(mir::MirOpcode::Add, {});
    add->setResult(value);
    auto* divide = fixture.arena.makeInstruction(mir::MirOpcode::Divide, {});
    divide->setResult(value);
    context.expect(passes::MirEffectClassifier::removableWhenUnused(add), "unused pure result must be removable");
    context.expect(!passes::MirEffectClassifier::removableWhenUnused(divide), "unused trapping result must not be removable");
    context.expect(!passes::MirEffectClassifier::removableWhenUnused(nullptr), "null instruction must never be removable");
}
}

void registerPassConstantEffectsModule(TestRegistry& registry)
{
    addPassContract(registry, 21, "unit", equalityContract);
    addPassContract(registry, 22, "unit", integerModelContract);
    addPassContract(registry, 23, "unit", conversionContract);
    addPassContract(registry, 24, "unit", unaryContract);
    addPassContract(registry, 25, "unit", comparisonContract);
    addPassContract(registry, 26, "unit", arithmeticContract);
    addPassContract(registry, 27, "unit", shiftContract);
    addPassContract(registry, 28, "unit", dispatchContract);
    addPassContract(registry, 29, "unit", pureEffectsContract);
    addPassContract(registry, 30, "unit", observableEffectsContract);
    addPassContract(registry, 31, "unit", removabilityContract);
}
}
