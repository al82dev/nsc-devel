#include "SelfHostDocumentationTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_documentation_support::PendingCase;

constexpr std::string_view kArchitectureRequirement =
    "the controller has no hermetic documentation/source mutation executor that traces each "
    "README architecture claim to the current selfhost tree and records disagreements";

constexpr std::array<PendingCase, 6> kArchitectureCases{{
    {"TSH-00216", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
    {"TSH-00217", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
    {"TSH-00218", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
    {"TSH-00219", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
    {"TSH-00220", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
    {"TSH-00221", "CSH-DOC-001", "trace each claim to current source or mark historical",
     kArchitectureRequirement},
}};
}

void registerSelfHostDocumentationArchitectureModule(TestRegistry& registry)
{
    for (const PendingCase& pending : kArchitectureCases)
    {
        selfhost_documentation_support::addInfrastructurePendingCase(registry, pending);
    }
}
}
