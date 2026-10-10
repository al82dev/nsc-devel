#include "NscTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using nsc_support::PendingContract;

constexpr std::array<PendingContract, 10> kProcessContracts{{
    {11U,
     "the controller lacks a general NSC argv, language-scope, diagnostic, and exit-code executor"},
    {12U,
     "the controller cannot observe exact arguments forwarded from Host.nsc to a controlled Driver"},
    {13U,
     "the controller process executor only supports Host.Basic compiler-identity operations"},
    {14U,
     "LLVM supervisor tests require controlled child launch, framing, diagnostics, and worker seams"},
    {15U,
     "banner and top-level progress require a controlled Driver plus ordered stdout/stderr capture"},
    {16U,
     "explicit and automatic logs require isolated cwd, environment, filesystem, and process fixtures"},
    {17U,
     "time-trace publication requires an isolated NSC process and atomic output-file oracle"},
    {18U,
     "failure bundles require controlled project failure, filesystem, and bundle-writer fault injection"},
    {19U,
     "UTF-16 argv conversion requires a Windows wmain process fixture with invalid-sequence injection"},
    {20U,
     "entry-point crash containment requires a platform process fixture with controlled thrown faults"},
}};
}

// Materializes TF-NSC-081..160 exactly once. The cases remain non-selectable until the dedicated
// process executor exists; none is downgraded to an empty oracle, SKIP, XFAIL, or synthetic PASS.
void registerNscProcess081To160Module(TestRegistry& registry)
{
    for (const PendingContract& contract : kProcessContracts)
    {
        nsc_support::addInfrastructureContract(registry, contract, "process");
    }
}
}
