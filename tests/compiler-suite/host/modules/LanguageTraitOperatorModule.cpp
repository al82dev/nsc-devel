#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

constexpr std::array<OperatorTraitContract, 20> kExpectedOperators = {{
    {"+", 1, "lang:UnaryPlus", "op_UnaryPlus"},
    {"-", 1, "lang:Negate", "op_UnaryNegation"},
    {"!", 1, "lang:LogicalNot", "op_LogicalNot"},
    {"~", 1, "lang:OnesComplement", "op_OnesComplement"},
    {"+", 2, "lang:Add", "op_Addition"},
    {"-", 2, "lang:Subtract", "op_Subtraction"},
    {"*", 2, "lang:Multiply", "op_Multiply"},
    {"/", 2, "lang:Divide", "op_Division"},
    {"%", 2, "lang:Remainder", "op_Modulus"},
    {"&", 2, "lang:BitwiseAnd", "op_BitwiseAnd"},
    {"|", 2, "lang:BitwiseOr", "op_BitwiseOr"},
    {"^", 2, "lang:ExclusiveOr", "op_ExclusiveOr"},
    {"<<", 2, "lang:LeftShift", "op_LeftShift"},
    {">>", 2, "lang:RightShift", "op_RightShift"},
    {"==", 2, "lang:Equality", "op_Equality"},
    {"!=", 2, "lang:Inequality", "op_Inequality"},
    {"<", 2, "lang:LessThan", "op_LessThan"},
    {"<=", 2, "lang:LessThanOrEqual", "op_LessThanOrEqual"},
    {">", 2, "lang:GreaterThan", "op_GreaterThan"},
    {">=", 2, "lang:GreaterThanOrEqual", "op_GreaterThanOrEqual"},
}};

struct LegacyProjection final
{
    std::string_view spelling;
    std::string_view langItem;
};

constexpr std::array<LegacyProjection, 18> kExpectedLegacyProjections = {{
    {"+", "lang:Add"},
    {"-", "lang:Subtract"},
    {"!", "lang:LogicalNot"},
    {"~", "lang:OnesComplement"},
    {"*", "lang:Multiply"},
    {"/", "lang:Divide"},
    {"%", "lang:Remainder"},
    {"&", "lang:BitwiseAnd"},
    {"|", "lang:BitwiseOr"},
    {"^", "lang:ExclusiveOr"},
    {"<<", "lang:LeftShift"},
    {">>", "lang:RightShift"},
    {"==", "lang:Equality"},
    {"!=", "lang:Inequality"},
    {"<", "lang:LessThan"},
    {"<=", "lang:LessThanOrEqual"},
    {">", "lang:GreaterThan"},
    {">=", "lang:GreaterThanOrEqual"},
}};

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-021", "Host.Language", "unit", "PR",
        {"language", "trait", "operator"}, std::move(function)});
}

void registerOperatorTableCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-282", [](TestContext& context) {
        const std::span<const OperatorTraitContract> actual = operatorTraitContracts();
        context.expect(actual.size() == kExpectedOperators.size(),
            "TL-LANG-282 the operator registry must contain exactly 20 rows");
        if (actual.size() != kExpectedOperators.size()) return;
        for (std::size_t index = 0; index < kExpectedOperators.size(); ++index)
        {
            const OperatorTraitContract& expected = kExpectedOperators[index];
            const OperatorTraitContract& found = actual[index];
            context.expect(found.spelling == expected.spelling && found.arity == expected.arity &&
                    found.langItem == expected.langItem && found.methodName == expected.methodName,
                "TL-LANG-282 every row and field must preserve the frozen operator table order");
        }
    });
}

void registerForwardLookupCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-283", [](TestContext& context) {
        const std::span<const OperatorTraitContract> contracts = operatorTraitContracts();
        for (const OperatorTraitContract& contract : contracts)
        {
            context.expect(operatorTraitContract(contract.spelling, contract.arity) == &contract,
                "TL-LANG-283 each spelling+arity lookup must return its exact registry row");
        }
        context.expect(operatorTraitContract("+", 0) == nullptr &&
                operatorTraitContract("+", 3) == nullptr &&
                operatorTraitContract("&&", 2) == nullptr &&
                operatorTraitContract("unknown", 1) == nullptr,
            "TL-LANG-283 unsupported spelling+arity pairs must return null");
    });
}

void registerReverseLookupCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-284", [](TestContext& context) {
        const std::span<const OperatorTraitContract> contracts = operatorTraitContracts();
        for (const OperatorTraitContract& contract : contracts)
        {
            context.expect(operatorTraitContractForMethod(contract.methodName, contract.arity) == &contract,
                "TL-LANG-284 each method+arity lookup must return its exact registry row");
        }
        context.expect(operatorTraitContractForMethod("op_Addition", 1) == nullptr &&
                operatorTraitContractForMethod("op_UnaryPlus", 2) == nullptr &&
                operatorTraitContractForMethod("op_Unknown", 2) == nullptr,
            "TL-LANG-284 unsupported method+arity pairs must return null");
    });
}

void registerProjectionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-285", [](TestContext& context) {
        for (const OperatorTraitContract& expected : kExpectedOperators)
        {
            context.expect(operatorTrait(expected.spelling, expected.arity) == expected.langItem &&
                    operatorMethodName(expected.spelling, expected.arity) == expected.methodName,
                "TL-LANG-285 arity-aware trait and method projections must match every frozen row");
        }
        for (const LegacyProjection& expected : kExpectedLegacyProjections)
        {
            context.expect(operatorTrait(expected.spelling) == expected.langItem,
                "TL-LANG-285 each legacy projection must match its exact unary-or-binary preference");
        }
        context.expect(operatorTrait("unknown", 2).empty() &&
                operatorMethodName("unknown", 2).empty() && operatorTrait("unknown").empty(),
            "TL-LANG-285 every unknown projection must return an empty view");
    });
}

void registerOverloadableCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-286", [](TestContext& context) {
        for (const OperatorTraitContract& expected : kExpectedOperators)
        {
            context.expect(operatorCanBeOverloaded(expected.spelling),
                "TL-LANG-286 every spelling present in the contractual table must be overloadable");
        }
        constexpr std::array<std::string_view, 6> forbidden = {
            "&&", "||", "??", "unknown", "", "=>"};
        for (const std::string_view spelling : forbidden)
        {
            context.expect(!operatorCanBeOverloaded(spelling),
                "TL-LANG-286 short-circuiting and unknown spellings must not be overloadable");
        }
    });
}
}

void registerLanguageTraitOperatorModule(TestRegistry& registry)
{
    registerOperatorTableCase(registry);
    registerForwardLookupCase(registry);
    registerReverseLookupCase(registry);
    registerProjectionCase(registry);
    registerOverloadableCase(registry);
}
}
