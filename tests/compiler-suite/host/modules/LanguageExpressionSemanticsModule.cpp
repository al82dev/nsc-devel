#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ExpressionErgonomics.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-008", "Host.Language", "unit", "PR",
        {"language", "expressions"}, std::move(function)});
}

InterpolationPart literal(std::string text)
{
    return {InterpolationPartKind::Literal, std::move(text), {}, 0, 0};
}

InterpolationPart hole(std::string text, std::string format = {})
{
    return {InterpolationPartKind::Hole, std::move(text), std::move(format), 0, 0};
}

void registerLoweringContractCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-098", [](TestContext& context) {
        struct Vector final { ExpressionFeature feature; std::string_view spelling; };
        constexpr std::array vectors = {
            Vector{ExpressionFeature::NamedArguments, "named arguments"},
            Vector{ExpressionFeature::TypedParameterPacks, "params ReadOnlySpan<T>"},
            Vector{ExpressionFeature::NullAwareOperators, "null-aware operators"},
            Vector{ExpressionFeature::RangeIndex, ".. / ^"},
            Vector{ExpressionFeature::GeneralizedInitializers, "generalized initializer"},
            Vector{ExpressionFeature::StaticInterpolation, "$\"...\""},
            Vector{ExpressionFeature::WithExpressions, "with"},
        };
        for (const Vector& vector : vectors)
        {
            const ExpressionLoweringContract& contract = expressionLoweringContract(vector.feature);
            context.expect(contract.feature == vector.feature && contract.spelling == vector.spelling &&
                    contract.sourceOrder && contract.primaryEvaluatedOnce &&
                    !contract.mayAllocateImplicitly && contract.preservesAbi,
                "TL-LANG-098 each feature must expose its exact allocation-free lowering contract");
        }
    });
}

void registerInvalidFeatureCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LANG-099",
        "CL-LANG-008",
        "Host.Language",
        "unit",
        "NIGHTLY",
        {"language", "expressions", "known-product-defect"},
        [](TestContext& context) {
            constexpr ExpressionFeature invalid = static_cast<ExpressionFeature>(7);
            const ExpressionLoweringContract& observed = expressionLoweringContract(invalid);
            context.expect(false,
                "TL-LANG-099 an external ExpressionFeature must be rejected before table indexing; "
                "the current product returned feature " +
                    std::to_string(static_cast<unsigned int>(observed.feature)));
        },
        TestDisposition::KnownProductDefect,
    });
}

void registerFeatureAvailabilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-100", [](TestContext& context) {
        constexpr std::array features = {
            ExpressionFeature::NamedArguments,
            ExpressionFeature::TypedParameterPacks,
            ExpressionFeature::NullAwareOperators,
            ExpressionFeature::RangeIndex,
            ExpressionFeature::GeneralizedInitializers,
            ExpressionFeature::StaticInterpolation,
            ExpressionFeature::WithExpressions,
        };
        for (const ExpressionFeature feature : features)
            context.expect(expressionFeatureAvailable(feature),
                "TL-LANG-100 every declared expression feature must remain available");
    });
}

void registerTypedPackClassificationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-101", [](TestContext& context) {
        struct Vector final { std::string_view type; bool expected; };
        constexpr std::array vectors = {
            Vector{"ReadOnlySpan<i32>", true},
            Vector{"params ReadOnlySpan<i32>", true},
            Vector{"prefixReadOnlySpan<i32>", true},
            Vector{"ReadOnlySpan<>", true},
            Vector{"ReadOnlySpan<i32>>", true},
            Vector{"", false},
            Vector{"ReadOnlySpan<i32", false},
            Vector{"ReadOnlySpan<i32> trailing", false},
            Vector{"Span<i32>", false},
        };
        for (const Vector& vector : vectors)
            context.expect(isTypedParameterPackType(vector.type) == vector.expected,
                "TL-LANG-101 marker position, empty payload, closing byte, and trailing text must be exact");
    });
}

void registerNullAwareClassificationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-102", [](TestContext& context) {
        struct Vector final
        {
            std::string_view type;
            NullAwareOperandKind kind;
            bool supported;
        };
        constexpr std::array vectors = {
            Vector{"i32*", NullAwareOperandKind::Pointer, true},
            Vector{"type:Widget", NullAwareOperandKind::Reference, true},
            Vector{"builtin:string", NullAwareOperandKind::Reference, true},
            Vector{"Optional<i32>", NullAwareOperandKind::StaticOptional, true},
            Vector{"prefix.Optional<i32>", NullAwareOperandKind::StaticOptional, true},
            Vector{"type:Optional<i32>*", NullAwareOperandKind::Pointer, true},
            Vector{"type:Optional<i32>", NullAwareOperandKind::Reference, true},
            Vector{"i32", NullAwareOperandKind::NotNullable, false},
            Vector{"", NullAwareOperandKind::NotNullable, false},
        };
        for (const Vector& vector : vectors)
        {
            context.expect(classifyNullAwareOperand(vector.type) == vector.kind &&
                    supportsNullAwareOperators(vector.type) == vector.supported,
                "TL-LANG-102 classification priority and null-aware support must remain coherent");
        }
    });
}

void registerRangeEndpointCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-103", [](TestContext& context) {
        constexpr std::size_t length = 5;
        context.expect(resolveRangeEndpoint({0, false}, length) == 0 &&
                resolveRangeEndpoint({length, false}, length) == length &&
                !resolveRangeEndpoint({length + 1, false}, length).has_value(),
            "TL-LANG-103 forward endpoints must accept exactly zero through length");
        context.expect(resolveRangeEndpoint({0, true}, length) == length &&
                resolveRangeEndpoint({length, true}, length) == 0 &&
                !resolveRangeEndpoint({length + 1, true}, length).has_value(),
            "TL-LANG-103 from-end endpoints must subtract exactly and reject length plus one");
    });
}

void registerHalfOpenRangeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-104", [](TestContext& context) {
        const auto expectRange = [&context](
            std::optional<RangeEndpoint> start,
            std::optional<RangeEndpoint> end,
            std::size_t expectedStart,
            std::size_t expectedEnd,
            std::string message)
        {
            const auto range = resolveHalfOpenRange(start, end, 8);
            context.expect(range.has_value() && range->start == expectedStart && range->end == expectedEnd,
                std::move(message));
        };
        expectRange(std::nullopt, std::nullopt, 0, 8,
            "TL-LANG-104 omitted endpoints must resolve to the whole range");
        expectRange(RangeEndpoint{2, false}, std::nullopt, 2, 8,
            "TL-LANG-104 an omitted end must resolve to length");
        expectRange(std::nullopt, RangeEndpoint{2, true}, 0, 6,
            "TL-LANG-104 a from-end endpoint must resolve before ordering");
        expectRange(RangeEndpoint{4, false}, RangeEndpoint{4, false}, 4, 4,
            "TL-LANG-104 an empty half-open range must remain valid");
        context.expect(!resolveHalfOpenRange(RangeEndpoint{6, false}, RangeEndpoint{2, false}, 8).has_value() &&
                !resolveHalfOpenRange(RangeEndpoint{9, false}, std::nullopt, 8).has_value() &&
                !resolveHalfOpenRange(std::nullopt, RangeEndpoint{9, true}, 8).has_value(),
            "TL-LANG-104 reversed or out-of-bounds endpoints must return nullopt");
    });
}

void registerSimpleInterpolationParseCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-105", [](TestContext& context) {
        std::vector<InterpolationPart> parts;
        std::string error = "stale";
        context.expect(parseStaticInterpolation("plain text", parts, error) && error.empty() &&
                parts.size() == 1 && parts[0].kind == InterpolationPartKind::Literal &&
                parts[0].text == "plain text" && parts[0].expressionOffset == 0 &&
                parts[0].expressionLength == 0,
            "TL-LANG-105 plain text must produce one exact literal part");

        context.expect(parseStaticInterpolation("Hello { name }!", parts, error) && error.empty() &&
                parts.size() == 3 && parts[0].kind == InterpolationPartKind::Literal &&
                parts[0].text == "Hello " && parts[1].kind == InterpolationPartKind::Hole &&
                parts[1].text == "name" && parts[1].format.empty() &&
                parts[1].expressionOffset == 8 && parts[1].expressionLength == 4 &&
                parts[2].kind == InterpolationPartKind::Literal && parts[2].text == "!",
            "TL-LANG-105 a trimmed hole must preserve exact parts and source byte offsets");

        context.expect(parseStaticInterpolation("{value:X4}", parts, error) && error.empty() &&
                parts.size() == 1 && parts[0].kind == InterpolationPartKind::Hole &&
                parts[0].text == "value" && parts[0].format == "X4" &&
                parts[0].expressionOffset == 1 && parts[0].expressionLength == 5,
            "TL-LANG-105 a formatted hole must split expression, format, and offsets exactly");
    });
}

void registerComplexInterpolationParseCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-106", [](TestContext& context) {
        std::vector<InterpolationPart> parts;
        std::string error;
        context.expect(parseStaticInterpolation("{{left}} and {{right}}", parts, error) &&
                error.empty() && parts.size() == 1 && parts[0].text == "{left} and {right}",
            "TL-LANG-106 escaped braces must decode into one literal part");

        constexpr std::string_view source = "x={call(\"}\", nested[0]):X2}";
        constexpr std::string_view expression = "call(\"}\", nested[0])";
        context.expect(parseStaticInterpolation(source, parts, error) && error.empty() &&
                parts.size() == 2 && parts[0].kind == InterpolationPartKind::Literal &&
                parts[0].text == "x=" && parts[1].kind == InterpolationPartKind::Hole &&
                parts[1].text == expression && parts[1].format == "X2" &&
                parts[1].expressionOffset == 3 && parts[1].expressionLength == expression.size(),
            "TL-LANG-106 quotes, parentheses, brackets, and a top-level format must segment exactly");

        constexpr std::string_view nested = "{outer({inner}):fmt}";
        context.expect(parseStaticInterpolation(nested, parts, error) && error.empty() &&
                parts.size() == 1 && parts[0].text == "outer({inner})" && parts[0].format == "fmt",
            "TL-LANG-106 nested braces must not terminate the enclosing interpolation hole");
    });
}

void registerInvalidInterpolationParseCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-107", [](TestContext& context) {
        struct Vector final { std::string_view source; std::string_view error; };
        constexpr std::array vectors = {
            Vector{"text } tail", "unmatched '}' in interpolated string"},
            Vector{"text {value", "unterminated interpolation hole"},
            Vector{"{}", "interpolation hole requires an expression"},
            Vector{"{   }", "interpolation hole requires an expression"},
            Vector{"{:X4}", "interpolation hole requires an expression"},
            Vector{"{value:X4", "unterminated interpolation hole"},
            Vector{"{\"unterminated}", "unterminated interpolation hole"},
        };
        for (const Vector& vector : vectors)
        {
            std::vector<InterpolationPart> parts{literal("stale")};
            std::string error;
            context.expect(!parseStaticInterpolation(vector.source, parts, error) &&
                    error == vector.error && parts.empty(),
                "TL-LANG-107 every invalid brace, expression, format, or quote must clear parts and diagnose");
        }
    });
}

class TraceHandler final : public StaticInterpolationHandler
{
public:
    explicit TraceHandler(std::optional<std::size_t> failingCall = std::nullopt, bool leaveErrorEmpty = false)
        : failingCall_(failingCall), leaveErrorEmpty_(leaveErrorEmpty) {}

    bool appendLiteral(std::string_view text, std::string& error) override
    {
        return append("literal:" + std::string(text), error);
    }

    bool appendHole(const InterpolationPart& part, std::string& error) override
    {
        return append("hole:" + part.text + ":" + part.format, error);
    }

    [[nodiscard]] const std::vector<std::string>& calls() const noexcept { return calls_; }

private:
    bool append(std::string call, std::string& error)
    {
        calls_.push_back(std::move(call));
        if (!failingCall_.has_value() || calls_.size() - 1 != *failingCall_) return true;
        if (!leaveErrorEmpty_) error = "handler failure at " + std::to_string(*failingCall_);
        return false;
    }

    std::optional<std::size_t> failingCall_;
    bool leaveErrorEmpty_ = false;
    std::vector<std::string> calls_;
};

void registerInterpolationHandlerCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-108", [](TestContext& context) {
        const std::vector<InterpolationPart> parts = {
            literal("a"), hole("value", "X2"), literal("z")};
        TraceHandler success;
        std::string error = "stale";
        context.expect(renderStaticInterpolation(parts, success, error) && error.empty() &&
                success.calls() == std::vector<std::string>{"literal:a", "hole:value:X2", "literal:z"},
            "TL-LANG-108 a successful handler must observe every part in source order");

        for (std::size_t failure = 0; failure < parts.size(); ++failure)
        {
            TraceHandler handler(failure);
            context.expect(!renderStaticInterpolation(parts, handler, error) &&
                    error == "handler failure at " + std::to_string(failure) &&
                    handler.calls().size() == failure + 1,
                "TL-LANG-108 handler failure must propagate exactly and short-circuit later parts");
        }
        TraceHandler silentFailure(1, true);
        context.expect(!renderStaticInterpolation(parts, silentFailure, error) &&
                error == "compile-time interpolation handler rejected a part" &&
                silentFailure.calls().size() == 2,
            "TL-LANG-108 an empty handler diagnostic must receive the canonical fallback");
    });
}

void registerBuiltinHoleSupportCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-109", [](TestContext& context) {
        struct Vector final { InterpolationPart part; bool expected; };
        const std::array vectors = {
            Vector{literal("anything"), true},
            Vector{hole("true"), true}, Vector{hole("false"), true},
            Vector{hole("true", "D"), false},
            Vector{hole("0"), true}, Vector{hole("-42"), true},
            Vector{hole("42", "D"), true}, Vector{hole("42", "d3"), true},
            Vector{hole("42", "X64"), true}, Vector{hole("42", "x0"), true},
            Vector{hole("-42", "X"), false}, Vector{hole("-42", "x2"), false},
            Vector{hole("42", "D65"), false}, Vector{hole("42", "D-1"), false},
            Vector{hole("42", "D2x"), false}, Vector{hole("42", "Q2"), false},
            Vector{hole(""), false}, Vector{hole("+42"), false}, Vector{hole("42x"), false},
        };
        for (const Vector& vector : vectors)
            context.expect(builtinStaticInterpolationHoleSupported(vector.part) == vector.expected,
                "TL-LANG-109 bool, integer, mode, sign, and width support matrix must be exact");
    });
}

std::optional<std::string> renderBuiltin(
    std::initializer_list<InterpolationPart> parts,
    std::string& error)
{
    std::string result;
    const std::vector<InterpolationPart> values(parts);
    if (!renderBuiltinStaticInterpolation(values, result, error)) return std::nullopt;
    return result;
}

void registerBuiltinRenderingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-110", [](TestContext& context) {
        std::string error;
        context.expect(renderBuiltin({hole("42")}, error) == "42" &&
                renderBuiltin({hole("42", "D5")}, error) == "00042" &&
                renderBuiltin({hole("42", "d3")}, error) == "042",
            "TL-LANG-110 decimal rendering and D/d width must produce exact bytes");
        context.expect(renderBuiltin({hole("255", "X4")}, error) == "00FF" &&
                renderBuiltin({hole("255", "x4")}, error) == "00ff",
            "TL-LANG-110 X/x rendering must preserve requested hex case and width");
        context.expect(renderBuiltin({hole("-42", "D5")}, error) == "-0042" &&
                renderBuiltin({hole("-0", "D5")}, error) == "00000",
            "TL-LANG-110 sign must participate in decimal width and negative zero must normalize");
        context.expect(renderBuiltin({literal("a="), hole("true"), literal(",b="), hole("false")}, error) ==
                "a=true,b=false" && error.empty(),
            "TL-LANG-110 literals and booleans must render in exact source order");
    });
}

void registerBuiltinRenderingFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-111", [](TestContext& context) {
        const std::array invalid = {
            hole("-1", "X"),
            hole("1", "D65"),
            hole("1", "Q2"),
            hole("name"),
            hole("18446744073709551616"),
        };
        for (const InterpolationPart& part : invalid)
        {
            std::string result = "stale";
            std::string error;
            const std::vector<InterpolationPart> parts{literal("prefix"), part, literal("suffix")};
            const std::string expected = "static interpolation hole '" + part.text +
                "' must be a bool or integer literal with constant D/d/X/x format; "
                "runtime holes require an explicit formatting handler";
            context.expect(!renderBuiltinStaticInterpolation(parts, result, error) &&
                    error == expected && result.empty(),
                "TL-LANG-111 unsupported builtin holes must fail exactly and clear partial output");
        }
    });
}
}

void registerLanguageExpressionSemanticsModule(TestRegistry& registry)
{
    registerLoweringContractCase(registry);
    registerInvalidFeatureCase(registry);
    registerFeatureAvailabilityCase(registry);
    registerTypedPackClassificationCase(registry);
    registerNullAwareClassificationCase(registry);
    registerRangeEndpointCase(registry);
    registerHalfOpenRangeCase(registry);
    registerSimpleInterpolationParseCase(registry);
    registerComplexInterpolationParseCase(registry);
    registerInvalidInterpolationParseCase(registry);
    registerInterpolationHandlerCase(registry);
    registerBuiltinHoleSupportCase(registry);
    registerBuiltinRenderingCase(registry);
    registerBuiltinRenderingFailureCase(registry);
}
}
