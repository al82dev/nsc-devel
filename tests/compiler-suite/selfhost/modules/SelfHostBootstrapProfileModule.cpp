#include "SelfHostBootstrapTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_bootstrap_support::PendingCase;

constexpr std::string_view kSourceRequirement =
    "GenerationBuilder.VerifySource is private and the controller has no SelfHost source-profile "
    "executor capable of observing the first rejected construct";
constexpr std::string_view kManifestRequirement =
    "GenerationBuilder.VerifyManifest is private and the controller has no SelfHost manifest "
    "executor capable of observing a normalized model or exact rejection category";

constexpr std::array<PendingCase, 16> kProfileCases{{
    {"TSH-00029", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00030", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00031", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00032", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00033", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00034", "CSH-BOO-001", "component", "PR",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00035", "CSH-BOO-001", "component", "NIGHTLY",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00036", "CSH-BOO-001", "component", "NIGHTLY",
     "accept/reject matrix with first rejected construct", kSourceRequirement},
    {"TSH-00037", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00038", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00039", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00040", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00041", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00042", "CSH-BOO-002", "component", "PR",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00043", "CSH-BOO-002", "component", "NIGHTLY",
     "normalized manifest model or exact rejection category", kManifestRequirement},
    {"TSH-00044", "CSH-BOO-002", "component", "NIGHTLY",
     "normalized manifest model or exact rejection category", kManifestRequirement},
}};
}

// Materializes TSH-00029..044 exactly once. No case is executable until a real controller
// boundary reaches the private verifiers without copying their algorithms into a surrogate.
void registerSelfHostBootstrapProfileModule(TestRegistry& registry)
{
    for (const PendingCase& pending : kProfileCases)
    {
        selfhost_bootstrap_support::addInfrastructurePendingCase(registry, pending);
    }
}
}
