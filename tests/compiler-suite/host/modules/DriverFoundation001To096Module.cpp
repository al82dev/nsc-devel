#include "DriverTestSupport.h"

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/CompilationGraphScheduler.h"
#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/CompilationProfile.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <array>
#include <chrono>
#include <stdexcept>

namespace nsc::suite
{
namespace
{
using namespace driver_support;
using namespace nativesharp::driver;

using CurrentDocument = std::string (*)();
using WriteDocument = bool (*)(const std::filesystem::path&, std::string&);
using VerifyDocument = bool (*)(const std::filesystem::path&, std::string&);

void documentContract(
    TestContext& context,
    std::size_t variant,
    std::string_view identity,
    CurrentDocument current,
    WriteDocument write,
    VerifyDocument verify)
{
    TemporaryTree tree(identity);
    const auto path = tree.root() / "nested" / "contract.txt";
    std::string error;
    if (variant == 3U)
    {
        context.expect(!verify(tree.root() / "missing.txt", error) && !error.empty(),
            "missing canonical document is rejected with a diagnostic");
        return;
    }
    if (variant == 4U)
    {
        tree.write("contract.txt", "invalid\n");
        context.expect(!verify(tree.root() / "contract.txt", error) && !error.empty(),
            "malformed canonical document is rejected exactly");
        return;
    }
    if (variant == 5U)
    {
        context.expect(!write({}, error) && !error.empty(),
            "empty canonical document destination is rejected");
        return;
    }
    const bool first = write(path, error);
    const std::string firstText = readText(path);
    const bool verified = verify(path, error);
    const bool second = write(path, error);
    context.expect(first && verified && second && firstText == current() && readText(path) == firstText,
        "canonical document write, verify, and overwrite are deterministic");
}

void ledgerContract(TestContext& context, std::size_t variant)
{
    documentContract(context, variant, "ledger", currentBreakingChangeLedger,
        writeBreakingChangeLedger, verifyBreakingChangeLedger);
}

void policyContract(TestContext& context, std::size_t variant)
{
    documentContract(context, variant, "policy", currentCompatibilityPolicy,
        writeCompatibilityPolicy, verifyCompatibilityPolicy);
}

void matrixContract(TestContext& context, std::size_t variant)
{
    documentContract(context, variant, "matrix", currentCompatibilityMatrix,
        writeCompatibilityMatrix, verifyCompatibilityMatrix);
}

void compatibilityContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("compatibility");
    std::string error;
    std::string explanation;
    if (variant == 3U)
    {
        tree.write("policy.txt", "NSCOMPAT 1\ncompiler_release=0\n");
        context.expect(!verifyInstalledCompatibilityCapabilities(tree.root() / "policy.txt", error) &&
                contains(error, "missing required capability"),
            "incomplete installed capability policy is rejected");
        return;
    }
    if (variant == 4U)
    {
        tree.write("policy.txt", currentCompatibilityPolicy() + "compiler_release=duplicate\n");
        context.expect(!verifyInstalledCompatibilityCapabilities(tree.root() / "policy.txt", error) &&
                contains(error, "duplicate"),
            "duplicate installed capability is rejected");
        return;
    }
    const auto policy = tree.write("policy.txt", currentCompatibilityPolicy());
    const std::string required = variant == 8U ? std::string(4096U, '9') : "0.0";
    const bool compatible = checkCompilerCompatibility(required, explanation, error);
    context.expect(verifyInstalledCompatibilityCapabilities(policy, error) && compatible && error.empty() &&
            contains(explanation, required) && contains(explanation, "capabilities"),
        "compiler labels remain informational while installed capabilities are exact");
}

void graphAdmissionContract(TestContext& context, std::size_t variant)
{
    CompilationGraphScheduler graph;
    if (variant == 3U)
    {
        bool rejected = false;
        try { graph.addTask("", {}, [](std::size_t) {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        context.expect(rejected, "empty task key is rejected at admission");
        return;
    }
    const std::size_t first = graph.addTask("first", {}, [](std::size_t) {});
    if (variant == 4U)
    {
        const std::array duplicate{first, first};
        bool rejected = false;
        try { graph.addTask("duplicate", duplicate, [](std::size_t) {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        context.expect(rejected && graph.taskCount() == 1U,
            "duplicate dependency is rejected without partial admission");
        return;
    }
    if (variant == 5U)
    {
        const std::array future{first + 1U};
        bool rejected = false;
        try { graph.addTask("future", future, [](std::size_t) {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        context.expect(rejected && graph.taskCount() == 1U,
            "future dependency is rejected without partial admission");
        return;
    }
    const std::array dependency{first};
    const std::size_t count = variant == 8U ? 1024U : 2U;
    for (std::size_t index = 1U; index < count; ++index)
    {
        const std::array previous{index - 1U};
        graph.addTask("task-" + std::to_string(index), previous, [](std::size_t) {});
    }
    context.expect(first == 0U && graph.taskCount() == count && dependency.front() == 0U,
        "task ordinals and dependency topology remain insertion-stable");
}

void graphExecutionContract(TestContext& context, std::size_t variant)
{
    CompilationGraphScheduler graph;
    std::atomic_size_t executions{0U};
    const std::size_t first = graph.addTask("z-root", {}, [&executions](std::size_t) { ++executions; });
    graph.addTask("a-root", {}, [variant, &executions](std::size_t)
    {
        ++executions;
        if (variant == 3U) throw std::runtime_error("directed failure");
    });
    const std::array dependency{first};
    graph.addTask("dependent", dependency, [&executions](std::size_t) { ++executions; });
    CompilationWorkerPool pool(variant == 8U ? 8U : 2U);
    std::string error;
    const bool result = graph.run(pool, error);
    if (variant == 3U)
    {
        context.expect(!result && contains(error, "directed failure"),
            "worker failure aborts the current graph wave with exact cause");
        return;
    }
    const auto order = graph.submissionOrder();
    context.expect(result && error.empty() && executions == 3U && order.size() == 3U &&
            order[0] == 1U && order[1] == 0U && order[2] == 2U,
        "ready waves are key-sorted and dependencies execute after their producer");
}

void jobPolicyContract(TestContext& context, std::size_t variant)
{
    CompilationJobPolicy policy(variant == 3U ? 0U : 8U);
    if (variant == 4U) policy.setReservedCores(20U);
    if (variant == 5U) policy.setRequestedJobs(kMaximumCompilationJobs + 1U);
    if (variant == 6U) policy.setRequestedJobs(3U);
    const std::size_t expected = variant == 4U ? 1U : variant == 5U ? 7U :
        variant == 6U ? 3U : variant == 3U ? 1U : 7U;
    context.expect(policy.hardwareThreads() == (variant == 3U ? 1U : 8U) &&
            policy.effectiveJobs() == expected &&
            policy.usesAutomaticJobs() == (variant != 5U && variant != 6U),
        "hardware normalization, reservation, and requested-job clamp are exact");
}

void jobScopeContract(TestContext& context, std::size_t variant)
{
    CompilationJobPolicy outer(4U);
    CompilationJobPolicy inner(8U);
    const auto* before = activeCompilationJobPolicy();
    bool nested = false;
    {
        CompilationJobPolicyScope outerScope(outer);
        nested = activeCompilationJobPolicy() == &outer;
        if (variant != 3U)
        {
            CompilationJobPolicyScope innerScope(inner);
            nested = nested && activeCompilationJobPolicy() == &inner;
        }
        nested = nested && activeCompilationJobPolicy() == &outer;
    }
    context.expect(nested && activeCompilationJobPolicy() == before,
        "nested thread-local job policy scopes restore the exact previous pointer");
}

void profileContract(TestContext& context, std::size_t variant)
{
    using namespace nativesharp::llvm_backend;
    if (variant == 3U)
    {
        context.expect(!parseCompilationProfile("Debug").has_value() &&
                compilationProfileName(static_cast<CompilationProfile>(99)).empty(),
            "profile parsing is canonical and unknown enum names are empty");
        return;
    }
    const auto profile = variant == 2U ? CompilationProfile::Release : CompilationProfile::Debug;
    const auto resolved = resolveCompilationProfile(profile,
        variant == 5U ? std::optional(LlvmOptimizationLevel::O3) : std::nullopt,
        variant == 6U ? std::optional(DebugInformationKind::None) : std::nullopt);
    const auto expectedOptimization = variant == 5U ? LlvmOptimizationLevel::O3 :
        profile == CompilationProfile::Debug ? LlvmOptimizationLevel::O0 : LlvmOptimizationLevel::O2;
    context.expect(parseCompilationProfile(compilationProfileName(profile)) == profile &&
            resolved.optimizationLevel == expectedOptimization &&
            (variant != 6U || resolved.debugInformation == DebugInformationKind::None),
        "profile defaults and explicit optimization/debug overrides compose exactly");
}

void timingAggregationContract(TestContext& context, std::size_t variant)
{
    std::string input = variant == 3U ? "timing phase=bad us=x\n" :
        "timing phase=parse us=12 peak_working_set_bytes=34\n"
        "timing phase=emit us=56 peak_working_set_bytes=78\n"
        "timing total_ms=9 peak_working_set_bytes=80\n";
    if (variant == 8U)
    {
        input.clear();
        for (std::size_t index = 0; index < 1000U; ++index)
            input += "timing phase=p" + std::to_string(index) + " us=1\n";
    }
    const std::string json = renderCompilationTimeTraceJson(input, 7);
    context.expect(contains(json, "\"exit_code\": 7") &&
            (variant == 3U ? contains(json, "\"phases\": []") :
             variant == 8U ? contains(json, "\"name\": \"p999\"") :
             contains(json, "\"name\": \"parse\"") && contains(json, "\"total_ms\": 9")),
        "time trace accepts only complete numeric records and aggregates canonical phases");
}

void timingJsonContract(TestContext& context, std::size_t variant)
{
    const std::string input = variant == 3U ? "timing total_ms=-1\n" :
        "timing total_ms=4 peak_working_set_bytes=8\n"
        "timing project_descriptor_file_reads=3 project_descriptor_cache_hits=2\n";
    const std::string first = renderCompilationTimeTraceJson(input, variant == 4U ? -2 : 0);
    const std::string second = renderCompilationTimeTraceJson(input, variant == 4U ? -2 : 0);
    context.expect(first == second && contains(first, "\"schema\": \"nativesharp-time-trace\"") &&
            contains(first, variant == 3U ? "\"total_ms\": 0" : "\"project_descriptor_cache_hits\": 2"),
        "time trace JSON has stable schema, numeric fallback, and process counters");
}

void progressContract(TestContext& context, std::size_t variant)
{
    ProgressFormat format = ProgressFormat::Text;
    if (variant == 3U)
    {
        context.expect(!parseProgressFormat("XML", format) && format == ProgressFormat::Text,
            "unknown progress format is rejected without changing the output parameter");
        return;
    }
    const bool json = variant % 2U == 0U;
    const bool parsed = parseProgressFormat(json ? "json" : "text", format);
    const nativesharp::basic::CompilationEvent event{
        variant == 5U ? nativesharp::basic::CompilationEventKind::Failed :
            nativesharp::basic::CompilationEventKind::WorkFinished,
        "parse", "unit\"42", 2U, 3U, std::chrono::microseconds(17)};
    const std::string rendered = renderCompilationEvent(event, format);
    context.expect(parsed && (json ? contains(rendered, "\"schema\":\"nativesharp-progress-event\"") &&
            contains(rendered, "unit\\\"42") : contains(rendered, "NSC parse")),
        "progress parsing and text/JSON event rendering preserve exact fields");
}
}

void registerDriverFoundation001To096Module(TestRegistry& registry)
{
    addDriverContract(registry, 1U, "unit", ledgerContract);
    addDriverContract(registry, 2U, "unit", policyContract);
    addDriverContract(registry, 3U, "unit", matrixContract);
    addDriverContract(registry, 4U, "unit", compatibilityContract);
    addDriverContract(registry, 5U, "unit", graphAdmissionContract);
    addDriverContract(registry, 6U, "integration", graphExecutionContract);
    addDriverContract(registry, 7U, "unit", jobPolicyContract);
    addDriverContract(registry, 8U, "unit", jobScopeContract);
    addDriverContract(registry, 9U, "unit", profileContract);
    addDriverContract(registry, 10U, "unit", timingAggregationContract);
    addDriverContract(registry, 11U, "unit", timingJsonContract);
    addDriverContract(registry, 12U, "unit", progressContract);
}
}
