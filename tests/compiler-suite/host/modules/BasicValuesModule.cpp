#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
void addCase(
    TestRegistry& registry,
    std::string id,
    std::string contractId,
    std::string layer,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        std::move(contractId),
        "Host.Basic",
        std::move(layer),
        "PR",
        {"basic", "values"},
        std::move(function),
    });
}

void registerFileIdCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-001", "CB-BAS-001", "unit", [](TestContext& context) {
        const nativesharp::FileId id;
        context.expect(!id.isValid(), "default FileId must be invalid");
        context.expect(id.value() == 0, "default FileId must preserve the zero sentinel");
    });
    addCase(registry, "TB-BAS-002", "CB-BAS-001", "unit", [](TestContext& context) {
        const nativesharp::FileId id(1);
        context.expect(id.isValid(), "FileId(1) must be valid");
        context.expect(id.value() == 1, "FileId(1) must preserve its value");
    });
    addCase(registry, "TB-BAS-003", "CB-BAS-001", "unit", [](TestContext& context) {
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        const nativesharp::FileId id(maximum);
        context.expect(id.isValid(), "maximum FileId must be valid");
        context.expect(id.value() == maximum, "maximum FileId must be preserved");
    });
    addCase(registry, "TB-BAS-004", "CB-BAS-001", "unit", [](TestContext& context) {
        context.expect(nativesharp::FileId{} == nativesharp::FileId{}, "zero IDs must compare equal");
        context.expect(nativesharp::FileId(1) == nativesharp::FileId(1), "equal IDs must compare equal");
        context.expect(!(nativesharp::FileId(1) == nativesharp::FileId(2)), "different IDs must not compare equal");
    });
}

void registerSourceLocationCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-005", "CB-BAS-002", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation location;
        context.expect(!location.isValid(), "default source location must be invalid");
        context.expect(location.fileId().value() == 0, "default source location must use file zero");
        context.expect(location.offset() == 0, "default source location must use offset zero");
    });
    addCase(registry, "TB-BAS-006", "CB-BAS-002", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation location(nativesharp::FileId(7), 0);
        context.expect(location.isValid(), "valid file with offset zero must be valid");
        context.expect(location.fileId() == nativesharp::FileId(7), "source location must preserve its file");
        context.expect(location.offset() == 0, "source location must preserve offset zero");
    });
    addCase(registry, "TB-BAS-007", "CB-BAS-002", "unit", [](TestContext& context) {
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        const nativesharp::SourceLocation location(nativesharp::FileId(7), maximum);
        context.expect(location.isValid(), "location validity must not inspect its offset");
        context.expect(location.offset() == maximum, "source location must preserve maximum offset");
    });
    addCase(registry, "TB-BAS-008", "CB-BAS-002", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation base(nativesharp::FileId(3), 11);
        context.expect(base == nativesharp::SourceLocation(nativesharp::FileId(3), 11), "equal locations must compare equal");
        context.expect(!(base == nativesharp::SourceLocation(nativesharp::FileId(4), 11)), "file participates in location equality");
        context.expect(!(base == nativesharp::SourceLocation(nativesharp::FileId(3), 12)), "offset participates in location equality");
    });
}

void registerSourceRangeCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-009", "CB-BAS-003", "unit", [](TestContext& context) {
        context.expect(!nativesharp::SourceRange{}.isValid(), "default source range must be invalid");
    });
    addCase(registry, "TB-BAS-010", "CB-BAS-003", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation point(nativesharp::FileId(2), 9);
        context.expect(nativesharp::SourceRange(point, point).isValid(), "empty same-file range must be valid");
    });
    addCase(registry, "TB-BAS-011", "CB-BAS-003", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation begin(nativesharp::FileId(2), 3);
        const nativesharp::SourceLocation end(nativesharp::FileId(2), 9);
        const nativesharp::SourceRange range(begin, end);
        context.expect(range.isValid(), "ordered same-file range must be valid");
        context.expect(range.begin() == begin, "source range must preserve its beginning");
        context.expect(range.end() == end, "source range must preserve its end");
    });
    addCase(registry, "TB-BAS-012", "CB-BAS-003", "unit", [](TestContext& context) {
        const nativesharp::SourceRange range(
            nativesharp::SourceLocation(nativesharp::FileId(2), 10),
            nativesharp::SourceLocation(nativesharp::FileId(2), 9));
        context.expect(!range.isValid(), "inverted source range must be invalid");
    });
    addCase(registry, "TB-BAS-013", "CB-BAS-003", "unit", [](TestContext& context) {
        const nativesharp::SourceRange range(
            nativesharp::SourceLocation(nativesharp::FileId(2), 1),
            nativesharp::SourceLocation(nativesharp::FileId(3), 2));
        context.expect(!range.isValid(), "cross-file source range must be invalid");
    });
    addCase(registry, "TB-BAS-014", "CB-BAS-003", "unit", [](TestContext& context) {
        const nativesharp::SourceLocation valid(nativesharp::FileId(2), 1);
        const nativesharp::SourceLocation invalid;
        context.expect(!nativesharp::SourceRange(invalid, valid).isValid(), "range with invalid beginning must be invalid");
        context.expect(!nativesharp::SourceRange(valid, invalid).isValid(), "range with invalid end must be invalid");
    });
}

