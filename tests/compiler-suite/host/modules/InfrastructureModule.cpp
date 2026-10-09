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
        "INFRA-CONTEXT",
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
        "INFRA-REGISTRY",
        "Infrastructure",
        "unit",
        "PR",
        {"infrastructure", "smoke"},
        [](TestContext& context) {
            TestRegistry local;
            const auto noOperation = [](TestContext&) {};
            local.add(TestCase{"Z-CASE-001", "Z-CONTRACT", "Z", "unit", "PR", {}, noOperation});
            local.add(TestCase{"A-CASE-001", "A-CONTRACT", "A", "unit", "PR", {}, noOperation});
            context.expect(local.cases().size() == 2, "registry must preserve both cases");
            context.expect(local.cases().front().id == "A-CASE-001", "registry must sort by stable ID");
            context.expect(local.find("Z-CASE-001") != nullptr, "registry lookup must find an existing ID");
            context.expect(local.find("MISSING") == nullptr, "registry lookup must reject an absent ID");
        },
    });
}

void registerDuplicateCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "INFRA-REGISTRY-002",
        "INFRA-REGISTRY",
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
