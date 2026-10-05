#include "nativesharp/Language/IntegrationQuality.h"

#include "../support/TestContext.h"

#include <array>
#include <set>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const auto first = buildNativeSharp3InteractionMatrix(1000);
    const auto second = buildNativeSharp3InteractionMatrix(1000);
    tests.expect(first.size() == 7 && first.size() == second.size() &&
            first[2].identity == second[2].identity &&
            first[2].deterministicSeed == second[2].deterministicSeed,
        "the interaction catalog is generated deterministically from a fixed seed");
    tests.expect(validatePrioritizedInteractionMatrix(first, error),
        "the catalog covers required high-risk pairs and at least one prioritized triple");

    std::set<std::uint64_t> seeds;
    for (const InteractionCase& item : first) seeds.insert(item.deterministicSeed);
    tests.expect(seeds.size() == first.size() && *seeds.begin() == 1001,
        "every interaction case owns a stable unique seed");
    tests.expect(first[2].areas.size() == 3 && first[2].changesMir &&
            first[2].changesOwnership && first[2].risk == InteractionRisk::Critical,
        "ownership-coroutine-error interaction is prioritized by MIR and ownership risk");

    const std::array essential = {FeatureArea::Ownership, FeatureArea::Coroutines};
    const auto minimized = minimizeInteractionCase(first[2], essential, error);
    tests.expect(minimized.has_value() && minimized->areas.size() == 2 &&
            minimized->deterministicSeed == first[2].deterministicSeed &&
            minimized->identity.ends_with("-min"),
        "failure minimization preserves the seed and only essential feature areas");

    const std::array absent = {FeatureArea::Ownership, FeatureArea::Ffi};
    tests.expect(!minimizeInteractionCase(first[2], absent, error).has_value() &&
            error.find("absent") != std::string::npos,
        "a minimized reproducer cannot introduce a feature absent from the failure");

    auto missing = first;
    missing.erase(missing.begin() + 3);
    tests.expect(!validatePrioritizedInteractionMatrix(missing, error) &&
            error.find("high-risk feature pair") != std::string::npos,
        "omitting a required module-FFI pair fails the matrix causally");

    auto duplicate = first;
    duplicate.push_back(first.front());
    tests.expect(!validatePrioritizedInteractionMatrix(duplicate, error) &&
            error.find("duplicate") != std::string::npos,
        "duplicate case identities cannot inflate interaction coverage");
    return tests.result();
}
