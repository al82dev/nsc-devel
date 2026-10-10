#include "SemaTestSupport.h"

#include "nativesharp/Sema/EffectContext.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Sema/ResolvedOperatorInfo.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <memory>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::addSemaCase;
constexpr std::array<Effect, 5> effects = {
    Effect::Unsafe, Effect::Throws, Effect::Async, Effect::Comptime, Effect::NoHeap};
constexpr std::array<ValueCategory, 5> categories = {ValueCategory::Value, ValueCategory::Place,
    ValueCategory::Temporary, ValueCategory::BorrowedValue, ValueCategory::MovedFrom};

void c018(TestContext& c)
{
    const EffectSet set;
    bool exact = set.bits() == 0;
    for (const Effect effect : effects) exact = exact && !set.contains(effect);
    c.expect(exact, "TS-SEMA-018 default EffectSet is exactly empty");
}

void c019(TestContext& c)
{
    for (const Effect selected : effects)
    {
        const EffectSet set(selected);
        bool exact = true;
        for (const Effect current : effects) exact = exact && set.contains(current) == (current == selected);
        c.expect(exact, "TS-SEMA-019 each effect constructor selects only its own bit");
    }
}

void c020(TestContext& c)
{
    const EffectSet original(Effect::Unsafe);
    const EffectSet result = original.with(Effect::Throws).with(Effect::Throws);
    c.expect(original.bits() == static_cast<std::uint32_t>(Effect::Unsafe) &&
            result.contains(Effect::Unsafe) && result.contains(Effect::Throws),
        "TS-SEMA-020 with is immutable, cumulative, and idempotent");
}

void c021(TestContext& c)
{
    const std::array<std::string_view, 5> names = {"unsafe", "throws", "async", "comptime", "noheap"};
    bool exact = effectName(static_cast<Effect>(1U << 31U)) == "unknown";
    for (std::size_t index = 0; index < effects.size(); ++index)
        exact = exact && effectName(effects[index]) == names[index];
    c.expect(exact, "TS-SEMA-021 effect names and unknown fallback match exactly");
}

void c022(TestContext& c)
{
    c.expect(!effectParticipatesInFunctionType(Effect::Unsafe) &&
            effectParticipatesInFunctionType(Effect::Throws) &&
            effectParticipatesInFunctionType(Effect::Async) &&
            !effectParticipatesInFunctionType(Effect::Comptime) &&
            effectParticipatesInFunctionType(Effect::NoHeap),
        "TS-SEMA-022 only Throws, Async, and NoHeap participate in function types");
}

void c023(TestContext& c)
{
    const EffectContext local{EffectSet(Effect::Throws)};
    c.expect(local.allows(Effect::Throws) && !local.require(Effect::Throws).has_value() &&
            !local.allows(Effect::Unsafe) && local.require(Effect::Unsafe) == Effect::Unsafe,
        "TS-SEMA-023 allows and require are exact duals in a local context");
}

void c024(TestContext& c)
{
    const EffectContext root{EffectSet(Effect::Unsafe)};
    const EffectContext middle(EffectSet(Effect::Throws), &root);
    const EffectContext leaf(EffectSet(Effect::Async), &middle);
    c.expect(leaf.allows(Effect::Unsafe) && leaf.allows(Effect::Throws) &&
            leaf.allows(Effect::Async) && !leaf.allows(Effect::NoHeap),
        "TS-SEMA-024 parent chains expose every inherited authorization");
}

void c025(TestContext& c)
{
    const EffectSet parentSet = EffectSet(Effect::Unsafe).with(Effect::Throws);
    const EffectContext parent(parentSet);
    const EffectContext child(EffectSet(Effect::Throws).with(Effect::Async), &parent);
    c.expect(child.allows(Effect::Unsafe) && child.allows(Effect::Throws) &&
            child.allows(Effect::Async) && parent.localEffects().bits() == parentSet.bits(),
        "TS-SEMA-025 context shadowing forms a logical union without mutating parents");
}

void c026(TestContext& c)
{
    constexpr std::size_t count = 256;
    std::vector<std::unique_ptr<EffectContext>> chain;
    chain.reserve(count);
    chain.push_back(std::make_unique<EffectContext>(EffectSet(Effect::NoHeap)));
    for (std::size_t index = 1; index < count; ++index)
        chain.push_back(std::make_unique<EffectContext>(EffectSet{}, chain.back().get()));
    c.expect(chain.back()->allows(Effect::NoHeap),
        "TS-SEMA-026 deep acyclic contexts preserve the current inherited behavior");
}

