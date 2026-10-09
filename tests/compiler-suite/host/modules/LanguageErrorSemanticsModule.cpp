#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ErrorSemantics.h"

#include <array>
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
    registry.add(TestCase{std::move(id), "CL-LANG-007", "Host.Language", "unit", "PR",
        {"language", "errors"}, std::move(function)});
}

void registerCompleteResultProtocolCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-088", [](TestContext& context) {
        const ResultProtocol protocol{"Value", "Error", true, true, true};
        const ResultProtocol before = protocol;
        std::string error = "stale";
        context.expect(supportsQuestionPropagation(protocol, error) && error.empty(),
            "TL-LANG-088 a complete static Result protocol must support question propagation");
        context.expect(protocol.successType == before.successType && protocol.errorType == before.errorType &&
                protocol.hasIsSuccess == before.hasIsSuccess &&
                protocol.hasTakeSuccess == before.hasTakeSuccess && protocol.hasTakeError == before.hasTakeError,
            "TL-LANG-088 protocol validation must not mutate its input");
    });
}

void registerIncompleteResultProtocolCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-089", [](TestContext& context) {
        const ResultProtocol complete{"Value", "Error", true, true, true};
        std::array<ResultProtocol, 5> incomplete{};
        incomplete.fill(complete);
        incomplete[0].successType.clear();
        incomplete[1].errorType.clear();
        incomplete[2].hasIsSuccess = false;
        incomplete[3].hasTakeSuccess = false;
        incomplete[4].hasTakeError = false;
        for (const ResultProtocol& protocol : incomplete)
        {
            std::string error;
            context.expect(!supportsQuestionPropagation(protocol, error) &&
                    error == "? requires complete static Result protocol members",
                "TL-LANG-089 each missing Result member must produce the exact first diagnostic");
        }
        ResultProtocol multiplyIncomplete = incomplete[0];
        multiplyIncomplete.errorType.clear();
        multiplyIncomplete.hasIsSuccess = false;
        std::string error;
        context.expect(!supportsQuestionPropagation(multiplyIncomplete, error) &&
                error == "? requires complete static Result protocol members",
            "TL-LANG-089 combined omissions must preserve the same deterministic diagnostic");
    });
}

void registerErrorConversionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-090", [](TestContext& context) {
        struct Vector final
        {
            std::string_view source;
            std::string_view target;
            bool adapter;
            bool expected;
        };
        constexpr std::array vectors = {
            Vector{"IoError", "IoError", false, true},
            Vector{"IoError", "IoError", true, true},
            Vector{"IoError", "AppError", false, false},
            Vector{"IoError", "AppError", true, true},
            Vector{"", "", false, true},
            Vector{"", "AppError", false, false},
        };
        for (const Vector& vector : vectors)
        {
            context.expect(errorConversionIsExplicit(vector.source, vector.target, vector.adapter) == vector.expected,
                "TL-LANG-090 identity, difference, and adapter conversion matrix must be exact");
        }
    });
}

void registerThrowContractCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-091", [](TestContext& context) {
        std::string error = "stale";
        for (const ThrowAbi abi : std::array{ThrowAbi::ResultCarrier, ThrowAbi::DualReturn})
        {
            for (const bool crossesCAbi : std::array{false, true})
            {
                for (const bool hasAdapter : std::array{false, true})
                {
                    const ThrowContract contract{"IoError", abi, crossesCAbi, hasAdapter};
                    const bool expected = !crossesCAbi || hasAdapter;
                    const bool accepted = validateThrowContract(contract, error);
                    context.expect(accepted == expected,
                        "TL-LANG-091 C ABI adaptation must be independent of the throw carrier ABI");
                    context.expect(expected ? error.empty() :
                            error == "typed throws cannot cross a C ABI boundary without an adapter",
                        "TL-LANG-091 throw validation must return the exact diagnostic or clear success state");
                }
            }
        }
        context.expect(!validateThrowContract({"", ThrowAbi::DualReturn, true, false}, error) &&
                error == "throws requires an explicit error type",
            "TL-LANG-091 a missing error type must have priority over C ABI adaptation");
    });
}

void registerCatchCoverageCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-092", [](TestContext& context) {
        const std::vector<std::string> empty;
        const std::vector<std::string> declared{"IoError", "ParseError", "IoError"};
        const std::vector<std::string> none;
        const std::vector<std::string> subset{"IoError", "IoError"};
        const std::vector<std::string> exact{"ParseError", "IoError"};
        const std::vector<std::string> superset{"OtherError", "IoError", "ParseError", "IoError"};
        context.expect(catchesAllDeclaredErrors(empty, none, false),
            "TL-LANG-092 an empty declared set must be covered without catches");
        context.expect(!catchesAllDeclaredErrors(declared, none, false) &&
                !catchesAllDeclaredErrors(declared, subset, false),
            "TL-LANG-092 empty and proper-subset catches must not cover declared errors");
        context.expect(catchesAllDeclaredErrors(declared, exact, false) &&
                catchesAllDeclaredErrors(declared, superset, false),
            "TL-LANG-092 catches must use set semantics for duplicates and supersets");
        context.expect(catchesAllDeclaredErrors(declared, none, true),
            "TL-LANG-092 rethrow must cover every declared error regardless of catches");
    });
}

void registerNeverCoercionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-093", [](TestContext& context) {
        context.expect(!neverCoercesTo(""),
            "TL-LANG-093 never must reject an unresolved empty target type");
        for (const std::string_view type : std::array<std::string_view, 5>{
                 "bool", "i32", "ptr<Widget>", "Result<Value,Error>", "never"})
        {
            context.expect(neverCoercesTo(type),
                "TL-LANG-093 never must coerce to every non-empty representative target type");
        }
    });
}

void registerLoopBreakUnificationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-094", [](TestContext& context) {
        const auto unify = [](std::initializer_list<std::string> types) {
            const std::vector<std::string> values(types);
            return unifyLoopBreakTypes(values);
        };
        context.expect(unify({}) == std::optional<std::string>{"never"} &&
                unify({"never"}) == std::optional<std::string>{"never"} &&
                unify({"never", "never"}) == std::optional<std::string>{"never"},
            "TL-LANG-094 empty and never-only break sets must unify to never");
        context.expect(unify({"i32"}) == std::optional<std::string>{"i32"} &&
                unify({"i32", "i32"}) == std::optional<std::string>{"i32"} &&
                unify({"never", "i32", "never", "i32"}) == std::optional<std::string>{"i32"},
            "TL-LANG-094 one concrete type must unify while never entries are ignored");
        context.expect(!unify({"i32", "bool"}).has_value() &&
                !unify({"never", "i32", "bool"}).has_value(),
            "TL-LANG-094 conflicting concrete break types must not unify");
    });
}

void registerPanicProfileParsingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-095", [](TestContext& context) {
        context.expect(parsePanicProfile("abort") == PanicProfile::Abort &&
                parsePanicProfile("Abort") == PanicProfile::Abort &&
                parsePanicProfile("unwind-native") == PanicProfile::NativeUnwind &&
                parsePanicProfile("UnwindNative") == PanicProfile::NativeUnwind,
            "TL-LANG-095 every supported panic spelling must map to the exact profile");
        for (const std::string_view invalid : std::array<std::string_view, 7>{
                 "", "ABORT", "abort ", " abort", "unwind", "UNWIND-NATIVE", "UnwindNative "})
        {
            context.expect(!parsePanicProfile(invalid).has_value(),
                "TL-LANG-095 invalid case, grammar, or trailing bytes must return nullopt");
        }
    });
}

void registerPanicContractCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-096", [](TestContext& context) {
        const PanicContract abort = panicContract(PanicProfile::Abort);
        const PanicContract unwind = panicContract(PanicProfile::NativeUnwind);
        context.expect(panicProfileName(PanicProfile::Abort) == "abort" &&
                !abort.runsCleanup && !abort.mayCrossFfi,
            "TL-LANG-096 abort must use canonical spelling and skip cleanup and FFI unwinding");
        context.expect(panicProfileName(PanicProfile::NativeUnwind) == "unwind-native" &&
                unwind.runsCleanup && !unwind.mayCrossFfi,
            "TL-LANG-096 native unwind must run cleanup but never cross FFI");
    });
}

void registerAssertionSafetyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-097", [](TestContext& context) {
        struct Vector final { AssertionKind kind; bool expected; };
        constexpr std::array vectors = {
            Vector{AssertionKind::Assert, false},
            Vector{AssertionKind::Unreachable, false},
            Vector{AssertionKind::Assume, true},
            Vector{AssertionKind::Panic, false},
            // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
            Vector{static_cast<AssertionKind>(255), false},
        };
        for (const Vector& vector : vectors)
        {
            context.expect(assertionRequiresUnsafe(vector.kind) == vector.expected,
                "TL-LANG-097 only Assume may require unsafe; external values use the false fallback");
        }
    });
}
}

void registerLanguageErrorSemanticsModule(TestRegistry& registry)
{
    registerCompleteResultProtocolCase(registry);
    registerIncompleteResultProtocolCase(registry);
    registerErrorConversionCase(registry);
    registerThrowContractCase(registry);
    registerCatchCoverageCase(registry);
    registerNeverCoercionCase(registry);
    registerLoopBreakUnificationCase(registry);
    registerPanicProfileParsingCase(registry);
    registerPanicContractCase(registry);
    registerAssertionSafetyCase(registry);
}
}
