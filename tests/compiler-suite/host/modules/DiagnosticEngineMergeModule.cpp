#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::diagnostics::Diagnostic;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticSeverity;

// Registers one case under the deterministic-merge contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-007", "Host.Diagnostics", "component",
        std::move(profile), {"diagnostics", "engine", "merge"}, std::move(function)});
}

// Extracts the observable message sequence from an engine. Complexity is O(D).
std::vector<std::string> messages(const DiagnosticEngine& engine)
{
    std::vector<std::string> result;
    result.reserve(engine.diagnostics().size());
    for (const Diagnostic& diagnostic : engine.diagnostics())
        result.push_back(diagnostic.message());
    return result;
}

// Reports a distinguishable record without bypassing the shard's own budget. Complexity is O(message size).
void report(DiagnosticEngine& engine, DiagnosticId id, DiagnosticSeverity severity,
    SourceLocation location, std::string message)
{
    engine.report(id, severity, location, std::move(message));
}

void registerShardPermutationCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-032", "PR", [](TestContext& context) {
        DiagnosticEngine early;
        DiagnosticEngine late;
        report(early, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(1), 10), "early");
        report(late, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(2), 20), "late");
        const std::array<DiagnosticEngine, 2> forward{early, late};
        const std::array<DiagnosticEngine, 2> reverse{late, early};
        DiagnosticEngine firstResult;
        DiagnosticEngine secondResult;
        firstResult.mergeDeterministically(forward);
        secondResult.mergeDeterministically(reverse);
        context.expect(messages(firstResult) == std::vector<std::string>({"early", "late"}),
            "TD-DIA-032 distinct keys must produce their canonical sequence");
        context.expect(messages(firstResult) == messages(secondResult),
            "TD-DIA-032 distinct keys must ignore shard arrival order");
    });
}

void registerLocationValidityCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-033", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(), "invalid location");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(8), 90), "valid location");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"valid location", "invalid location"}),
            "TD-DIA-033 valid locations must sort before invalid locations");
    });
}

void registerLocationTupleCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-034", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        shard.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceRange(SourceLocation(FileId(2), 1), SourceLocation(FileId(2), 2)), "file two");
        shard.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceRange(SourceLocation(FileId(1), 10), SourceLocation(FileId(1), 20)), "end twenty");
        shard.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceRange(SourceLocation(FileId(1), 10), SourceLocation(FileId(1), 15)), "end fifteen");
        shard.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceRange(SourceLocation(FileId(1), 5), SourceLocation(FileId(1), 6)), "start five");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        const std::vector<std::string> expected{
            "start five", "end fifteen", "end twenty", "file two"};
        context.expect(messages(receiver) == expected,
            "TD-DIA-034 FileId, start and end must form the leading lexicographic key");
    });
}

void registerSeverityOrderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-035", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        const SourceLocation location(FileId(9), 12);
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Note, location, "note");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Lint, location, "lint");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Warning, location, "warning");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error, location, "error");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        const std::vector<std::string> expected{"error", "warning", "lint", "note"};
        context.expect(messages(receiver) == expected,
            "TD-DIA-035 canonical severity order must be Error, Warning, Lint, Note");
    });
}

void registerDiagnosticIdOrderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-036", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        const SourceLocation location(FileId(10), 13);
        report(shard, DiagnosticId::InvalidCall, DiagnosticSeverity::Warning, location, "later ID");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Warning, location, "earlier ID");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"earlier ID", "later ID"}),
            "TD-DIA-036 equal locations and severities must sort by enum value");
    });
}

void registerMessageOrderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-037", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        const SourceLocation location(FileId(11), 14);
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Warning, location, "zeta");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Warning, location, "alpha");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"alpha", "zeta"}),
            "TD-DIA-037 the final key field must order equal records by message");
    });
}

void registerAbsentRangeEndCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-038", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        const SourceLocation location(FileId(12), 15);
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Warning, location, "without range");
        shard.report(DiagnosticId::InvalidType, DiagnosticSeverity::Warning,
            SourceRange(location, SourceLocation(FileId(12), 25)), "with range");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver;
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"without range", "with range"}),
            "TD-DIA-038 an absent range must use location.offset as its end key");
    });
}

void registerExistingReceiverPrefixCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-039", "PR", [](TestContext& context) {
        DiagnosticEngine receiver;
        report(receiver, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(99), 99), "existing receiver record");
        DiagnosticEngine shard;
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(1), 1), "merged record");
        const std::array<DiagnosticEngine, 1> shards{shard};
        receiver.mergeDeterministically(shards);
        const std::vector<std::string> expected{"existing receiver record", "merged record"};
        context.expect(messages(receiver) == expected,
            "TD-DIA-039 merge must preserve the receiver prefix and append the locally sorted input");
    });
}

void registerReceiverBudgetCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-040", "PR", [](TestContext& context) {
        DiagnosticEngine shard;
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(13), 30), "third");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(13), 10), "first");
        report(shard, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(13), 20), "second");
        const std::array<DiagnosticEngine, 1> shards{shard};
        DiagnosticEngine receiver(2);
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"first", "second"}),
            "TD-DIA-040 the receiver must accept the canonical prefix up to its budget");
        context.expect(receiver.errorCount() == 2 && receiver.suppressedDiagnosticCount() == 1,
            "TD-DIA-040 the receiver budget must count accepted and suppressed records once");
    });
}

void registerCombinedSuppressionCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-041", "PR", [](TestContext& context) {
        DiagnosticEngine first(1);
        DiagnosticEngine second(1);
        report(first, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(14), 10), "first accepted");
        report(first, DiagnosticId::InvalidType, DiagnosticSeverity::Warning,
            SourceLocation(FileId(14), 11), "first suppressed");
        report(second, DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(14), 20), "second accepted");
        report(second, DiagnosticId::InvalidType, DiagnosticSeverity::Note,
            SourceLocation(FileId(14), 21), "second suppressed");
        const std::array<DiagnosticEngine, 2> shards{first, second};
        DiagnosticEngine receiver(1);
        receiver.mergeDeterministically(shards);
        context.expect(messages(receiver) == std::vector<std::string>({"first accepted"}),
            "TD-DIA-041 the receiver must keep only its accepted canonical prefix");
        context.expect(receiver.suppressedDiagnosticCount() == 3,
            "TD-DIA-041 suppression must sum two shard rejections and one receiver rejection");
    });
}

std::vector<std::string> sortedLabelMessages(const DiagnosticEngine& engine)
{
    std::vector<std::string> result;
    for (const Diagnostic& diagnostic : engine.diagnostics())
    {
        if (!diagnostic.secondaryLabels().empty())
            result.push_back(diagnostic.secondaryLabels().front().message);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void registerFullyTiedSecondaryDataCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-042", "NIGHTLY", [](TestContext& context) {
        const SourceRange primary(SourceLocation(FileId(15), 10), SourceLocation(FileId(15), 20));
        const SourceRange secondary(SourceLocation(FileId(15), 1), SourceLocation(FileId(15), 2));
        DiagnosticEngine left;
        DiagnosticEngine right;
        left.reportStructured(DiagnosticId::InvalidType, DiagnosticSeverity::Warning,
            primary, "tied", {{secondary, "left"}}, {});
        right.reportStructured(DiagnosticId::InvalidType, DiagnosticSeverity::Warning,
            primary, "tied", {{secondary, "right"}}, {});
        const std::array<DiagnosticEngine, 2> forward{left, right};
        const std::array<DiagnosticEngine, 2> reverse{right, left};
        DiagnosticEngine firstResult;
        DiagnosticEngine secondResult;
        firstResult.mergeDeterministically(forward);
        secondResult.mergeDeterministically(reverse);
        const std::vector<std::string> expected{"left", "right"};
        context.expect(sortedLabelMessages(firstResult) == expected,
            "TD-DIA-042 a full-key tie must preserve both secondary payloads");
        context.expect(sortedLabelMessages(secondResult) == expected,
            "TD-DIA-042 the reverse permutation must preserve both secondary payloads");
    });
}
}

// Registers the complete CD-DIA-007 merge matrix, including its declared non-total-order boundary.
void registerDiagnosticEngineMergeModule(TestRegistry& registry)
{
    registerShardPermutationCase(registry);
    registerLocationValidityCase(registry);
    registerLocationTupleCase(registry);
    registerSeverityOrderCase(registry);
    registerDiagnosticIdOrderCase(registry);
    registerMessageOrderCase(registry);
    registerAbsentRangeEndCase(registry);
    registerExistingReceiverPrefixCase(registry);
    registerReceiverBudgetCase(registry);
    registerCombinedSuppressionCase(registry);
    registerFullyTiedSecondaryDataCase(registry);
}
}