void c027(TestContext& c)
{
    c.expect(static_cast<int>(ValueCategory::Value) == 0 &&
            static_cast<int>(ValueCategory::Place) == 1 && ValueCategory::LValue == ValueCategory::Place &&
            ValueCategory::RValue == ValueCategory::Value,
        "TS-SEMA-027 category numeric identities and compatibility aliases stay stable");
}

void c028(TestContext& c)
{
    for (const ValueCategory value : categories)
        c.expect(isPlaceCategory(value) ==
                (value == ValueCategory::Place || value == ValueCategory::BorrowedValue),
            "TS-SEMA-028 only Place and BorrowedValue are place categories");
}

void c029(TestContext& c)
{
    for (const ValueCategory value : categories)
        c.expect(isUsableValueCategory(value) == (value != ValueCategory::MovedFrom),
            "TS-SEMA-029 only MovedFrom is unusable");
}

void c030(TestContext& c)
{
    for (const ValueCategory value : categories)
        c.expect(requiresMaterialization(value) == (value == ValueCategory::Temporary),
            "TS-SEMA-030 only Temporary requires materialization");
}

void c031(TestContext& c)
{
    const std::array<std::string_view, 5> names = {
        "value", "place", "temporary", "borrowed-value", "moved-from"};
    bool exact = valueCategoryName(static_cast<ValueCategory>(99)) == "value";
    for (std::size_t index = 0; index < categories.size(); ++index)
        exact = exact && valueCategoryName(categories[index]) == names[index];
    c.expect(exact, "TS-SEMA-031 category names and external fallback match exactly");
}

void c032(TestContext& c)
{
    const ExpressionInfo info;
    c.expect(info.type == nullptr && info.category == ValueCategory::Value && !info.modifiable &&
            !info.addressable && !info.isConstant && !info.requiresBoundsCheck &&
            info.constantValue.kind() == ConstantKind::None && info.symbol == nullptr,
        "TS-SEMA-032 default ExpressionInfo preserves all null and false fields");
}

void c033(TestContext& c)
{
    TypeContext types;
    Symbol symbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    ExpressionInfo info;
    info.type = symbol.type();
    info.category = ValueCategory::Place;
    info.modifiable = info.addressable = info.isConstant = info.requiresBoundsCheck = true;
    info.constantValue = ConstantValue::signedInteger(7);
    info.symbol = &symbol;
    c.expect(info.type == symbol.type() && info.category == ValueCategory::Place && info.modifiable &&
            info.addressable && info.isConstant && info.requiresBoundsCheck &&
            info.constantValue.signedInteger() == 7 && info.symbol == &symbol,
        "TS-SEMA-033 ExpressionInfo fields remain orthogonal");
}

void c034(TestContext& c)
{
    const ResolvedOperatorInfo info;
    c.expect(info.tokenKind == nativesharp::lexer::TokenKind::Invalid && info.leftType == nullptr &&
            info.rightType == nullptr && info.operationType == nullptr && info.resultType == nullptr && !info.unary,
        "TS-SEMA-034 default ResolvedOperatorInfo is invalid, null, and non-unary");
}

void c035(TestContext& c)
{
    TypeContext types;
    ResolvedOperatorInfo unary;
    unary.tokenKind = nativesharp::lexer::TokenKind::Minus;
    unary.leftType = unary.operationType = unary.resultType = types.builtin(TypeKind::Int);
    unary.unary = true;
    ResolvedOperatorInfo binary = unary;
    binary.tokenKind = nativesharp::lexer::TokenKind::Plus;
    binary.rightType = types.builtin(TypeKind::Long);
    binary.unary = false;
    c.expect(unary.unary && unary.rightType == nullptr &&
            binary.tokenKind == nativesharp::lexer::TokenKind::Plus && binary.rightType != nullptr && !binary.unary,
        "TS-SEMA-035 unary and binary operator records preserve token, types, and arity");
}
}

void registerSemaEffectsExpressionModule(TestRegistry& registry)
{
    const std::array<TestFunction, 18> tests = {c018, c019, c020, c021, c022, c023, c024, c025,
        c026, c027, c028, c029, c030, c031, c032, c033, c034, c035};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 18;
        const std::string id = "TS-SEMA-0" + std::to_string(ordinal);
        addSemaCase(registry, id, ordinal <= 26 ? "CS-SEMA-004" : "CS-SEMA-005", "unit",
            tests[index], ordinal == 26 ? "NIGHTLY" : "PR");
    }
}
}
