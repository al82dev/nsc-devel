#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#define NSC_BACKEND_ENTRY(id, component, function) int function();
#include "nsc_test/BackendLegacyEntries.def"
#undef NSC_BACKEND_ENTRY

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

// Register one O0-safe historical backend contract without copying its assertion logic.
// The registry work is iterative O(n); each selected entry retains its original behavior.
void addBackendEntry(TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical O0-safe backend contract",
        {"legacy-parity", "exclusive", "o0-only"},
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical backend contract reported at least one failed assertion");
            }
        },
    });
}
}

void registerBackendUnitTests(TestRegistry& registry)
{
#define NSC_BACKEND_ENTRY(id, component, function) addBackendEntry(registry, id, component, function);
#include "nsc_test/BackendLegacyEntries.def"
#undef NSC_BACKEND_ENTRY
}
}
