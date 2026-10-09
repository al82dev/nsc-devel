#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/AbiSemantics.h"

#include <array>
#include <cstdint>
#include <limits>
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
    registry.add(TestCase{std::move(id), "CL-LANG-001", "Host.Language", "unit",
        "PR", {"language", "abi"}, std::move(function)});
}

void registerValidRepresentationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-001", [](TestContext& context) {
        for (const std::size_t value : std::array<std::size_t, 5>{1, 2, 4, 8, 16})
        {
            std::string error = "stale";
            context.expect(validateRepresentation({Representation::Native, value, std::nullopt, 0}, error),
                "TL-LANG-001 each supported packing must be accepted");
            context.expect(error.empty(), "TL-LANG-001 accepted packing must clear stale errors");
            error = "stale";
            context.expect(validateRepresentation({Representation::C, std::nullopt, value, 0}, error),
                "TL-LANG-001 each supported alignment must be accepted");
            context.expect(error.empty(), "TL-LANG-001 accepted alignment must clear stale errors");
        }
        std::string error = "stale";
        context.expect(validateRepresentation(
                {Representation::Transparent, std::nullopt, std::nullopt, 1}, error),
            "TL-LANG-001 transparent representation with one field must be accepted");
        context.expect(error.empty(), "TL-LANG-001 accepted transparent representation must clear errors");
    });
}

void registerInvalidRepresentationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-002", [](TestContext& context) {
        for (const std::size_t value : std::array<std::size_t, 3>{0, 3, 32})
        {
            std::string error;
            const RepresentationContract packing{Representation::Native, value, std::nullopt, 0};
            context.expect(!validateRepresentation(packing, error) &&
                    error == "packing must be a power of two no larger than 16 on Windows x64" &&
                    packing.packing == value,
                "TL-LANG-002 invalid packing must fail exactly without normalizing input");
            const RepresentationContract alignment{Representation::C, std::nullopt, value, 0};
            context.expect(!validateRepresentation(alignment, error) &&
                    error == "alignment must be a power of two no larger than 16 on Windows x64" &&
                    alignment.alignment == value,
                "TL-LANG-002 invalid alignment must fail exactly without normalizing input");
        }
        for (const std::size_t fields : std::array<std::size_t, 2>{0, 2})
        {
            std::string error;
            const RepresentationContract contract{
                Representation::Transparent, std::nullopt, std::nullopt, fields};
            context.expect(!validateRepresentation(contract, error) &&
                    error == "repr(transparent) requires exactly one non-zero-sized field" &&
                    contract.nonZeroFields == fields,
                "TL-LANG-002 transparent field cardinality must fail exactly without mutation");
        }
        std::string error;
        context.expect(!validateRepresentation({Representation::Transparent, 2, std::nullopt, 1}, error) &&
                error == "repr(transparent) cannot alter packing",
            "TL-LANG-002 transparent packing must use its distinct exact error");
    });
}

void registerBitFieldMaskCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-003", [](TestContext& context) {
        struct Valid final { BitFieldContract contract; std::uint64_t mask; };
        const std::array valid{
            Valid{{8, 0, 1, false, false}, 0x1},
            Valid{{8, 7, 1, false, false}, 0x80},
            Valid{{16, 4, 4, false, false}, 0xf0},
            Valid{{32, 0, 32, false, false}, 0xffffffffULL},
            Valid{{64, 0, 64, false, false}, std::numeric_limits<std::uint64_t>::max()},
            Valid{{64, 63, 1, false, false}, std::uint64_t{1} << 63U},
        };
        for (const Valid& item : valid)
            context.expect(bitFieldMask(item.contract) == item.mask,
                "TL-LANG-003 each valid bit-field boundary must produce the exact mask");
        const std::array invalid{
            BitFieldContract{7, 0, 1, false, false},
            BitFieldContract{128, 0, 1, false, false},
            BitFieldContract{8, 0, 0, false, false},
            BitFieldContract{8, 0, 9, false, false},
            BitFieldContract{8, 7, 2, false, false},
        };
        for (const BitFieldContract& item : invalid)
            context.expect(!bitFieldMask(item).has_value(),
                "TL-LANG-003 each invalid storage or range must return nullopt");
    });
}