void registerCompilationEventCases(TestRegistry& registry)
{
    using nativesharp::basic::CompilationEventKind;
    addCase(registry, "TB-BAS-015", "CB-BAS-004", "unit", [](TestContext& context) {
        constexpr std::array expected{
            std::pair{CompilationEventKind::PhaseStarted, std::string_view("phase-started")},
            std::pair{CompilationEventKind::PhaseFinished, std::string_view("phase-finished")},
            std::pair{CompilationEventKind::WorkStarted, std::string_view("work-started")},
            std::pair{CompilationEventKind::WorkFinished, std::string_view("work-finished")},
            std::pair{CompilationEventKind::CacheHit, std::string_view("cache-hit")},
            std::pair{CompilationEventKind::CacheMiss, std::string_view("cache-miss")},
            std::pair{CompilationEventKind::Skipped, std::string_view("skipped")},
            std::pair{CompilationEventKind::Failed, std::string_view("failed")},
        };
        for (const auto& [kind, name] : expected)
        {
            context.expect(nativesharp::basic::compilationEventKindName(kind) == name, "compilation event kind name must be stable");
        }
    });
    addCase(registry, "TB-BAS-016", "CB-BAS-004", "unit", [](TestContext& context) {
        const auto invalid = static_cast<CompilationEventKind>(999);
        context.expect(nativesharp::basic::compilationEventKindName(invalid) == "unknown", "unknown event kind must use fallback name");
    });
    addCase(registry, "TB-BAS-017", "CB-BAS-004", "unit", [](TestContext& context) {
        const nativesharp::basic::CompilationEvent event;
        context.expect(event.kind == CompilationEventKind::PhaseStarted, "default event must start a phase");
        context.expect(event.phase.empty() && event.detail.empty(), "default event views must be empty");
        context.expect(event.current == 0 && event.total == 0, "default event progress must be zero");
        context.expect(event.duration == std::chrono::nanoseconds::zero(), "default event duration must be zero");
    });
    addCase(registry, "TB-BAS-018", "CB-BAS-004", "unit", [](TestContext& context) {
        const nativesharp::basic::CompilationEvent event{
            CompilationEventKind::WorkFinished,
            "frontend",
            "semantic-analysis",
            7,
            11,
            std::chrono::nanoseconds(1234),
        };
        context.expect(event.kind == CompilationEventKind::WorkFinished, "event must preserve kind");
        context.expect(event.phase == "frontend" && event.detail == "semantic-analysis", "event must preserve views");
        context.expect(event.current == 7 && event.total == 11, "event must preserve progress");
        context.expect(event.duration == std::chrono::nanoseconds(1234), "event must preserve duration");
    });
}

void registerExitCodeCase(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-019", "CB-BAS-005", "component", [](TestContext& context) {
        context.expect(static_cast<int>(nativesharp::ExitCode::Success) == 0, "success exit code must be 0");
        context.expect(static_cast<int>(nativesharp::ExitCode::CompilationError) == 1, "compilation exit code must be 1");
        context.expect(static_cast<int>(nativesharp::ExitCode::CommandLineError) == 2, "command-line exit code must be 2");
        context.expect(static_cast<int>(nativesharp::ExitCode::InternalError) == 70, "internal exit code must be 70");
    });
}
}

void registerBasicValuesModule(TestRegistry& registry)
{
    registerFileIdCases(registry);
    registerSourceLocationCases(registry);
    registerSourceRangeCases(registry);
    registerCompilationEventCases(registry);
    registerExitCodeCase(registry);
}
}
