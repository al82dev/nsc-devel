#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#define NSC_IR_ENTRY(id, component, function) int function();
#include "nsc_test/IrLegacyEntries.def"
#undef NSC_IR_ENTRY

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

// Register one historical IR contract without duplicating its assertion logic.
// The adapter executes synchronously, owns no resources, and performs O(1) registry work.
void addIrEntry(TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical HIR, MIR, or pass contract",
        {"legacy-parity", "exclusive"},
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical IR contract reported at least one failed assertion");
            }
        },
    });
}
}

// Populate the consolidated executable from the deterministic X-macro inventory.
// Registration is iterative, preserves inventory order, and has O(n) time with O(1) extra space.
void registerIrUnitTests(TestRegistry& registry)
{
#define NSC_IR_ENTRY(id, component, function) addIrEntry(registry, id, component, function);
#include "nsc_test/IrLegacyEntries.def"
#undef NSC_IR_ENTRY
}
}