std::vector<std::pair<PointerContract, PointerContract>> pointerMatrix()
{
    const PointerContract base{};
    std::vector<std::pair<PointerContract, PointerContract>> pairs;
    pairs.emplace_back(base, base);
    PointerContract source = base;
    PointerContract target = base;
    target.nullable = false;
    pairs.emplace_back(source, target);
    source.nullable = false;
    target = base;
    pairs.emplace_back(source, target);
    source = base;
    source.mutablePointee = false;
    target = base;
    pairs.emplace_back(source, target);
    source = base;
    target.mutablePointee = false;
    pairs.emplace_back(source, target);
    source = base;
    source.hasProvenance = false;
    target = base;
    pairs.emplace_back(source, target);
    source = base;
    target.hasProvenance = false;
    pairs.emplace_back(source, target);
    source = base;
    source.knownAlignment = 4;
    target = base;
    target.knownAlignment = 8;
    pairs.emplace_back(source, target);
    source.knownAlignment = 8;
    target.knownAlignment = 4;
    pairs.emplace_back(source, target);
    source = base;
    target = base;
    target.addressSpace = 1;
    pairs.emplace_back(source, target);
    return pairs;
}

void registerPointerAssignabilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-004", [](TestContext& context) {
        const std::array expected{true, false, true, false, true, false, true, false, true, false};
        const auto pairs = pointerMatrix();
        for (std::size_t index = 0; index < pairs.size(); ++index)
            context.expect(pointerContractImplicitlyAssignable(pairs[index].first, pairs[index].second) ==
                    expected[index],
                "TL-LANG-004 pointer assignability matrix cell must match the contract");
    });
}

void registerPointerUnsafeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-005", [](TestContext& context) {
        for (const auto& [source, target] : pointerMatrix())
            context.expect(pointerCastRequiresUnsafe(source, target) ==
                    !pointerContractImplicitlyAssignable(source, target),
                "TL-LANG-005 restricted pointer changes must be the exact assignability complement");
    });
}

void registerCallingConventionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-006", [](TestContext& context) {
        const std::array conventions{
            AbiCallingConvention::Native, AbiCallingConvention::Cdecl, AbiCallingConvention::StdCall,
            AbiCallingConvention::VectorCall, AbiCallingConvention::Win64};
        for (const AbiCallingConvention convention : conventions)
        {
            context.expect(callingConventionSupportedOnWindowsX64(convention, false),
                "TL-LANG-006 every declared convention must support non-variadic calls");
            const bool expectedVariadic = convention == AbiCallingConvention::Cdecl ||
                convention == AbiCallingConvention::Win64;
            context.expect(callingConventionSupportedOnWindowsX64(convention, true) == expectedVariadic,
                "TL-LANG-006 only cdecl and Win64 must support variadic calls");
        }
    });
}

void registerUnsafeOperationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-007", [](TestContext& context) {
        const std::array operations{
            UnsafeOperation::RawDereference, UnsafeOperation::UnionInactiveRead,
            UnsafeOperation::IntegerPointerCast, UnsafeOperation::InlineAssembly, UnsafeOperation::FfiCall};
        for (const UnsafeOperation operation : operations)
            context.expect(unsafeOperationRequiresContext(operation),
                "TL-LANG-007 every declared unsafe operation must require context");
        // The public helper deliberately accepts the enum by value and its current fallback is part of the plan.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        context.expect(unsafeOperationRequiresContext(static_cast<UnsafeOperation>(999)),
            "TL-LANG-007 the current external enum fallback must require context");
    });
}

void registerUnsafeBlockCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-008", [](TestContext& context) {
        const std::array counts{
            std::size_t{0}, std::size_t{8}, std::size_t{9}, std::numeric_limits<std::size_t>::max()};
        for (const std::size_t count : counts)
        {
            context.expect(unsafeBlockTooBroad(count, false) == (count > 8),
                "TL-LANG-008 an unexplained block must use the strict greater-than-eight threshold");
            context.expect(!unsafeBlockTooBroad(count, true),
                "TL-LANG-008 an explicit reason must exempt every statement count");
        }
    });
}

void registerFfiParameterCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-009", [](TestContext& context) {
        std::string error = "stale";
        FfiParameterContract parameter;
        context.expect(validateFfiParameter(parameter, 0, error) && error.empty(),
            "TL-LANG-009 an ordinary parameter must be accepted in an empty signature model");
        parameter.lengthParameter = 0;
        context.expect(validateFfiParameter(parameter, 1, error) && error.empty(),
            "TL-LANG-009 the first valid length index must be accepted");
        parameter.lengthParameter = 2;
        context.expect(validateFfiParameter(parameter, 3, error) && error.empty(),
            "TL-LANG-009 the final valid length index must be accepted");
        parameter.lengthParameter = 3;
        context.expect(!validateFfiParameter(parameter, 3, error) &&
                error == "FFI buffer length parameter index is out of range" &&
                parameter.lengthParameter == 3,
            "TL-LANG-009 an out-of-range length index must fail exactly without mutation");
        parameter = {};
        parameter.callbackMayEscape = true;
        context.expect(!validateFfiParameter(parameter, 1, error) &&
                error == "escaping callback requires callback shape and transferred/owned lifetime",
            "TL-LANG-009 an escaping non-callback must fail with the exact callback error");
        parameter.callback = true;
        context.expect(!validateFfiParameter(parameter, 1, error),
            "TL-LANG-009 an escaping borrowed callback must be rejected");
        parameter.ownership = FfiOwnership::Owned;
        context.expect(validateFfiParameter(parameter, 1, error) && error.empty(),
            "TL-LANG-009 an escaping owned callback must be accepted");
        parameter.ownership = FfiOwnership::Transferred;
        context.expect(validateFfiParameter(parameter, 1, error) && error.empty(),
            "TL-LANG-009 an escaping transferred callback must be accepted");
    });
}

void registerFfiUnwindCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-010", [](TestContext& context) {
        for (std::size_t repetition = 0; repetition < 16; ++repetition)
            context.expect(!ffiMayUnwindAcrossBoundary(),
                "TL-LANG-010 FFI unwind policy must remain false and stateless on every call");
    });
}

void registerSimdValidationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-011", [](TestContext& context) {
        std::string error;
        for (const std::size_t laneBits : std::array<std::size_t, 4>{8, 16, 32, 64})
            context.expect(validateSimd({laneBits, 2, false}, error) && error.empty(),
                "TL-LANG-011 every supported lane width with two lanes must be accepted");
        context.expect(validateSimd({64, 8, false}, error) && error.empty(),
            "TL-LANG-011 a total width of exactly 512 bits must be accepted");
        const std::array invalid{
            SimdContract{0, 2, false}, SimdContract{4, 2, false}, SimdContract{24, 2, false},
            SimdContract{128, 2, false}, SimdContract{8, 0, false}, SimdContract{8, 1, false},
            SimdContract{8, 3, false}, SimdContract{64, 16, false}, SimdContract{9, 57, false}};
        for (const SimdContract& contract : invalid)
            context.expect(!validateSimd(contract, error) &&
                    error == "portable SIMD requires power-of-two lanes with total width <= 512 bits",
                "TL-LANG-011 every invalid lane matrix cell must use the exact validation error");
    });
}

void registerSimdRoundTripCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-012", [](TestContext& context) {
        const std::array<std::string_view, 10> laneTypes{
            "builtin:sbyte", "builtin:byte", "builtin:short", "builtin:ushort", "builtin:int",
            "builtin:uint", "builtin:long", "builtin:ulong", "builtin:float", "builtin:double"};
        for (const std::string_view laneType : laneTypes)
        {
            for (const bool mask : std::array{false, true})
            {
                std::string error;
                const auto type = portableSimdType(laneType, 4, mask, error);
                context.expect(type.has_value() && error.empty(),
                    "TL-LANG-012 every supported lane type and shape must construct");
                if (!type.has_value()) continue;
                const auto parsed = parsePortableSimdType(portableSimdTypeIdentity(*type));
                context.expect(parsed.has_value() && parsed->laneKind == type->laneKind &&
                        parsed->laneTypeKey == type->laneTypeKey && parsed->laneBits == type->laneBits &&
                        parsed->laneCount == type->laneCount && parsed->mask == type->mask,
                    "TL-LANG-012 portable SIMD identity must round-trip every field exactly");
            }
        }
    });
}

void registerInvalidSimdIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-013", [](TestContext& context) {
        const std::array<std::string_view, 12> identities{
            "", "vector<builtin:int,4>", "xsimd<builtin:int,4>", "simd<builtin:int,4",
            "simd<builtin:int>", "simd<,4>", "simd<builtin:int,>", "simd<builtin:int,x>",
            "simd<builtin:int,1>", "simd<unknown,4>", "simd<builtin:int,4>trailing",
            "simd<builtin:int,184467440737095516160>"};
        for (const std::string_view identity : identities)
            context.expect(!parsePortableSimdType(identity).has_value(),
                "TL-LANG-013 malformed identities must return nullopt without partial acceptance");
    });
}

void registerShuffleAndMaskCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-014", [](TestContext& context) {
        std::string error = "stale";
        context.expect(validateSimdShuffle(4, "0,1,2,3", error) && error.empty(),
            "TL-LANG-014 a complete ordered shuffle must be accepted");
        context.expect(validateSimdShuffle(4, "3,2,1,0", error) && error.empty(),
            "TL-LANG-014 a complete reversed shuffle must preserve supplied order");
        context.expect(!validateSimdShuffle(4, "0,1", error) &&
                error == "SIMD shuffle must specify exactly one index per result lane",
            "TL-LANG-014 a missing-index shuffle must use the exact cardinality error");
        context.expect(!validateSimdShuffle(4, "0,1,2,3,0", error) &&
                error == "SIMD shuffle must specify exactly one index per result lane",
            "TL-LANG-014 an extra-index shuffle must use the exact cardinality error");
        context.expect(!validateSimdShuffle(4, "0,1,2,4", error) &&
                error == "SIMD shuffle indices must be compile-time lane indices in range",
            "TL-LANG-014 an out-of-range shuffle must use the exact index error");
        context.expect(!validateSimdShuffle(4, "0,1,2,", error) &&
                error == "SIMD shuffle must specify exactly one index per result lane",
            "TL-LANG-014 a trailing separator must use the current cardinality error");
        context.expect(validateSimdShuffle(0, "", error) && error.empty(),
            "TL-LANG-014 the current zero-lane empty shuffle fallback must be characterized");
        context.expect(simdMaskLane(false, 8) == 0 && simdMaskLane(true, 0) == 0 &&
                simdMaskLane(true, 1) == 1 && simdMaskLane(true, 8) == 0xff &&
                simdMaskLane(true, 64) == std::numeric_limits<std::uint64_t>::max() &&
                simdMaskLane(true, 65) == 0,
            "TL-LANG-014 mask lanes must produce zero or exactly all selected lane bits");
        context.expect(simdScalarFallbackPreservesSemantics(),
            "TL-LANG-014 scalar fallback must preserve semantics by contract");
    });
}
}

void registerLanguageAbiSemanticsModule(TestRegistry& registry)
{
    registerValidRepresentationCase(registry);
    registerInvalidRepresentationCase(registry);
    registerBitFieldMaskCase(registry);
    registerPointerAssignabilityCase(registry);
    registerPointerUnsafeCase(registry);
    registerCallingConventionCase(registry);
    registerUnsafeOperationCase(registry);
    registerUnsafeBlockCase(registry);
    registerFfiParameterCase(registry);
    registerFfiUnwindCase(registry);
    registerSimdValidationCase(registry);
    registerSimdRoundTripCase(registry);
    registerInvalidSimdIdentityCase(registry);
    registerShuffleAndMaskCase(registry);
}
}
