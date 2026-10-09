#include "nsc_suite/TestRegistry.h"

#include <stdexcept>

namespace nsc::suite
{
namespace
{
void registerContextCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "INFRA-CONTEXT-001",
        "INFRA-CONTEXT-001",
        "Infrastructure",
        "unit",
        "PR",
        {"infrastructure", "smoke"},
        [](TestContext& context) {
            context.expect(true, "true expectation must pass");
            context.expect(context.passed(), "context must remain passing");
        },
    });
}

void registerOrderingCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "INFRA-REGISTRY-001",
        "INFRA-REGISTRY-001",
        "Infrastructure",
        "unit",
        "PR",
        {"infrastructure", "smoke"},
        [](TestContext& context) {
            TestRegistry local;
            const auto noOperation = [](TestContext&) {};
            local.add(TestCase{"Z-CASE-001", "Z-CONTRACT", "Z", "unit", "PR", {}, noOperation});
            local.add(TestCase{"A-CASE-001", "A-CONTRACT", "A", "unit", "PR", {}, noOperation});
            local.add(TestCase{"K-CASE-001", "K-CONTRACT", "A", "unit", "PR", {}, noOperation,
                TestDisposition::KnownProductDefect});
            context.expect(local.cases().size() == 3, "registry must preserve every registered case");
            context.expect(local.cases().front().id == "A-CASE-001", "registry must sort by stable ID");
            context.expect(local.find("Z-CASE-001") != nullptr, "registry lookup must find an existing ID");
            context.expect(local.find("K-CASE-001") != nullptr,
                "registry lookup must expose a known product defect for explicit execution");
            context.expect(local.all().size() == 2 && local.byComponent("A").size() == 1,
                "ordinary aggregate selectors must exclude known product defects");
            context.expect(local.find("MISSING") == nullptr, "registry lookup must reject an absent ID");
        },
    });
}

void registerDuplicateCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "INFRA-REGISTRY-002",
        "INFRA-REGISTRY-001",
        "Infrastructure",
        "unit",
        "PR",
        {"infrastructure"},
        [](TestContext& context) {
            TestRegistry local;
            const auto noOperation = [](TestContext&) {};
            local.add(TestCase{"DUP-CASE-001", "DUP-CONTRACT", "D", "unit", "PR", {}, noOperation});
            bool rejected = false;
            try
            {
                local.add(TestCase{"DUP-CASE-001", "DUP-CONTRACT", "D", "unit", "PR", {}, noOperation});
            }
            catch (const std::invalid_argument&)
            {
                rejected = true;
            }
            context.expect(rejected, "registry must reject duplicate IDs");
        },
    });
}
}

void registerInfrastructureModule(TestRegistry& registry)
{
    registerContextCase(registry);
    registerOrderingCase(registry);
    registerDuplicateCase(registry);
}
}
