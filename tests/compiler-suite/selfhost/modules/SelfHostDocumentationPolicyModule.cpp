#include "SelfHostDocumentationTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_documentation_support::PendingCase;

constexpr std::string_view kPolicyRequirement =
    "the controller has no hermetic selfhost source/project inventory executor that can mutate "
    "bootstrap policy inputs and preserve the explicit version-residue contradiction record";

constexpr std::array<PendingCase, 6> kPolicyCases{{
    {"TSH-00222", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
    {"TSH-00223", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
    {"TSH-00224", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
    {"TSH-00225", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
    {"TSH-00226", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
    {"TSH-00227", "CSH-DOC-002", "source/project inventory and explicit contradiction record",
     kPolicyRequirement},
}};
}

void registerSelfHostDocumentationPolicyModule(TestRegistry& registry)
{
    for (const PendingCase& pending : kPolicyCases)
    {
        selfhost_documentation_support::addInfrastructurePendingCase(registry, pending);
    }
}
}
