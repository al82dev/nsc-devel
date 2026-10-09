#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Source/SourcePosition.h"

namespace nsc::suite
{
void registerSourcePositionModule(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-001",
        "CS-SRC-001",
        "Host.Source",
        "unit",
        "PR",
        {"source", "position", "value"},
        [](TestContext& context) {
            const nativesharp::source::SourcePosition position{};
            context.expect(position.line == 1, "default source line must be one-based");
            context.expect(position.column == 1, "default source column must be one-based");
        },
    });
    registry.add(TestCase{
        "TS-SRC-002",
        "CS-SRC-001",
        "Host.Source",
        "unit",
        "PR",
        {"source", "position", "value"},
        [](TestContext& context) {
            const nativesharp::source::SourcePosition position{7, 11};
            context.expect(position.line == 7, "aggregate source line must preserve its value");
            context.expect(position.column == 11, "aggregate source column must preserve its value");
        },
    });
}
}
