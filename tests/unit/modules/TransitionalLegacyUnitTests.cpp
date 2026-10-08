#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#include <chrono>

#define NSC_TRANSITIONAL_ENTRY(id, component, function) int function();
#include "TransitionalLegacyEntries.def"
#undef NSC_TRANSITIONAL_ENTRY

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

void addTransitionalLegacyEntry(
    TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical owner preserved until its NativeSharp fixture reaches parity",
        {"legacy-parity", "transitional", "exclusive"},
        std::chrono::milliseconds(300000),
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical owner reported a failure");
            }
        },
    });
}
}

void registerTransitionalLegacyUnitTests(TestRegistry& registry)
{
#define NSC_TRANSITIONAL_ENTRY(id, component, function) \
    addTransitionalLegacyEntry(registry, id, component, function);
#include "TransitionalLegacyEntries.def"
#undef NSC_TRANSITIONAL_ENTRY
}
}
