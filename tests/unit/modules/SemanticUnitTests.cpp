#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#include <array>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#define NSC_SEMANTIC_ENTRY(id, component, function) int function();
#define NSC_CC1_ENTRY(id, selector)
#include "nsc_test/SemanticLegacyEntries.def"
#undef NSC_CC1_ENTRY
#undef NSC_SEMANTIC_ENTRY

int nscCc1CorrectnessLegacyEntry(int argumentCount, char** arguments);

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

void addLegacyEntry(TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical semantic contract",
        {"legacy-parity", "exclusive"},
        std::chrono::milliseconds(120000),
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical semantic contract reported at least one failed assertion");
            }
        },
    });
}

void addCc1Entry(TestRegistry& registry, const char* id, const char* selector)
{
    registry.add(TestCase{
        id,
        "sema",
        "historical CC1 semantic fixture",
        {"legacy-parity", "cc1", "exclusive"},
        std::chrono::milliseconds(120000),
        [selector](TestContext& context) {
            std::string executable = "nsc_unit_tests";
            std::string selectedFixture = selector;
            std::array<char*, 2> arguments{executable.data(), selectedFixture.data()};
            if (nscCc1CorrectnessLegacyEntry(static_cast<int>(arguments.size()), arguments.data()) != 0)
            {
                context.fail("historical CC1 semantic fixture reported at least one failed assertion");
            }
        },
    });
}
}

void registerSemanticUnitTests(TestRegistry& registry)
{
    // These adapters preserve the exact historical assertions while the
    // external NativeSharp fixtures are moved to the common case catalog.
#define NSC_SEMANTIC_ENTRY(id, component, function) addLegacyEntry(registry, id, component, function);
#define NSC_CC1_ENTRY(id, selector) addCc1Entry(registry, id, selector);
#include "nsc_test/SemanticLegacyEntries.def"
#undef NSC_CC1_ENTRY
#undef NSC_SEMANTIC_ENTRY
}
}
