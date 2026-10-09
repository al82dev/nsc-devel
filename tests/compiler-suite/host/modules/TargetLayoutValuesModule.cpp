#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::target::ArrayLayout;
using nativesharp::target::StructFieldLayout;
using nativesharp::target::StructLayout;
using nativesharp::target::TypeLayout;

void addLayoutValueCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-002",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "layout-value"},
        std::move(function),
    });
}

void registerMinimumCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-009", "PR", [](TestContext& context) {
        const TypeLayout type{1, 1};
        const ArrayLayout array{1, 1, 1, 1};
        const StructLayout structure{1, 1, {}};
        context.expect(type.size == 1 && type.alignment == 1, "TF-TGT-009 TypeLayout fields must be exact");
        context.expect(array.size == 1 && array.alignment == 1, "TF-TGT-009 ArrayLayout size and alignment must be exact");
        context.expect(array.stride == 1 && array.count == 1, "TF-TGT-009 ArrayLayout stride and count must be exact");
        context.expect(structure.size == 1 && structure.alignment == 1, "TF-TGT-009 StructLayout scalars must be exact");
        context.expect(structure.fields.empty(), "TF-TGT-009 StructLayout fields must remain empty");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-010", "PR", [](TestContext& context) {
        const TypeLayout type{16, 16};
        const ArrayLayout array{96, 16, 32, 3};
        const StructFieldLayout field{32, 16, 16, 4, 12};
        const StructLayout structure{64, 16, {field}};
        context.expect(type.size == 16 && type.alignment == 16, "TF-TGT-010 TypeLayout must preserve 16/16");
        context.expect(array.size == 96 && array.alignment == 16, "TF-TGT-010 ArrayLayout size and alignment must be exact");
        context.expect(array.stride == 32 && array.count == 3, "TF-TGT-010 ArrayLayout stride and count must be exact");
        context.expect(structure.size == 64 && structure.alignment == 16, "TF-TGT-010 StructLayout scalars must be exact");
        context.expect(structure.fields.size() == 1, "TF-TGT-010 StructLayout must own one field");
        context.expect(field.offset == 32 && field.size == 16 && field.alignment == 16, "TF-TGT-010 field scalars must be exact");
        context.expect(field.bitOffset == 4 && field.bitWidth == 12, "TF-TGT-010 bit metadata must be exact");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-011", "PR", [](TestContext& context) {
        const TypeLayout type{};
        const ArrayLayout array{};
        const StructFieldLayout field{};
        const StructLayout structure{};
        context.expect(type.size == 0 && type.alignment == 0, "TF-TGT-011 value-init TypeLayout must be zero");
        context.expect(array.size == 0 && array.alignment == 0 && array.stride == 0 && array.count == 0, "TF-TGT-011 value-init ArrayLayout must be zero");
        context.expect(field.offset == 0 && field.size == 0 && field.alignment == 0, "TF-TGT-011 value-init field scalars must be zero");
        context.expect(!field.bitOffset.has_value() && !field.bitWidth.has_value(), "TF-TGT-011 value-init optionals must be empty");
        context.expect(structure.size == 0 && structure.alignment == 0 && structure.fields.empty(), "TF-TGT-011 value-init StructLayout must be empty");
    });
}

void registerPassiveInvalidCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-012", "PR", [](TestContext& context) {
        const TypeLayout type{0, 0};
        const ArrayLayout array{7, 0, 0, 0};
        const StructFieldLayout field{9, 0, 0, std::nullopt, 63};
        context.expect(type.size == 0 && type.alignment == 0, "TF-TGT-012 passive TypeLayout must preserve zero fields");
        context.expect(array.size == 7 && array.alignment == 0 && array.stride == 0 && array.count == 0, "TF-TGT-012 passive ArrayLayout must preserve incoherent fields");
        context.expect(field.offset == 9 && field.size == 0 && field.alignment == 0, "TF-TGT-012 passive field must preserve scalars");
        context.expect(!field.bitOffset.has_value() && field.bitWidth == 63, "TF-TGT-012 passive field must preserve independent optionals");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-013", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TypeLayout zero{0, 0};
        const TypeLayout maximumType{maximum, maximum};
        const ArrayLayout maximumArray{maximum, maximum, maximum, maximum};
        const StructFieldLayout maximumField{maximum, maximum, maximum, maximum, maximum};
        const StructLayout empty{0, 1, {}};
        context.expect(zero.size == 0 && zero.alignment == 0, "TF-TGT-013 zero boundary must be preserved");
        context.expect(maximumType.size == maximum && maximumType.alignment == maximum, "TF-TGT-013 maximum TypeLayout must be preserved");
        context.expect(maximumArray.size == maximum && maximumArray.count == maximum, "TF-TGT-013 maximum ArrayLayout must be preserved");
        context.expect(maximumField.offset == maximum && maximumField.bitWidth == maximum, "TF-TGT-013 maximum field values must be preserved");
        context.expect(empty.size == 0 && empty.alignment == 1 && empty.fields.empty(), "TF-TGT-013 empty vector boundary must be preserved");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-014", "PR", [](TestContext& context) {
        const StructFieldLayout emptyOptionals{0, 1, 1, std::nullopt, std::nullopt};
        const StructFieldLayout bothOptionals{8, 4, 4, 3, 5};
        const StructFieldLayout widthOnly{16, 8, 8, std::nullopt, 7};
        const StructLayout structure{24, 8, {emptyOptionals, bothOptionals, widthOnly}};
        context.expect(structure.fields.size() == 3, "TF-TGT-014 variant matrix must contain three fields");
        context.expect(!structure.fields[0].bitOffset && !structure.fields[0].bitWidth, "TF-TGT-014 first optionals must be empty");
        context.expect(structure.fields[1].bitOffset == 3 && structure.fields[1].bitWidth == 5, "TF-TGT-014 second optionals must be engaged");
        context.expect(!structure.fields[2].bitOffset && structure.fields[2].bitWidth == 7, "TF-TGT-014 width-only state must be preserved");
        context.expect(structure.fields[0].offset == 0 && structure.fields[1].offset == 8 && structure.fields[2].offset == 16, "TF-TGT-014 field order must be exact");
    });
}

void registerOwnershipCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-015", "PR", [](TestContext& context) {
        StructLayout source{16, 8, {{0, 8, 8, std::nullopt, std::nullopt}, {8, 8, 8, 0, 64}}};
        const StructLayout copy = source;
        source.size = 8;
        source.fields[0].offset = 99;
        source.fields.pop_back();
        context.expect(copy.size == 16 && copy.alignment == 8, "TF-TGT-015 copied scalars must remain exact");
        context.expect(copy.fields.size() == 2, "TF-TGT-015 copy must own an independent vector");
        context.expect(copy.fields[0].offset == 0 && copy.fields[1].offset == 8, "TF-TGT-015 copied field order must remain exact");
        context.expect(copy.fields[1].bitOffset == 0 && copy.fields[1].bitWidth == 64, "TF-TGT-015 copied optionals must remain exact");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addLayoutValueCase(registry, "TF-TGT-016", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t fieldCount = 65536;
        std::vector<StructFieldLayout> fields;
        fields.reserve(fieldCount);
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < fieldCount; ++index)
        {
            const std::uint64_t offset = static_cast<std::uint64_t>(index) * 8;
            fields.push_back({offset, 8, 8, std::nullopt, std::nullopt});
            checksum += offset;
        }
        const StructLayout structure{fieldCount * 8, 8, std::move(fields)};
        context.expect(structure.fields.size() == fieldCount, "TF-TGT-016 must preserve all 65,536 fields");
        context.expect(structure.fields.front().offset == 0, "TF-TGT-016 first field must remain exact");
        context.expect(structure.fields[fieldCount / 2].offset == 262144, "TF-TGT-016 middle field must remain exact");
        context.expect(structure.fields.back().offset == 524280, "TF-TGT-016 last field must remain exact");
        context.expect(checksum == 17179607040ULL, "TF-TGT-016 offset checksum must remain exact");
    });
}
}

void registerTargetLayoutValuesModule(TestRegistry& registry)
{
    registerMinimumCase(registry);
    registerRepresentativeCase(registry);
    registerNoPrerequisiteCase(registry);
    registerPassiveInvalidCase(registry);
    registerBoundaryCase(registry);
    registerVariantMatrixCase(registry);
    registerOwnershipCase(registry);
    registerScaleCase(registry);
}
}
