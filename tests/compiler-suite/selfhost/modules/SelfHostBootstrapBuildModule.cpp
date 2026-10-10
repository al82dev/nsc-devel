#include "SelfHostBootstrapTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_bootstrap_support::PendingCase;

constexpr std::string_view kBuildRequirement =
    "the controller process executor supports only Host.Basic identity operations and cannot "
    "invoke GenerationBuilder.Build with isolated producer, manifest, output, trace, and diagnostics";

constexpr std::array<PendingCase, 7> kBuildCases{{
    {"TSH-00045", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00046", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00047", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00048", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00049", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00050", "CSH-BOO-003", "integration", "PR",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
    {"TSH-00051", "CSH-BOO-003", "integration", "NIGHTLY",
     "stage sequence outputs diagnostics and exit result", kBuildRequirement},
}};
}

// Materializes TSH-00045..051 exactly once as non-executable infrastructure work. The current
// source-derived codegen blockage is evidence to preserve, not a synthetic expected success.
void registerSelfHostBootstrapBuildModule(TestRegistry& registry)
{
    for (const PendingCase& pending : kBuildCases)
    {
        selfhost_bootstrap_support::addInfrastructurePendingCase(registry, pending);
    }
}
}
