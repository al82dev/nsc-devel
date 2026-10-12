#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/SumTypeSemantics.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-020", "Host.Language", "unit", "PR",
        {"language", "sum-type", "layout"}, std::move(function)});
}

struct LayoutExpectation
{
    std::size_t count;
    std::size_t tagSize;
};

std::vector<VariantCase> cases(std::size_t count)
{
    std::vector<VariantCase> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
        result.push_back({"Case" + std::to_string(index), 0, 1});
    return result;
}

void expectLayout(
    TestContext& context,
    const LayoutExpectation expectation)
{
    const std::vector<VariantCase> input = cases(expectation.count);
    const std::optional<VariantLayout> layout = computeWindowsX64VariantLayout(input);
    context.expect(layout.has_value() && layout->tagSize == expectation.tagSize &&
            layout->payloadOffset == expectation.tagSize && layout->size == expectation.tagSize &&
            layout->alignment == expectation.tagSize,
        "TL-LANG-261 each case-count boundary must use the smallest exact tag layout");
}

void registerTagBoundaryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-261", [](TestContext& context) {
        expectLayout(context, {1, 1});
        expectLayout(context, {255, 1});
        expectLayout(context, {256, 1});
        expectLayout(context, {257, 2});
        expectLayout(context, {65'535, 2});
        expectLayout(context, {65'536, 2});
        expectLayout(context, {65'537, 4});
    });
}

void registerOverflowRejectionCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LANG-262",
        "CL-LANG-020",
        "Host.Language",
        "unit",
        "PR",
        {"language", "sum-type", "layout"},
        [](TestContext& context) {
            constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
            const std::array<std::array<VariantCase, 1>, 5> structurallyInvalid = {{
                {{{"", 1, 1}}},
                {{{"ZeroAlignment", 1, 0}}},
                {{{"NonPowerOfTwoAlignment", 1, 3}}},
                {{{"ExcessiveAlignment", 1, 32}}},
                {{{"PayloadAdditionOverflow", maximum, 16}}},
            }};
            for (const auto& input : structurallyInvalid)
            {
                context.expect(!computeWindowsX64VariantLayout(input).has_value(),
                    "TL-LANG-262 every invalid name, alignment, or payload sum must be rejected");
            }

            const std::array<VariantCase, 1> finalAlignmentOverflow = {{
                {"FinalAlignmentOverflow", maximum - 16U, 16},
            }};
            context.expect(!computeWindowsX64VariantLayout(finalAlignmentOverflow).has_value(),
                "TL-LANG-262 final layout alignment must reject overflow instead of publishing a wrapped size");
        },
    });
}

void registerMixedPayloadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-263", [](TestContext& context) {
        const std::array<VariantCase, 4> input = {{{"Empty", 0, 1}, {"Byte", 1, 1},
            {"Word", 6, 2}, {"Wide", 17, 16}}};
        const std::optional<VariantLayout> layout = computeWindowsX64VariantLayout(input);
        context.expect(layout.has_value() && layout->tagSize == 1 && layout->payloadOffset == 16 &&
                layout->size == 48 && layout->alignment == 16,
            "TL-LANG-263 mixed payloads must use maximum payload size/alignment and exact final padding");
        const std::array<VariantCase, 2> historical = {{{"Ok", 4, 4}, {"Error", 16, 8}}};
        const std::optional<VariantLayout> historicalLayout = computeWindowsX64VariantLayout(historical);
        context.expect(historicalLayout.has_value() && historicalLayout->tagSize == 1 &&
                historicalLayout->payloadOffset == 8 && historicalLayout->size == 24 &&
                historicalLayout->alignment == 8,
            "TL-LANG-263 the historical mixed layout must remain byte-for-byte stable");
    });
}

void registerMetadataIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-270", [](TestContext& context) {
        const std::array<VariantCase, 0> empty{};
        const std::array<VariantCase, 1> one = {{{"Only", 1, 1}}};
        const std::array<VariantCase, 3> many = {
            {{"A", 0, 1}, {"Wide", 16, 8}, {"Last", 4, 4}}};
        const std::array<VariantCase, 3> differentPayloads = {
            {{"A", 4'096, 16}, {"Wide", 0, 1}, {"Last", 99, 1}}};
        context.expect(variantMetadataIdentity("Demo.Empty", empty) == "Demo.Empty|variant|" &&
                variantMetadataIdentity("Demo.One", one) == "Demo.One|variant|4:Only;" &&
                variantMetadataIdentity("Demo.Many", many) == "Demo.Many|variant|1:A;4:Wide;4:Last;",
            "TL-LANG-270 zero, one and many cases must preserve qualified name and length-prefixed names");
        context.expect(variantMetadataIdentity("Demo.Many", differentPayloads) ==
                variantMetadataIdentity("Demo.Many", many),
            "TL-LANG-270 payload size and alignment must not affect metadata identity");
    });
}
}

void registerLanguageSumTypeLayoutModule(TestRegistry& registry)
{
    registerTagBoundaryCase(registry);
    registerOverflowRejectionCase(registry);
    registerMixedPayloadCase(registry);
    registerMetadataIdentityCase(registry);
}
}
