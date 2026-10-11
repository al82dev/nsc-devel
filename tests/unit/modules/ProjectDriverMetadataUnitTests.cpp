#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#define NSC_PDM_ENTRY(id, component, function) int function();
#include "nsc_test/ProjectDriverMetadataLegacyEntries.def"
#undef NSC_PDM_ENTRY

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

// Register O0-safe project, driver, and metadata contracts without copying assertions.
// Registration is iterative O(n) and each selected owner executes synchronously.
void addProjectDriverMetadataEntry(
    TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical O0-safe project, driver, or metadata contract",
        {"legacy-parity", "exclusive", "o0-only"},
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical project/driver/metadata contract reported a failure");
            }
        },
    });
}
}

void registerProjectDriverMetadataUnitTests(TestRegistry& registry)
{
#define NSC_PDM_ENTRY(id, component, function) \
    addProjectDriverMetadataEntry(registry, id, component, function);
#include "nsc_test/ProjectDriverMetadataLegacyEntries.def"
#undef NSC_PDM_ENTRY
}
}
