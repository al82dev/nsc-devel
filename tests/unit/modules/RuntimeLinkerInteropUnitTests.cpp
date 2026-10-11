#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#define NSC_RLI_ENTRY(id, component, function) int function();
#include "nsc_test/RuntimeLinkerInteropLegacyEntries.def"
#undef NSC_RLI_ENTRY

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

// Register O0-safe runtime, linker, and interop contracts without duplicating assertions.
void addRuntimeLinkerInteropEntry(
    TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical O0-safe runtime, linker, or interop contract",
        {"legacy-parity", "exclusive", "o0-only"},
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical runtime/linker/interop contract reported a failure");
            }
        },
    });
}
}

void registerRuntimeLinkerInteropUnitTests(TestRegistry& registry)
{
#define NSC_RLI_ENTRY(id, component, function) \
    addRuntimeLinkerInteropEntry(registry, id, component, function);
#include "nsc_test/RuntimeLinkerInteropLegacyEntries.def"
#undef NSC_RLI_ENTRY
}
}
