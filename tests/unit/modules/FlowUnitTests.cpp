#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#include <array>
#include <chrono>
#include <string>

#define NSC_FLOW_ENTRY(id, component, function) int function();
#define NSC_FLOW_CC1_ENTRY(id, selector)
#include "nsc_test/FlowLegacyEntries.def"
#undef NSC_FLOW_CC1_ENTRY
#undef NSC_FLOW_ENTRY

int nscCc1CorrectnessLegacyEntry(int argumentCount, char** arguments);

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

void addFlowEntry(TestRegistry& registry, const char* id, const char* component, LegacyEntry entry)
{
    registry.add(TestCase{
        id,
        component,
        "historical flow contract",
        {"legacy-parity", "exclusive"},
        std::chrono::milliseconds(180000),
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("historical flow contract reported at least one failed assertion");
            }
        },
    });
}

void addFlowCc1Entry(TestRegistry& registry, const char* id, const char* selector)
{
    registry.add(TestCase{
        id,
        "cfg",
        "historical CC1 flow fixture",
        {"legacy-parity", "cc1", "exclusive"},
        std::chrono::milliseconds(120000),
        [selector](TestContext& context) {
            std::string executable = "nsc_unit_tests";
            std::string selectedFixture = selector;
            std::array<char*, 2> arguments{executable.data(), selectedFixture.data()};
            if (nscCc1CorrectnessLegacyEntry(static_cast<int>(arguments.size()), arguments.data()) != 0)
            {
                context.fail("historical CC1 flow fixture reported at least one failed assertion");
            }
        },
    });
}
}

void registerFlowUnitTests(TestRegistry& registry)
{
#define NSC_FLOW_ENTRY(id, component, function) addFlowEntry(registry, id, component, function);
#define NSC_FLOW_CC1_ENTRY(id, selector) addFlowCc1Entry(registry, id, selector);
#include "nsc_test/FlowLegacyEntries.def"
#undef NSC_FLOW_CC1_ENTRY
#undef NSC_FLOW_ENTRY
}
}
