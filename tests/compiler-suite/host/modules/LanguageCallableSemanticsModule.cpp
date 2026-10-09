#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/CallableSemantics.h"

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
    registry.add(TestCase{std::move(id), "CL-LANG-003", "Host.Language", "unit",
        "PR", {"language", "callable"}, std::move(function)});
}

bool sameFunctionType(const FunctionType& left, const FunctionType& right)
{
    return left.parameters == right.parameters && left.result == right.result &&
        left.carrier == right.carrier && left.callingConvention == right.callingConvention &&
        left.errorType == right.errorType && left.lifetime == right.lifetime &&
        left.variadic == right.variadic && left.isUnsafe == right.isUnsafe &&
        left.isNoHeap == right.isNoHeap && left.isAsync == right.isAsync &&
        left.nullable == right.nullable;
}

FunctionType complexFunctionType()
{
    FunctionType type;
    type.parameters = {"i32", "pkg:T"};
    type.result = "bool";
    type.carrier = FunctionCarrier::Reference;
    type.callingConvention = CallingConvention::Win64;
    type.errorType = "Err";
    type.lifetime = "'a";
    type.variadic = true;
    type.isUnsafe = true;
    type.isNoHeap = true;
    type.isAsync = true;
    type.nullable = true;
    return type;
}

void registerFunctionIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-025", [](TestContext& context) {
        FunctionType simple;
        simple.result = "void";
        context.expect(functionTypeIdentity(simple) == "fn:ptr:native:00000:0#:0:4#void:0#",
            "TL-LANG-025 the default callable must have the exact byte identity");
        context.expect(functionTypeIdentity(complexFunctionType()) ==
                "fn:ref:win64:11111:2#'a:2:3#i32:5#pkg:T:4#bool:3#Err",
            "TL-LANG-025 all callable fields must appear in their exact serialized order");
        const std::array conventions{
            CallingConvention::NativeSharp, CallingConvention::C, CallingConvention::StdCall,
            CallingConvention::FastCall, CallingConvention::VectorCall, CallingConvention::Win64};
        const std::array names{"native", "c", "stdcall", "fastcall", "vectorcall", "win64"};
        for (std::size_t index = 0; index < conventions.size(); ++index)
        {
            FunctionType type;
            type.result = "r";
            type.carrier = index % 2 == 0 ? FunctionCarrier::Pointer : FunctionCarrier::Reference;
            type.callingConvention = conventions[index];
            const std::string carrier = index % 2 == 0 ? "ptr" : "ref";
            const std::string expected = "fn:" + carrier + ":" + names[index] +
                ":00000:0#:0:1#r:0#";
            context.expect(functionTypeIdentity(type) == expected,
                "TL-LANG-025 every carrier and calling convention must serialize exactly");
        }
    });
}

void registerFunctionRoundTripCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-026", [](TestContext& context) {
        FunctionType simple;
        simple.result = "void";
        FunctionType flags;
        flags.parameters = {"", "x:y#z"};
        flags.result = "R";
        flags.callingConvention = CallingConvention::VectorCall;
        flags.lifetime = "static";
        flags.isUnsafe = true;
        flags.isAsync = true;
        const std::array types{simple, complexFunctionType(), flags};
        for (const FunctionType& expected : types)
        {
            const auto parsed = parseFunctionTypeIdentity(functionTypeIdentity(expected));
            context.expect(parsed.has_value() && sameFunctionType(*parsed, expected),
                "TL-LANG-026 every complete function identity must round-trip without loss");
        }
    });
}

void registerInvalidFunctionIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-027", [](TestContext& context) {
        const std::array invalid{
            "", "xx:ptr:native:00000:0#:0:1#r:0#", "fn:bad:native:00000:0#:0:1#r:0#",
            "fn:ptr:bad:00000:0#:0:1#r:0#", "fn:ptr:native:0000:0#:0:1#r:0#",
            "fn:ptr:native:0000x:0#:0:1#r:0#", "fn:ptr:native:00000:x#:0:1#r:0#",
            "fn:ptr:native:00000:1#:0:1#r:0#", "fn:ptr:native:00000:0#;0:1#r:0#",
            "fn:ptr:native:00000:0#:x:1#r:0#",
            "fn:ptr:native:00000:0#:184467440737095516160:1#r:0#",
            "fn:ptr:native:00000:0#:1:1#p;1#r:0#", "fn:ptr:native:00000:0#:0:0#:0#",
            "fn:ptr:native:00000:0#:0:1#r:0#trailing"};
        for (const std::string_view identity : invalid)
            context.expect(!parseFunctionTypeIdentity(identity).has_value(),
                "TL-LANG-027 every malformed grammar class must return nullopt");
    });
}

void registerFunctionIdentityPredicateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-028", [](TestContext& context) {
        const std::string valid = functionTypeIdentity(complexFunctionType());
        context.expect(isFunctionTypeIdentity(valid),
            "TL-LANG-028 a valid complete identity must be recognized");
        for (std::size_t length = 0; length < valid.size(); ++length)
            context.expect(isFunctionTypeIdentity(std::string_view(valid).substr(0, length)) ==
                    parseFunctionTypeIdentity(std::string_view(valid).substr(0, length)).has_value(),
                "TL-LANG-028 predicate and parser must agree for every truncation");
        for (const std::string_view text : std::array{
                 std::string_view{"text"}, std::string_view{"fn"}, std::string_view{"lambda:0:0:"}})
            context.expect(isFunctionTypeIdentity(text) == parseFunctionTypeIdentity(text).has_value(),
                "TL-LANG-028 predicate and parser must agree for arbitrary text");
    });
}

void registerFunctionAssignabilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-029", [](TestContext& context) {
        FunctionType base;
        base.parameters = {"i32"};
        base.result = "bool";
        context.expect(functionTypesAssignable(base, base),
            "TL-LANG-029 an identical function type must assign to itself");
        const auto expectSymmetricReject = [&](FunctionType changed, std::string_view field) {
            context.expect(!functionTypesAssignable(base, changed) &&
                    !functionTypesAssignable(changed, base),
                "TL-LANG-029 invariant field mismatch must reject in both directions: " +
                    std::string(field));
        };
        FunctionType changed = base;
        changed.parameters = {"i64"}; expectSymmetricReject(changed, "parameters");
        changed = base; changed.result = "i32"; expectSymmetricReject(changed, "result");
        changed = base; changed.carrier = FunctionCarrier::Reference; expectSymmetricReject(changed, "carrier");
        changed = base; changed.callingConvention = CallingConvention::C;
        expectSymmetricReject(changed, "calling convention");
        changed = base; changed.lifetime = "'a"; expectSymmetricReject(changed, "lifetime");
        changed = base; changed.variadic = true; expectSymmetricReject(changed, "variadic");
        changed = base; changed.isAsync = true; expectSymmetricReject(changed, "async");

        FunctionType source = base;
        FunctionType target = base;
        target.errorType = "Err";
        context.expect(functionTypesAssignable(source, target) && !functionTypesAssignable(target, source),
            "TL-LANG-029 an infallible source may widen to a declared error type only");
        source = base; target = base; source.isUnsafe = true;
        context.expect(!functionTypesAssignable(source, target) && functionTypesAssignable(target, source),
            "TL-LANG-029 unsafe assignability must be directional");
        source = base; target = base; target.isNoHeap = true;
        context.expect(!functionTypesAssignable(source, target) && functionTypesAssignable(target, source),
            "TL-LANG-029 noheap assignability must be directional");
        source = base; target = base; source.nullable = true;
        context.expect(!functionTypesAssignable(source, target) && functionTypesAssignable(target, source),
            "TL-LANG-029 nullable assignability must be directional");
    });
}

void registerCallableNamesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-030", [](TestContext& context) {
        context.expect(functionCarrierName(FunctionCarrier::Pointer) == "ptr" &&
                functionCarrierName(FunctionCarrier::Reference) == "ref",
            "TL-LANG-030 every declared carrier must have its exact spelling");
        const std::array conventions{
            CallingConvention::NativeSharp, CallingConvention::C, CallingConvention::StdCall,
            CallingConvention::FastCall, CallingConvention::VectorCall, CallingConvention::Win64};
        const std::array names{"native", "c", "stdcall", "fastcall", "vectorcall", "win64"};
        for (std::size_t index = 0; index < conventions.size(); ++index)
            context.expect(callingConventionName(conventions[index]) == names[index],
                "TL-LANG-030 every declared convention must have its exact spelling");
        // External enum fallbacks are deliberate parts of the current public helper contract.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        context.expect(functionCarrierName(static_cast<FunctionCarrier>(999)) == "ptr",
            "TL-LANG-030 an external carrier value must use the current ptr fallback");
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        context.expect(callingConventionName(static_cast<CallingConvention>(999)) == "native",
            "TL-LANG-030 an external convention value must use the current native fallback");
    });
}

void registerLambdaRoundTripCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-031", [](TestContext& context) {
        const std::array lambdas{
            AnonymousLambdaType{0, 0, complexFunctionType()},
            AnonymousLambdaType{std::numeric_limits<std::size_t>::max(),
                std::numeric_limits<std::size_t>::max(), complexFunctionType()}};
        for (const AnonymousLambdaType& expected : lambdas)
        {
            const std::string identity = anonymousLambdaTypeIdentity(expected);
            const auto parsed = parseAnonymousLambdaTypeIdentity(identity);
            context.expect(parsed.has_value() && parsed->ownerOrdinal == expected.ownerOrdinal &&
                    parsed->lambdaOrdinal == expected.lambdaOrdinal &&
                    sameFunctionType(parsed->signature, expected.signature) &&
                    isAnonymousLambdaTypeIdentity(identity),
                "TL-LANG-031 ordinal boundaries and complex signatures must round-trip exactly");
        }
    });
}

void registerInvalidLambdaIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-032", [](TestContext& context) {
        FunctionType signature;
        signature.result = "r";
        const std::string function = functionTypeIdentity(signature);
        const std::array invalid{
            std::string{""}, std::string{"lamb:0:0:"} + function,
            std::string{"lambda::0:"} + function, std::string{"lambda:x:0:"} + function,
            std::string{"lambda:0::"} + function, std::string{"lambda:0:x:"} + function,
            std::string{"lambda:-1:0:"} + function,
            std::string{"lambda:184467440737095516160:0:"} + function,
            std::string{"lambda:0:0:not-a-function"},
            std::string{"lambda:0:0:"} + function + "trailing"};
        for (const std::string& identity : invalid)
            context.expect(!parseAnonymousLambdaTypeIdentity(identity).has_value() &&
                    !isAnonymousLambdaTypeIdentity(identity),
                "TL-LANG-032 invalid lambda grammar must return nullopt and false");
    });
}

void registerLambdaGeneratedNameCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-033", [](TestContext& context) {
        context.expect(anonymousLambdaGeneratedName({0, 0, {}}) == "__nsc_lambda_0_0",
            "TL-LANG-033 zero ordinals must have the exact generated name");
        const std::size_t maximum = std::numeric_limits<std::size_t>::max();
        const std::string expected = "__nsc_lambda_" + std::to_string(maximum) + "_" +
            std::to_string(maximum);
        context.expect(anonymousLambdaGeneratedName({maximum, maximum, {}}) == expected,
            "TL-LANG-033 maximum ordinals must retain every decimal digit");
    });
}

void registerClosureEscapeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-036", [](TestContext& context) {
        const std::array captures{
            Capture{"copy", 4, 4, CaptureMode::Copy, false},
            Capture{"move", 4, 4, CaptureMode::Move, false},
            Capture{"ref", 8, 8, CaptureMode::Reference, false}};
        std::string error = "stale";
        context.expect(validateClosureEscape(captures, false, error) && error.empty(),
            "TL-LANG-036 a non-escaping closure must accept all captures and clear errors");
        const std::array values{
            Capture{"copy", 4, 4, CaptureMode::Copy, false},
            Capture{"move", 4, 4, CaptureMode::Move, false}};
        context.expect(validateClosureEscape(values, true, error) && error.empty(),
            "TL-LANG-036 copy and move captures may escape regardless of source lifetime");
        const std::array references{
            Capture{"good", 8, 8, CaptureMode::Reference, true},
            Capture{"bad", 8, 8, CaptureMode::MutableReference, false}};
        context.expect(!validateClosureEscape(references, true, error) &&
                error == "closure reference capture escapes its source lifetime: bad",
            "TL-LANG-036 the first non-outliving reference must use the exact error");
    });
}

void registerCallableTraitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-037", [](TestContext& context) {
        const std::vector<Capture> empty;
        const std::array copy{Capture{"c", 1, 1, CaptureMode::Copy, true}};
        const std::array reference{Capture{"r", 1, 1, CaptureMode::Reference, true}};
        const std::array mutableReference{Capture{"m", 1, 1, CaptureMode::MutableReference, true}};
        const std::array move{Capture{"v", 1, 1, CaptureMode::Move, true}};
        const std::array combined{
            Capture{"m", 1, 1, CaptureMode::MutableReference, true},
            Capture{"v", 1, 1, CaptureMode::Move, true}};
        context.expect(callableTrait(empty) == CallableTrait::Fn &&
                callableTrait(copy) == CallableTrait::Fn &&
                callableTrait(reference) == CallableTrait::Fn,
            "TL-LANG-037 empty, copy and shared-reference captures must be Fn");
        context.expect(callableTrait(mutableReference) == CallableTrait::FnMut,
            "TL-LANG-037 a mutable-reference capture must be FnMut");
        context.expect(callableTrait(move) == CallableTrait::FnOnce &&
                callableTrait(combined) == CallableTrait::FnOnce,
            "TL-LANG-037 move must select FnOnce ahead of FnMut");
    });
}

void registerLambdaConversionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-038", [](TestContext& context) {
        const std::vector<Capture> empty;
        context.expect(nonCapturingLambdaConvertsToPointer(empty),
            "TL-LANG-038 an empty capture list must convert to a function pointer");
        for (const CaptureMode mode : std::array{
                 CaptureMode::Copy, CaptureMode::Move,
                 CaptureMode::Reference, CaptureMode::MutableReference})
        {
            const std::array captures{Capture{"x", 1, 1, mode, true}};
            context.expect(!nonCapturingLambdaConvertsToPointer(captures),
                "TL-LANG-038 every non-empty capture mode must reject pointer conversion");
        }
        context.expect(!genericLambdaUsesErasedDispatch(),
            "TL-LANG-038 generic lambdas must not use erased dispatch");
    });
}

void registerBoundMethodAndMustTailCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-039", [](TestContext& context) {
        const BoundMethodLayout layout = windowsX64BoundMethodLayout();
        context.expect(layout.size == 32 && layout.alignment == 8,
            "TL-LANG-039 Windows x64 bound methods must use the exact ABI 1.1 layout");
        FunctionType caller;
        caller.parameters = {"i32"};
        caller.result = "bool";
        FunctionType callee = caller;
        std::string error = "stale";
        context.expect(validateMustTail(caller, callee, false, error) && error.empty(),
            "TL-LANG-039 compatible cleanup-free must_tail must succeed and clear errors");
        callee.result = "i32";
        context.expect(!validateMustTail(caller, callee, true, error) &&
                error == "must_tail call has pending cleanup",
            "TL-LANG-039 pending cleanup must have first rejection priority");
        context.expect(!validateMustTail(caller, callee, false, error) &&
                error == "must_tail caller and callee ABIs are incompatible",
            "TL-LANG-039 an incompatible ABI must use the exact error after cleanup passes");
    });
}
}

void registerLanguageCallableSemanticsModule(TestRegistry& registry)
{
    registerFunctionIdentityCase(registry);
    registerFunctionRoundTripCase(registry);
    registerInvalidFunctionIdentityCase(registry);
    registerFunctionIdentityPredicateCase(registry);
    registerFunctionAssignabilityCase(registry);
    registerCallableNamesCase(registry);
    registerLambdaRoundTripCase(registry);
    registerInvalidLambdaIdentityCase(registry);
    registerLambdaGeneratedNameCase(registry);
    registerClosureEscapeCase(registry);
    registerCallableTraitCase(registry);
    registerLambdaConversionCase(registry);
    registerBoundMethodAndMustTailCase(registry);
}
}
