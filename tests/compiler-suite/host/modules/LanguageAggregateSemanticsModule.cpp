#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/AggregateSemantics.h"

#include <array>
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
    registry.add(TestCase{std::move(id), "CL-LANG-002", "Host.Language", "unit",
        "PR", {"language", "aggregate"}, std::move(function)});
}

void registerTupleIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-015", [](TestContext& context) {
        const std::vector<TupleElement> empty;
        const std::array one{TupleElement{"i32", "value"}};
        const std::array many{
            TupleElement{"builtin:i32", "left"}, TupleElement{"pkg:Thing", "right"},
            TupleElement{"", "emptyType"}};
        const std::array renamed{
            TupleElement{"builtin:i32", "alpha"}, TupleElement{"pkg:Thing", "beta"},
            TupleElement{"", "gamma"}};
        context.expect(tupleIdentity(empty) == "tuple:()",
            "TL-LANG-015 an empty tuple must have the exact empty identity");
        context.expect(tupleIdentity(one) == "tuple:(3:i32)",
            "TL-LANG-015 a single element must use the exact length-prefixed identity");
        const std::string expected = "tuple:(11:builtin:i32,9:pkg:Thing,0:)";
        context.expect(tupleIdentity(many) == expected && tupleIdentity(renamed) == expected,
            "TL-LANG-015 tuple identity must preserve type order and exclude element names");
    });
}

void registerTupleNamePolicyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-016", [](TestContext& context) {
        for (std::size_t repetition = 0; repetition < 16; ++repetition)
            context.expect(!tupleNamesAffectIdentity(),
                "TL-LANG-016 tuple names must never affect identity");
    });
}

void registerTupleConversionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-017", [](TestContext& context) {
        const std::vector<std::string> empty;
        const std::array i32{std::string{"builtin:i32"}};
        const std::array f64{std::string{"builtin:f64"}};
        const std::array stringType{std::string{"builtin:string"}};
        const std::array userA{std::string{"app:A"}};
        const std::array userB{std::string{"app:B"}};
        const std::array two{std::string{"builtin:i32"}, std::string{"app:A"}};
        context.expect(tupleConversionExists(empty, empty),
            "TL-LANG-017 empty tuples must convert by identity");
        context.expect(!tupleConversionExists(empty, i32),
            "TL-LANG-017 different tuple arity must reject conversion");
        context.expect(tupleConversionExists(i32, i32) && tupleConversionExists(userA, userA),
            "TL-LANG-017 equal element types must convert exactly");
        context.expect(tupleConversionExists(i32, f64),
            "TL-LANG-017 builtin numeric elements must allow the current numeric conversion");
        context.expect(!tupleConversionExists(stringType, i32) &&
                !tupleConversionExists(i32, stringType) && !tupleConversionExists(userA, userB),
            "TL-LANG-017 strings and arbitrary user types must not gain numeric conversion");
        context.expect(!tupleConversionExists(two,
                std::array{std::string{"builtin:f64"}, std::string{"app:B"}}),
            "TL-LANG-017 one invalid element must reject the complete tuple conversion");
    });
}

void registerAggregateLayoutCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-018", [](TestContext& context) {
        const std::vector<AggregateFieldLayout> empty;
        const auto emptyLayout = computeWindowsX64AggregateLayout(empty);
        context.expect(emptyLayout.has_value() && emptyLayout->offsets.empty() &&
                emptyLayout->size == 0 && emptyLayout->alignment == 1,
            "TL-LANG-018 an empty aggregate must have the exact neutral layout");

        const std::array fields{
            AggregateFieldLayout{1, 1}, AggregateFieldLayout{4, 4}, AggregateFieldLayout{2, 2}};
        const auto layout = computeWindowsX64AggregateLayout(fields);
        const std::vector<std::size_t> expectedOffsets{0, 4, 8};
        context.expect(layout.has_value() && layout->offsets == expectedOffsets &&
                layout->size == 12 && layout->alignment == 4,
            "TL-LANG-018 mixed fields must preserve source order, padding, size and alignment");

        const std::array wide{
            AggregateFieldLayout{3, 1}, AggregateFieldLayout{8, 8}, AggregateFieldLayout{1, 16}};
        const auto wideLayout = computeWindowsX64AggregateLayout(wide);
        const std::vector<std::size_t> wideOffsets{0, 8, 16};
        context.expect(wideLayout.has_value() && wideLayout->offsets == wideOffsets &&
                wideLayout->size == 32 && wideLayout->alignment == 16,
            "TL-LANG-018 final size must round up to the maximum valid alignment");
    });
}

void registerInvalidAggregateLayoutCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LANG-019",
        "CL-LANG-002",
        "Host.Language",
        "unit",
        "PR",
        {"language", "aggregate"},
        [](TestContext& context) {
            const std::array zeroSize{AggregateFieldLayout{0, 1}};
            context.expect(!computeWindowsX64AggregateLayout(zeroSize).has_value(),
                "TL-LANG-019 a zero-size field must return nullopt");

            const std::array zeroAlignment{AggregateFieldLayout{1, 0}};
            context.expect(!computeWindowsX64AggregateLayout(zeroAlignment).has_value(),
                "TL-LANG-019 a zero field alignment must return nullopt");

            const std::array nonPowerOfTwoAlignment{AggregateFieldLayout{1, 3}};
            context.expect(!computeWindowsX64AggregateLayout(nonPowerOfTwoAlignment).has_value(),
                "TL-LANG-019 a non-power-of-two field alignment must return nullopt");

            const std::array excessiveAlignment{AggregateFieldLayout{1, 32}};
            context.expect(!computeWindowsX64AggregateLayout(excessiveAlignment).has_value(),
                "TL-LANG-019 an alignment above the Windows x64 limit must return nullopt");

            const std::size_t maximum = std::numeric_limits<std::size_t>::max();
            const std::array fieldEndOverflow{
                AggregateFieldLayout{maximum, 1}, AggregateFieldLayout{1, 1}};
            context.expect(!computeWindowsX64AggregateLayout(fieldEndOverflow).has_value(),
                "TL-LANG-019 field-end overflow must return nullopt instead of a wrapped layout");

            const std::array alignedOffsetOverflow{
                AggregateFieldLayout{maximum - 7U, 1}, AggregateFieldLayout{1, 16}};
            context.expect(!computeWindowsX64AggregateLayout(alignedOffsetOverflow).has_value(),
                "TL-LANG-019 aligned-offset overflow must return nullopt instead of a partial layout");

            const std::array finalPaddingOverflow{
                AggregateFieldLayout{maximum - 7U, 16}};
            context.expect(!computeWindowsX64AggregateLayout(finalPaddingOverflow).has_value(),
                "TL-LANG-019 final-padding overflow must return nullopt instead of a wrapped size");
        },
    });
}

void registerAggregateReturnCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-020", [](TestContext& context) {
        for (std::size_t size = 0; size <= 16; ++size)
        {
            const bool direct = size == 1 || size == 2 || size == 4 || size == 8;
            const auto expected = direct ? WindowsX64AggregateReturn::DirectInteger
                                         : WindowsX64AggregateReturn::IndirectSRet;
            context.expect(classifyWindowsX64AggregateReturn(size) == expected,
                "TL-LANG-020 every size from zero through sixteen must match the ABI table");
        }
        context.expect(classifyWindowsX64AggregateReturn(std::numeric_limits<std::size_t>::max()) ==
                WindowsX64AggregateReturn::IndirectSRet,
            "TL-LANG-020 the maximum size must use indirect sret");
    });
}

void registerDeconstructionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-021", [](TestContext& context) {
        std::string error = "stale";
        const std::vector<DeconstructionBinding> empty;
        context.expect(validateDeconstructionBindings(empty, error) && error.empty(),
            "TL-LANG-021 an empty binding list must succeed and clear stale errors");
        const std::array valid{
            DeconstructionBinding{"value", DeconstructionBindingMode::Value},
            DeconstructionBinding{"reference", DeconstructionBindingMode::Reference},
            DeconstructionBinding{"moved", DeconstructionBindingMode::Move}};
        context.expect(validateDeconstructionBindings(valid, error) && error.empty(),
            "TL-LANG-021 distinct named bindings in every active mode must succeed");
        const std::array discarded{
            DeconstructionBinding{"same", DeconstructionBindingMode::Discard},
            DeconstructionBinding{"same", DeconstructionBindingMode::Value},
            DeconstructionBinding{"_", DeconstructionBindingMode::Value},
            DeconstructionBinding{"_", DeconstructionBindingMode::Reference}};
        context.expect(validateDeconstructionBindings(discarded, error) && error.empty(),
            "TL-LANG-021 discard and underscore must not reserve a binding name");
        const std::array unnamed{DeconstructionBinding{"", DeconstructionBindingMode::Move}};
        context.expect(!validateDeconstructionBindings(unnamed, error) &&
                error == "deconstruction binding requires a name or discard",
            "TL-LANG-021 an unnamed active binding must use the exact error");
        const std::array duplicate{
            DeconstructionBinding{"dup", DeconstructionBindingMode::Value},
            DeconstructionBinding{"dup", DeconstructionBindingMode::Reference}};
        context.expect(!validateDeconstructionBindings(duplicate, error) &&
                error == "duplicate deconstruction binding 'dup'",
            "TL-LANG-021 a duplicate active name must use the exact error");
    });
}

void registerRecordDeriveSetCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-022", [](TestContext& context) {
        const std::array derives{
            RecordDerive::Equality, RecordDerive::Hash, RecordDerive::Print, RecordDerive::Clone};
        const RecordDeriveSet empty;
        for (const RecordDerive derive : derives)
            context.expect(!empty.contains(derive),
                "TL-LANG-022 a default set must contain no declared derive");
        for (const RecordDerive selected : derives)
        {
            const RecordDeriveSet isolated(selected);
            for (const RecordDerive candidate : derives)
                context.expect(isolated.contains(candidate) == (candidate == selected),
                    "TL-LANG-022 an isolated set must contain exactly its selected derive");
        }
        const RecordDeriveSet equality(RecordDerive::Equality);
        const auto equalityHash = equality.with(RecordDerive::Hash);
        const auto all = equalityHash.with(RecordDerive::Print).with(RecordDerive::Clone);
        context.expect(equality.contains(RecordDerive::Equality) &&
                !equality.contains(RecordDerive::Hash),
            "TL-LANG-022 with must leave the source set immutable");
        context.expect(equalityHash.contains(RecordDerive::Equality) &&
                equalityHash.contains(RecordDerive::Hash) &&
                !equalityHash.contains(RecordDerive::Print),
            "TL-LANG-022 with must union exactly one requested bit");
        for (const RecordDerive derive : derives)
            context.expect(all.contains(derive),
                "TL-LANG-022 the complete union must contain every declared derive");
    });
}

void registerPromotedFieldCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-023", [](TestContext& context) {
        std::string error = "stale";
        const std::vector<PromotedField> empty;
        context.expect(validatePromotedFields(empty, error) && error.empty(),
            "TL-LANG-023 an empty promoted-field list must clear stale errors");
        const std::array valid{
            PromotedField{"x", "builtin:i32", true, false},
            PromotedField{"y", "builtin:i64", false, true}};
        context.expect(validatePromotedFields(valid, error) && error.empty(),
            "TL-LANG-023 complete unique promoted fields must be accepted");
        const std::array missingName{PromotedField{"", "builtin:i32", false, false}};
        context.expect(!validatePromotedFields(missingName, error) &&
                error == "promoted field requires an explicit name and type",
            "TL-LANG-023 a missing name must use the exact required-field error");
        const std::array missingType{PromotedField{"x", "", false, false}};
        context.expect(!validatePromotedFields(missingType, error) &&
                error == "promoted field requires an explicit name and type",
            "TL-LANG-023 a missing type must use the exact required-field error");
        const std::array duplicate{
            PromotedField{"same", "builtin:i32", false, false},
            PromotedField{"same", "builtin:i64", true, true}};
        context.expect(!validatePromotedFields(duplicate, error) &&
                error == "duplicate promoted field 'same'",
            "TL-LANG-023 a duplicate name must use the exact duplicate error");
    });
}

void registerAggregateSpreadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-024", [](TestContext& context) {
        std::string error = "stale";
        const std::vector<std::string> empty;
        context.expect(validateAggregateSpread(0, empty, error) && error.empty(),
            "TL-LANG-024 zero sources and fields must succeed and clear stale errors");
        const std::array unique{std::string{"x"}, std::string{"y"}, std::string{""}};
        context.expect(validateAggregateSpread(1, unique, error) && error.empty(),
            "TL-LANG-024 one source and unique explicit field spellings must succeed");
        context.expect(!validateAggregateSpread(2, unique, error) &&
                error == "an aggregate spread accepts exactly one source",
            "TL-LANG-024 multiple sources must have first rejection priority");
        const std::array duplicate{std::string{"x"}, std::string{"x"}};
        context.expect(!validateAggregateSpread(1, duplicate, error) &&
                error == "duplicate explicit aggregate field 'x'",
            "TL-LANG-024 a duplicate explicit field must use the exact error");
    });
}
}

void registerLanguageAggregateSemanticsModule(TestRegistry& registry)
{
    registerTupleIdentityCase(registry);
    registerTupleNamePolicyCase(registry);
    registerTupleConversionCase(registry);
    registerAggregateLayoutCase(registry);
    registerInvalidAggregateLayoutCase(registry);
    registerAggregateReturnCase(registry);
    registerDeconstructionCase(registry);
    registerRecordDeriveSetCase(registry);
    registerPromotedFieldCase(registry);
    registerAggregateSpreadCase(registry);
}
}
