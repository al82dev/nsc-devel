#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/NumericSemantics.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-016", "Host.Language", "unit", std::move(profile),
        {"language", "numeric"}, std::move(function)});
}

bool expectedLossless(NumericType source, NumericType target)
{
    if (source.kind == NumericKind::Floating || target.kind == NumericKind::Floating)
        return source.kind == target.kind && target.bits >= source.bits;
    if (source.isSigned == target.isSigned) return target.bits >= source.bits;
    return !source.isSigned && target.isSigned && target.bits > source.bits;
}

bool sameDecision(const NumericConversionDecision& actual, NumericConversionKind kind,
    unsigned rank, bool implicit, bool lossless, bool requiresPolicy)
{
    return actual.kind == kind && actual.overloadRank == rank && actual.implicit == implicit &&
        actual.lossless == lossless && actual.requiresOverflowPolicy == requiresPolicy;
}

StrongNumericDefinition validDefinition()
{
    StrongNumericDefinition result;
    result.identity = "Physics.Meters";
    result.representation = {NumericKind::Floating, 32, true};
    result.reprTransparent = true;
    result.ffiExported = true;
    result.derivesCopy = true;
    result.derivesMove = true;
    result.constGenericArity = 1;
    result.operators = {StrongNumericOperator::Add, StrongNumericOperator::Subtract,
        StrongNumericOperator::Compare};
    return result;
}

void registerLosslessMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-204", "PR", [](TestContext& context) {
        constexpr std::array<NumericType, 12> types = {{
            {NumericKind::Integer, 8, true}, {NumericKind::Integer, 8, false},
            {NumericKind::Integer, 16, true}, {NumericKind::Integer, 16, false},
            {NumericKind::Integer, 32, true}, {NumericKind::Integer, 32, false},
            {NumericKind::Integer, 64, true}, {NumericKind::Integer, 64, false},
            {NumericKind::Floating, 32, true}, {NumericKind::Floating, 64, true},
            {NumericKind::Enum, 8, false}, {NumericKind::Enum, 32, true},
        }};
        for (const NumericType source : types)
            for (const NumericType target : types)
                context.expect(implicitNumericConversionIsLossless(source, target) ==
                        expectedLossless(source, target),
                    "TL-LANG-204 every representative NumericType pair must match the lossless matrix");
    });
}

void registerConversionClassificationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-205", "PR", [](TestContext& context) {
        const NumericType i8{NumericKind::Integer, 8, true};
        const NumericType u8{NumericKind::Integer, 8, false};
        const NumericType i16{NumericKind::Integer, 16, true};
        const NumericType u16{NumericKind::Integer, 16, false};
        const NumericType i32{NumericKind::Integer, 32, true};
        const NumericType i64{NumericKind::Integer, 64, true};
        const NumericType f32{NumericKind::Floating, 32, true};
        const NumericType f64{NumericKind::Floating, 64, true};
        const NumericType color{NumericKind::Enum, 8, false};
        context.expect(sameDecision(classifyNumericConversion(i32, i32, std::nullopt,
                NumericConversionUse::Overload), NumericConversionKind::Identity, 0, true, true, false),
            "TL-LANG-205 identity conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(i8, i16, std::nullopt,
                NumericConversionUse::Assignment), NumericConversionKind::ImplicitLossless, 1,
                true, true, false),
            "TL-LANG-205 signed widening conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(i16, u16, std::nullopt,
                NumericConversionUse::Assignment), NumericConversionKind::ExplicitSignChange, 3,
                false, false, true),
            "TL-LANG-205 sign-change conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(i64, i32, std::nullopt,
                NumericConversionUse::Assignment), NumericConversionKind::ExplicitNarrowing, 3,
                false, false, true),
            "TL-LANG-205 integer narrowing conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(i32, u8,
                NumericConstant{255, false, false}, NumericConversionUse::ConstGeneric),
                NumericConversionKind::ImplicitConstant, 1, true, true, false),
            "TL-LANG-205 representable constant conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(f32, f64, std::nullopt,
                NumericConversionUse::Assignment), NumericConversionKind::ImplicitLossless, 1,
                true, true, false),
            "TL-LANG-205 float widening conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(f64, i32, std::nullopt,
                NumericConversionUse::Assignment), NumericConversionKind::ExplicitFloatInteger, 3,
                false, false, true),
            "TL-LANG-205 float-to-integer conversion fields must be exact");
        context.expect(sameDecision(classifyNumericConversion(color, u8, std::nullopt,
                NumericConversionUse::EnumUnderlying), NumericConversionKind::ExplicitEnumUnderlying, 4,
                false, false, true),
            "TL-LANG-205 enum-underlying conversion fields must be exact");
        const auto assignment = classifyNumericConversion(u8, i16, std::nullopt,
            NumericConversionUse::Assignment);
        for (const NumericConversionUse use : {NumericConversionUse::Overload,
                NumericConversionUse::ConstGeneric, NumericConversionUse::SimdSplat})
        {
            const auto decision = classifyNumericConversion(u8, i16, std::nullopt, use);
            context.expect(sameDecision(decision, assignment.kind, assignment.overloadRank,
                    assignment.implicit, assignment.lossless, assignment.requiresOverflowPolicy),
                "TL-LANG-205 non-enum uses must share the scalar conversion decision");
        }
    });
}

void registerFloatingContractCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-206", "PR", [](TestContext& context) {
        const auto strict = floatingPointContract(FloatingPointMode::Strict);
        const auto precise = floatingPointContract(FloatingPointMode::Precise);
        const auto fast = floatingPointContract(FloatingPointMode::Fast);
        context.expect(strict.preservesSignedZero && strict.preservesNanSemantics &&
                !strict.allowsContraction && !strict.allowsReassociation && strict.preservesNanPayload &&
                strict.preservesDenormals && strict.constevalMatchesRuntime,
            "TL-LANG-206 strict floating-point flags must all be exact");
        context.expect(precise.preservesSignedZero && precise.preservesNanSemantics &&
                precise.allowsContraction && !precise.allowsReassociation && precise.preservesNanPayload &&
                precise.preservesDenormals && !precise.constevalMatchesRuntime,
            "TL-LANG-206 precise floating-point flags must all be exact");
        context.expect(!fast.preservesSignedZero && !fast.preservesNanSemantics &&
                fast.allowsContraction && fast.allowsReassociation && !fast.preservesNanPayload &&
                fast.preservesDenormals && !fast.constevalMatchesRuntime,
            "TL-LANG-206 fast floating-point flags must all be exact");
    });
}

void registerStrongIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-207", "PR", [](TestContext& context) {
        const StrongNumericType meters{"Physics.Meters", {NumericKind::Integer, 32, true}};
        const StrongNumericType seconds{"Physics.Seconds", {NumericKind::Integer, 32, true}};
        const StrongNumericType otherRepresentation{"Physics.Meters", {NumericKind::Floating, 64, true}};
        context.expect(!strongNumericHasRuntimeWrapper() &&
                strongNumericImplicitlyConverts(meters, meters) &&
                strongNumericImplicitlyConverts(meters, otherRepresentation) &&
                !strongNumericImplicitlyConverts(meters, seconds),
            "TL-LANG-207 strong numerics have no wrapper and convert implicitly only by identity");
    });
}

void registerStrongPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-208", "PR", [](TestContext& context) {
        std::string error;
        const auto definition = validDefinition();
        const auto plan = buildStrongNumericPlan(definition, error);
        context.expect(plan && error.empty() && plan->type.identity == definition.identity &&
                plan->type.representation.kind == NumericKind::Floating && plan->size == 4 &&
                plan->alignment == 4 && plan->constGenericArity == 1 && plan->abiTransparent &&
                plan->zeroRuntimeOverhead && plan->constructionIsExplicit && plan->extractionIsExplicit &&
                plan->derivesCopy && plan->derivesMove && plan->operators == definition.operators,
            "TL-LANG-208 every valid strong-numeric plan field must match the definition and ABI contract");
    });
}

void registerStrongInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-209", "PR", [](TestContext& context) {
        struct Invalid final { StrongNumericDefinition definition; const char* error; };
        auto empty = validDefinition(); empty.identity.clear();
        auto fields = validDefinition(); fields.fieldCount = 2;
        auto width = validDefinition(); width.representation.bits = 7;
        auto transparent = validDefinition(); transparent.reprTransparent = false;
        auto copyMove = validDefinition(); copyMove.derivesMove = false;
        const std::array invalid = {
            Invalid{empty, "strong numeric newtype requires a nominal identity"},
            Invalid{fields, "strong numeric newtype must contain exactly one representation field"},
            Invalid{width, "strong numeric representation requires a fundamental byte-sized numeric type"},
            Invalid{transparent, "FFI-exported strong numeric newtype requires repr(transparent)"},
            Invalid{copyMove, "a Copy strong numeric newtype must also satisfy Move"},
        };
        std::string error;
        for (const Invalid& sample : invalid)
            context.expect(!buildStrongNumericPlan(sample.definition, error) && error == sample.error,
                "TL-LANG-209 each invalid definition must stop at its exact first error");
    });
}

void registerStrongOperatorsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-210", "PR", [](TestContext& context) {
        std::string error;
        auto definition = validDefinition();
        const auto left = buildStrongNumericPlan(definition, error);
        const auto same = buildStrongNumericPlan(definition, error);
        definition.identity = "Physics.Seconds";
        const auto different = buildStrongNumericPlan(definition, error);
        definition.identity = "Physics.Meters";
        definition.constGenericArity = 2;
        const auto arity = buildStrongNumericPlan(definition, error);
        context.expect(left && same && different && arity &&
                strongNumericOperatorAllowed(*left, *same, StrongNumericOperator::Add) &&
                !strongNumericOperatorAllowed(*left, *same, StrongNumericOperator::Multiply) &&
                !strongNumericOperatorAllowed(*left, *different, StrongNumericOperator::Add) &&
                !strongNumericOperatorAllowed(*left, *arity, StrongNumericOperator::Add),
            "TL-LANG-210 declared operators require identical nominal identity and const-generic arity");
    });
}

void registerAllocationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-211", "PR", [](TestContext& context) {
        ConstantMemory memory;
        std::string error;
        const auto first = memory.allocate(1, 1, error);
        const auto second = memory.allocate(16, 8, error);
        context.expect(first && second && first->allocation == 1 && second->allocation == 2 &&
                first->offset == 0 && second->offset == 0 && error.empty(),
            "TL-LANG-211 valid allocations must receive monotonic provenance and zero offset");
        for (const auto& [size, alignment] : {std::pair{0U, 1U}, std::pair{1U, 0U},
                std::pair{1U, 3U}})
            context.expect(!memory.allocate(size, alignment, error) &&
                    error == "constant allocation requires nonzero size and power-of-two alignment",
                "TL-LANG-211 invalid allocation size or alignment must report the exact error");
    });
}

void registerPointerCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-212", "PR", [](TestContext& context) {
        ConstantMemory memory;
        std::string error;
        const auto base = memory.allocate(16, 8, error);
        const auto start = base ? memory.offset(*base, 0, error) : std::nullopt;
        const auto end = base ? memory.offset(*base, 16, error) : std::nullopt;
        context.expect(start && end && memory.validateDereference(*start, 16, error) &&
                !memory.validateDereference(*end, 1, error) &&
                error == "constant dereference is outside its allocation",
            "TL-LANG-212 start and one-past-end pointer rules must be exact");
        context.expect(base && !memory.offset(*base, -1, error) &&
                error == "constant pointer arithmetic escaped its allocation" &&
                !memory.offset(*base, 17, error) &&
                error == "constant pointer arithmetic escaped its allocation",
            "TL-LANG-212 negative and positive escape must fail without offset wrap");
        context.expect(base && memory.deallocate(base->allocation, error) &&
                !memory.offset(*base, 0, error) && error == "constant pointer has no live provenance" &&
                !memory.validateDereference(*base, 1, error) &&
                error == "constant dereference has no live provenance",
            "TL-LANG-212 deallocated pointers must fail both offset and dereference with exact errors");
    });
}

void registerDeallocationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-213", "PR", [](TestContext& context) {
        ConstantMemory memory;
        std::string error;
        const auto allocation = memory.allocate(8, 8, error);
        context.expect(allocation && memory.deallocate(allocation->allocation, error) && error.empty(),
            "TL-LANG-213 a live allocation must deallocate and clear stale errors");
        context.expect(allocation && !memory.deallocate(allocation->allocation, error) &&
                error == "constant allocation is unknown or already dead" &&
                !memory.deallocate(999, error) && error == "constant allocation is unknown or already dead",
            "TL-LANG-213 double-free and unknown provenance must report the exact error");
    });
}

void registerUnionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-214", "PR", [](TestContext& context) {
        ConstantUnionState state{"", 8};
        std::string error;
        context.expect(activateConstantUnionMember(state, "integer", 8, error) &&
                state.activeMember == "integer" && readConstantUnionMember(state, "integer", error),
            "TL-LANG-214 activating and reading the same fitting union member must succeed");
        context.expect(!readConstantUnionMember(state, "floating", error) &&
                error == "constant union read targets an inactive member",
            "TL-LANG-214 reading a different member must report the exact inactive-member error");
        context.expect(!activateConstantUnionMember(state, "", 1, error) &&
                error == "constant union member does not fit its storage" &&
                state.activeMember == "integer",
            "TL-LANG-214 invalid activation must retain the previous active member");
    });
}

void registerConstevalCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-215", "PR", [](TestContext& context) {
        std::string error;
        const IntegerArithmeticRequest add{IntegerArithmeticOperation::Add, 40, 2, {32, true},
            OverflowPolicy::Checked, NumericEvaluationDomain::Runtime};
        const auto runtime = evaluateIntegerArithmetic(add, error);
        const auto constant = evaluateConstantIntegerArithmetic(add, error);
        context.expect(runtime && constant && runtime->bits == constant->bits && constant->bits == 42,
            "TL-LANG-215 runtime and constant arithmetic must share the exact defined result");
        const auto loop = executeConstantIntegerLoop(1, 2, 5, 5, IntegerArithmeticOperation::Multiply,
            {32, true}, OverflowPolicy::Checked, error);
        context.expect(loop && loop->bits == 32 &&
                !executeConstantIntegerLoop(0, 1, 11, 10, IntegerArithmeticOperation::Add,
                    {32, true}, OverflowPolicy::Checked, error) &&
                error == "constant loop exceeded its deterministic iteration budget",
            "TL-LANG-215 loop operations and deterministic budget cutoff must be exact");
        const IntegerArithmeticRequest overflow{IntegerArithmeticOperation::Add, 127, 1, {8, true},
            OverflowPolicy::Unchecked, NumericEvaluationDomain::Runtime};
        context.expect(!evaluateConstantIntegerArithmetic(overflow, error) &&
                error == "constant evaluation encountered undefined integer overflow",
            "TL-LANG-215 consteval must reject unchecked UB with its exact diagnostic");
    });
}

void registerSelfCheckCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-216", "NIGHTLY", [](TestContext& context) {
        context.expect(integerSemanticsSelfCheck(),
            "TL-LANG-216 exhaustive signed i8 policy self-check must succeed");
    });
}
}

void registerLanguageNumericAdvancedModule(TestRegistry& registry)
{
    registerLosslessMatrixCase(registry);
    registerConversionClassificationCase(registry);
    registerFloatingContractCase(registry);
    registerStrongIdentityCase(registry);
    registerStrongPlanCase(registry);
    registerStrongInvalidCase(registry);
    registerStrongOperatorsCase(registry);
    registerAllocationCase(registry);
    registerPointerCase(registry);
    registerDeallocationCase(registry);
    registerUnionCase(registry);
    registerConstevalCase(registry);
    registerSelfCheckCase(registry);
}
}
