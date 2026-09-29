#include "nativesharp/Language/OwnershipSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    MoveState value{false, false, {true, true, true}};
    tests.expect(moveField(value, 1, error) && classifyValueState(value) == ValueState::PartiallyMoved &&
            requiredDropFlagCount(value) == 3,
        "partial move tracks initialized fields and introduces only required drop flags");
    tests.expect(!moveField(value, 1, error), "moving an already moved field is rejected");
    MoveState custom{false, true, {true, true}};
    tests.expect(!moveField(custom, 0, error),
        "types with custom destructors conservatively reject partial moves");
    tests.expect(moveWholeValue(value, error) && classifyValueState(value) == ValueState::Moved &&
            !moveWholeValue(value, error),
        "a move-only value becomes unobservable until reassigned");

    const BorrowPlace left{"packet", {"header"}};
    const BorrowPlace right{"packet", {"payload"}};
    const BorrowPlace nested{"packet", {"header", "size"}};
    tests.expect(mayTakeExclusiveBorrows(left, right) && !mayTakeExclusiveBorrows(left, nested),
        "split borrows accept disjoint fields and reject overlapping projections");
    tests.expect(inferElidedOutputLifetime(std::array<std::string, 1>{"'a"}, std::nullopt) == "'a" &&
            !inferElidedOutputLifetime(std::array<std::string, 2>{"'a", "'b"}, std::nullopt).has_value(),
        "lifetime elision succeeds only when the source is unambiguous");
    tests.expect(outlives(std::array<std::string, 3>{"'static", "'a", "'b"}, "'a", "'b") &&
            !outlives(std::array<std::string, 3>{"'static", "'a", "'b"}, "'b", "'a"),
        "named lifetime constraints use an explicit outlives ordering");

    const std::array<CleanupAction, 3> registered = {{{CleanupKind::Drop, "a"},
        {CleanupKind::DeferByReference, "log"}, {CleanupKind::Drop, "b"}}};
    const auto cleanup = cleanupExitOrder(registered);
    tests.expect(cleanup[0].name == "b" && cleanup[1].name == "log" && cleanup[2].name == "a",
        "all exits share deterministic reverse-registration cleanup order");
    tests.expect(operationPreservesPin("borrow") && operationPreservesPin("destroy-in-place") &&
            !operationPreservesPin("move"),
        "pinning permits observation and in-place destruction but rejects relocation");

    const EscapeSummary published{"Demo.Read", {EscapeClass::NoEscape, EscapeClass::Return}};
    const std::string encoded = encodeEscapeSummary(published);
    EscapeSummary decoded;
    tests.expect(decodeEscapeSummary(encoded, decoded, error) &&
            decoded.functionIdentity == published.functionIdentity && decoded.parameters == published.parameters,
        "escape summaries round-trip deterministically across module metadata");
    tests.expect(!decodeEscapeSummary("escape-v9|bad", decoded, error),
        "corrupt or unknown escape metadata fails closed");
    return tests.result();
}
