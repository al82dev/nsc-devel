#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/OwnershipSemantics.h"

#include <array>
#include <optional>
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
    registry.add(TestCase{std::move(id), "CL-LANG-017", "Host.Language", "unit", "PR",
        {"language", "ownership"}, std::move(function)});
}

void registerLifetimeElisionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-225", [](TestContext& context) {
        const std::array<std::string, 0> none{};
        const std::array<std::string, 1> one{"'a"};
        const std::array<std::string, 3> repeated{"'a", "'a", "'a"};
        const std::array<std::string, 3> ambiguous{"'a", "'b", "'a"};
        context.expect(inferElidedOutputLifetime(none, "'self") == "'self" &&
                inferElidedOutputLifetime(ambiguous, "'receiver") == "'receiver" &&
                !inferElidedOutputLifetime(none, std::nullopt) &&
                inferElidedOutputLifetime(one, std::nullopt) == "'a" &&
                inferElidedOutputLifetime(repeated, std::nullopt) == "'a" &&
                !inferElidedOutputLifetime(ambiguous, std::nullopt),
            "TL-LANG-225 receiver priority, zero, unique, repeated, and ambiguous inputs must be exact");
    });
}

void registerOutlivesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-226", [](TestContext& context) {
        const std::array<std::string, 5> chain{"'static", "'a", "'b", "'a", "'c"};
        context.expect(outlives(chain, "'static", "'c") && outlives(chain, "'a", "'b") &&
                !outlives(chain, "'b", "'a") && outlives(chain, "'a", "'a") &&
                !outlives(chain, "'missing", "'a") && !outlives(chain, "'a", "'missing") &&
                !outlives(std::array<std::string, 0>{}, "'a", "'a"),
            "TL-LANG-226 presence and first-match ordering must determine outlives exactly");
    });
}

void registerCleanupOrderCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-227", [](TestContext& context) {
        const std::array<CleanupAction, 0> none{};
        const std::array<CleanupAction, 1> one{{{CleanupKind::Drop, "only"}}};
        const std::array<CleanupAction, 3> many = {{{CleanupKind::Drop, "first"},
            {CleanupKind::DeferByReference, "second"}, {CleanupKind::DeferByValue, "third"}}};
        const auto emptyOrder = cleanupExitOrder(none);
        const auto oneOrder = cleanupExitOrder(one);
        const auto manyOrder = cleanupExitOrder(many);
        context.expect(emptyOrder.empty() && oneOrder.size() == 1 && oneOrder[0].name == "only" &&
                manyOrder.size() == 3 && manyOrder[0].kind == CleanupKind::DeferByValue &&
                manyOrder[0].name == "third" && manyOrder[1].kind == CleanupKind::DeferByReference &&
                manyOrder[1].name == "second" && manyOrder[2].kind == CleanupKind::Drop &&
                manyOrder[2].name == "first" && many[0].name == "first",
            "TL-LANG-227 cleanup must reverse zero, one, and many registrations without mutation");
    });
}

void registerPinPreservationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-228", [](TestContext& context) {
        context.expect(operationPreservesPin("borrow") &&
                operationPreservesPin("project-pinned-field") &&
                operationPreservesPin("destroy-in-place"),
            "TL-LANG-228 the three contractual operations must preserve pinning");
        for (const char* operation : {"", "Borrow", "project", "destroy", "move", "copy", "drop"})
            context.expect(!operationPreservesPin(operation),
                "TL-LANG-228 every non-whitelisted or differently-cased spelling must reject pin preservation");
    });
}

void registerEscapeRoundTripCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-229", [](TestContext& context) {
        const std::array summaries = {
            EscapeSummary{"", {}},
            EscapeSummary{"A|B:C", {EscapeClass::NoEscape, EscapeClass::Return,
                EscapeClass::CallerOwned, EscapeClass::Global, EscapeClass::Unknown}},
        };
        const std::array expected = {
            std::string{"escape-v1|0:|"}, std::string{"escape-v1|5:A|B:C|0,1,2,3,4,"}};
        for (std::size_t index = 0; index < summaries.size(); ++index)
        {
            const std::string encoded = encodeEscapeSummary(summaries[index]);
            EscapeSummary decoded{"stale", {EscapeClass::Unknown}};
            std::string error = "stale";
            context.expect(encoded == expected[index] && decodeEscapeSummary(encoded, decoded, error) &&
                    error.empty() && decoded.functionIdentity == summaries[index].functionIdentity &&
                    decoded.parameters == summaries[index].parameters,
                "TL-LANG-229 empty and full summaries must have exact bytes and structure round-trip");
        }
    });
}

void registerEscapeRejectionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-230", [](TestContext& context) {
        struct Vector final
        {
            std::string encoded;
            std::string error;
        };
        const std::array vectors = {
            Vector{"escape-v2|0:|", "unsupported escape summary version"},
            Vector{"escape-v1|0|", "corrupt escape summary identity"},
            Vector{"escape-v1|1x:A|", "corrupt escape summary length"},
            Vector{"escape-v1|999999999999999999999999:A|", "corrupt escape summary length"},
            Vector{"escape-v1|1:A/", "corrupt escape summary length"},
            Vector{"escape-v1|1:A|0x,", "invalid escape class"},
            Vector{"escape-v1|1:A|5,", "invalid escape class"},
            Vector{"escape-v1|1:A|,", "invalid escape class"},
            Vector{"escape-v1|1:A|0", "corrupt escape class list"},
            Vector{"escape-v1|1:A|0,tail", "corrupt escape class list"},
        };
        for (const Vector& vector : vectors)
        {
            EscapeSummary output{"stale", {EscapeClass::Unknown}};
            std::string error = "stale";
            context.expect(!decodeEscapeSummary(vector.encoded, output, error) &&
                    error == vector.error && output.functionIdentity.empty() && output.parameters.empty(),
                "TL-LANG-230 malformed summaries must fail with exact error and reset output state");
        }
    });
}
}

void registerLanguageOwnershipLifetimeModule(TestRegistry& registry)
{
    registerLifetimeElisionCase(registry);
    registerOutlivesCase(registry);
    registerCleanupOrderCase(registry);
    registerPinPreservationCase(registry);
    registerEscapeRoundTripCase(registry);
    registerEscapeRejectionCase(registry);
}
}
