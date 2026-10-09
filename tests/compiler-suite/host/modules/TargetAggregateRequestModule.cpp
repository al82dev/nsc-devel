#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::target::StructFieldRequest;
using nativesharp::target::StructLayoutOptions;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

void addAggregateRequestCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-003",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "aggregate-request"},
        std::move(function),
    });
}

void registerMinimumCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-017", "PR", [](TestContext& context) {
        const StructLayoutOptions options;
        const StructFieldRequest field{};
        context.expect(!options.isUnion && options.pack == 0 && options.minimumAlignment == 0, "TF-TGT-017 default options must describe a plain natural struct");
        context.expect(field.layout.size == 0 && field.layout.alignment == 0, "TF-TGT-017 default field layout must be zero");
        context.expect(!field.explicitOffset && !field.minimumAlignment && !field.bitWidth, "TF-TGT-017 default field optionals must be empty");
        context.expect(!field.isFlexibleArray && !field.isUnnamedBitField, "TF-TGT-017 default field flags must be false");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-018", "PR", [](TestContext& context) {
        const StructLayoutOptions options{true, 8, 16};
        const StructFieldRequest field{{4, 4}, 32, 8, true, std::nullopt, false};
        context.expect(options.isUnion && options.pack == 8 && options.minimumAlignment == 16, "TF-TGT-018 specified options must be exact");
        context.expect(field.layout.size == 4 && field.layout.alignment == 4, "TF-TGT-018 specified layout must be exact");
        context.expect(field.explicitOffset == 32 && field.minimumAlignment == 8, "TF-TGT-018 specified offsets and alignment must be exact");
        context.expect(field.isFlexibleArray && !field.bitWidth && !field.isUnnamedBitField, "TF-TGT-018 specified flags must be exact");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-019", "PR", [](TestContext& context) {
        const StructLayoutOptions options{};
        const StructFieldRequest field{};
        context.expect(!options.isUnion && options.pack == 0 && options.minimumAlignment == 0, "TF-TGT-019 value-init options must be defaults");
        context.expect(field.layout.size == 0 && field.layout.alignment == 0, "TF-TGT-019 value-init request must not need a producer");
        context.expect(!field.explicitOffset && !field.minimumAlignment && !field.bitWidth, "TF-TGT-019 value-init optionals must be empty");
    });
}

void registerInvalidCombinationCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-020", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array plainField{StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, false, std::nullopt, false}};
        const std::array explicitBitField{StructFieldRequest{{4, 4}, 0, std::nullopt, false, 1, false}};
        const std::array flexibleBitField{StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, true, 1, false}};
        context.expect(!layout.layoutStruct(plainField, StructLayoutOptions{false, 3, 0}), "TF-TGT-020 non-power-of-two pack must be rejected");
        context.expect(!layout.layoutStruct(explicitBitField, {}), "TF-TGT-020 explicit bit-field offset must be rejected");
        context.expect(!layout.layoutStruct(flexibleBitField, {}), "TF-TGT-020 flexible bit-field must be rejected");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-021", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const StructLayoutOptions zero{};
        const StructLayoutOptions maximumOptions{true, maximum, maximum};
        const StructFieldRequest maximumField{{maximum, maximum}, maximum, maximum, true, maximum, true};
        context.expect(!zero.isUnion && zero.pack == 0 && zero.minimumAlignment == 0, "TF-TGT-021 zero option boundary must be exact");
        context.expect(maximumOptions.isUnion && maximumOptions.pack == maximum && maximumOptions.minimumAlignment == maximum, "TF-TGT-021 maximum options must be preserved");
        context.expect(maximumField.layout.size == maximum && maximumField.layout.alignment == maximum, "TF-TGT-021 maximum layout must be preserved");
        context.expect(maximumField.explicitOffset == maximum && maximumField.minimumAlignment == maximum, "TF-TGT-021 maximum optionals must be preserved");
        context.expect(maximumField.isFlexibleArray && maximumField.bitWidth == maximum && maximumField.isUnnamedBitField, "TF-TGT-021 maximum flags and width must be preserved");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-022", "PR", [](TestContext& context) {
        const std::array fields{
            StructFieldRequest{{1, 1}, std::nullopt, std::nullopt, false, std::nullopt, false},
            StructFieldRequest{{2, 2}, 4, 4, false, std::nullopt, false},
            StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, true, std::nullopt, false},
            StructFieldRequest{{8, 8}, std::nullopt, std::nullopt, false, 7, false},
            StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, false, 0, true},
        };
        const std::array options{
            StructLayoutOptions{},
            StructLayoutOptions{true, 0, 0},
            StructLayoutOptions{false, 4, 0},
            StructLayoutOptions{false, 0, 16},
        };
        context.expect(fields[0].layout.size == 1 && !fields[0].explicitOffset, "TF-TGT-022 plain request must be exact");
        context.expect(fields[1].explicitOffset == 4 && fields[1].minimumAlignment == 4, "TF-TGT-022 explicit request must be exact");
        context.expect(fields[2].isFlexibleArray && !fields[2].bitWidth, "TF-TGT-022 flexible request must be exact");
        context.expect(fields[3].bitWidth == 7 && !fields[3].isUnnamedBitField, "TF-TGT-022 bit-field request must be exact");
        context.expect(fields[4].bitWidth == 0 && fields[4].isUnnamedBitField, "TF-TGT-022 unnamed barrier must be exact");
        context.expect(!options[0].isUnion && options[1].isUnion, "TF-TGT-022 plain and union options must be distinct");
        context.expect(options[2].pack == 4 && options[3].minimumAlignment == 16, "TF-TGT-022 pack and alignment variants must be exact");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-023", "PR", [](TestContext& context) {
        const StructLayoutOptions firstOptions{true, 8, 16};
        const StructLayoutOptions secondOptions = firstOptions;
        const StructFieldRequest firstField{{8, 8}, 24, 16, false, std::nullopt, false};
        const StructFieldRequest secondField = firstField;
        context.expect(firstOptions.isUnion == secondOptions.isUnion && firstOptions.pack == secondOptions.pack && firstOptions.minimumAlignment == secondOptions.minimumAlignment, "TF-TGT-023 copied options must be field-equal");
        context.expect(firstField.layout.size == secondField.layout.size && firstField.layout.alignment == secondField.layout.alignment, "TF-TGT-023 copied layouts must be field-equal");
        context.expect(firstField.explicitOffset == secondField.explicitOffset && firstField.minimumAlignment == secondField.minimumAlignment, "TF-TGT-023 copied optionals must be field-equal");
        context.expect(firstField.isFlexibleArray == secondField.isFlexibleArray && firstField.bitWidth == secondField.bitWidth && firstField.isUnnamedBitField == secondField.isUnnamedBitField, "TF-TGT-023 copied flags must be field-equal");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addAggregateRequestCase(registry, "TF-TGT-024", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t requestCount = 65536;
        std::vector<StructFieldRequest> requests;
        requests.reserve(requestCount);
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < requestCount; ++index)
        {
            const std::uint64_t offset = static_cast<std::uint64_t>(index) * 8;
            requests.push_back({TypeLayout{8, 8}, offset, 8, false, std::nullopt, false});
            checksum += offset + 2;
        }
        context.expect(requests.size() == requestCount, "TF-TGT-024 must preserve all 65,536 requests");
        context.expect(requests.front().explicitOffset == 0, "TF-TGT-024 first request must remain exact");
        context.expect(requests[requestCount / 2].explicitOffset == 262144, "TF-TGT-024 middle request must remain exact");
        context.expect(requests.back().explicitOffset == 524280, "TF-TGT-024 last request must remain exact");
        context.expect(checksum == 17179738112ULL, "TF-TGT-024 request checksum must remain exact");
    });
}
}

void registerTargetAggregateRequestModule(TestRegistry& registry)
{
    registerMinimumCase(registry);
    registerRepresentativeCase(registry);
    registerNoPrerequisiteCase(registry);
    registerInvalidCombinationCase(registry);
    registerBoundaryCase(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
