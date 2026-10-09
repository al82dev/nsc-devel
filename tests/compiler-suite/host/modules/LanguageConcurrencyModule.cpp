#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CL-LANG-005", "Host.Language", "unit",
        std::move(profile), {"language", "concurrency"}, std::move(function)});
}

void registerStructuralThreadSafetyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-069", [](TestContext& context) {
        for (const bool fieldsSend : std::array{false, true})
        {
            for (const bool fieldsSync : std::array{false, true})
            {
                for (const bool rawPointer : std::array{false, true})
                {
                    for (const bool negativeSend : std::array{false, true})
                    {
                        for (const bool negativeSync : std::array{false, true})
                        {
                            const ThreadSafetyFacts facts{
                                fieldsSend, fieldsSync, rawPointer, negativeSend, negativeSync};
                            context.expect(
                                structurallySend(facts) == (fieldsSend && !rawPointer && !negativeSend),
                                "TL-LANG-069 structural Send must match the complete boolean matrix");
                            context.expect(
                                structurallySync(facts) == (fieldsSync && !rawPointer && !negativeSync),
                                "TL-LANG-069 structural Sync must match the complete boolean matrix");
                        }
                    }
                }
            }
        }
    });
}

ThreadSafetyDecision evaluateSingleMember(
    ThreadSafetyMemberKind kind,
    bool send,
    bool sync,
    bool dynamicSend = false,
    bool dynamicSync = false)
{
    ThreadSafetyTypeFacts facts;
    facts.typeName = "Type";
    facts.members.push_back(ThreadSafetyMemberFacts{
        "member", kind, send, sync, dynamicSend, dynamicSync});
    return evaluateThreadSafety(facts);
}

void registerThreadSafetyDecisionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-070", [](TestContext& context) {
        ThreadSafetyTypeFacts facts;
        ThreadSafetyDecision decision = evaluateThreadSafety(facts);
        context.expect(!decision.send && !decision.sync &&
                decision.reason == "thread-safety derivation requires a resolved type identity",
            "TL-LANG-070 unresolved identity must have first rejection priority");

        facts.typeName = "Empty";
        decision = evaluateThreadSafety(facts);
        context.expect(decision.send && decision.sync && decision.reason.empty(),
            "TL-LANG-070 an empty resolved aggregate must derive Send and Sync");

        decision = evaluateSingleMember(ThreadSafetyMemberKind::Atomic, false, false);
        context.expect(decision.send && decision.sync && decision.reason.empty(),
            "TL-LANG-070 atomic members must be structurally Send and Sync");
        decision = evaluateSingleMember(ThreadSafetyMemberKind::Value, false, true);
        context.expect(!decision.send && decision.sync &&
                decision.reason == "one or more members do not satisfy structural Send/Sync",
            "TL-LANG-070 value members must preserve independent Send and Sync facts");
        decision = evaluateSingleMember(ThreadSafetyMemberKind::SharedReference, true, false);
        context.expect(!decision.send && !decision.sync,
            "TL-LANG-070 shared references must require Sync for both decisions");
        decision = evaluateSingleMember(ThreadSafetyMemberKind::MutableReference, false, true);
        context.expect(!decision.send && decision.sync,
            "TL-LANG-070 mutable references must use independent member facts");
        decision = evaluateSingleMember(ThreadSafetyMemberKind::RawPointer, true, true);
        context.expect(!decision.send && !decision.sync,
            "TL-LANG-070 raw pointers must fail closed for both decisions");
        decision = evaluateSingleMember(ThreadSafetyMemberKind::DynamicTrait, true, true, true, false);
        context.expect(decision.send && !decision.sync,
            "TL-LANG-070 dynamic traits must use their explicit dynamic guarantees");

        facts = ThreadSafetyTypeFacts{};
        facts.typeName = "Unsafe";
        facts.members.push_back({"raw", ThreadSafetyMemberKind::RawPointer, true, true, false, false});
        facts.unsafeSendImpl = true;
        facts.unsafeSyncImpl = true;
        decision = evaluateThreadSafety(facts);
        context.expect(!decision.send && !decision.sync && !decision.usedUnsafeSend &&
                !decision.usedUnsafeSync &&
                decision.reason == "unsafe Send/Sync implementation requires an audited safety proof",
            "TL-LANG-070 unreviewed unsafe overrides must not change derivation");
        facts.unsafeProofReviewed = true;
        decision = evaluateThreadSafety(facts);
        context.expect(decision.send && decision.sync && decision.usedUnsafeSend &&
                decision.usedUnsafeSync && decision.reason.empty(),
            "TL-LANG-070 reviewed unsafe overrides must be recorded exactly");
        facts.negativeSendImpl = true;
        decision = evaluateThreadSafety(facts);
        context.expect(!decision.send && decision.sync && !decision.usedUnsafeSend &&
                decision.usedUnsafeSync &&
                decision.reason == "negative Send/Sync implementation opts the type out structurally",
            "TL-LANG-070 a negative implementation must override a reviewed unsafe grant");
        facts.unsafeProofReviewed = false;
        decision = evaluateThreadSafety(facts);
        context.expect(decision.reason == "unsafe Send/Sync implementation requires an audited safety proof",
            "TL-LANG-070 missing unsafe proof must precede the negative-implementation reason");

        facts = ThreadSafetyTypeFacts{};
        facts.typeName = "Closure";
        facts.asyncClosure = true;
        facts.members.push_back({"capture", ThreadSafetyMemberKind::Value, false, true, false, false});
        decision = evaluateThreadSafety(facts);
        context.expect(!decision.send && decision.sync &&
                decision.reason == "async closure captures do not satisfy structural Send/Sync",
            "TL-LANG-070 async closure failures must use their specific diagnostic");
    });
}

void registerThreadSafetyMetadataCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-071", [](TestContext& context) {
        ThreadSafetyDecision source;
        source.send = true;
        source.sync = false;
        source.usedUnsafeSend = true;
        source.usedUnsafeSync = false;
        source.reason = "not serialized";
        ThreadSafetyMetadata metadata = exportThreadSafetyMetadata("Widget", source);
        context.expect(metadata.contractMajor == kConcurrencyContractMajor &&
                metadata.contractMinor == kConcurrencyContractMinor && metadata.typeName == "Widget" &&
                metadata.send && !metadata.sync && metadata.unsafeSend && !metadata.unsafeSync,
            "TL-LANG-071 export must preserve the versioned decision fields exactly");
        std::string error = "stale";
        auto imported = importThreadSafetyMetadata(metadata, error);
        context.expect(imported.has_value() && error.empty() && imported->send && !imported->sync &&
                imported->usedUnsafeSend && !imported->usedUnsafeSync && imported->reason.empty(),
            "TL-LANG-071 a valid round trip must preserve decision fields only");
        metadata.contractMinor = static_cast<std::uint16_t>(kConcurrencyContractMinor + 7);
        imported = importThreadSafetyMetadata(metadata, error);
        context.expect(imported.has_value() && error.empty(),
            "TL-LANG-071 a newer minor metadata version must remain compatible");
        metadata.contractMajor = static_cast<std::uint16_t>(kConcurrencyContractMajor + 1);
        context.expect(!importThreadSafetyMetadata(metadata, error).has_value() &&
                error == "incompatible Send/Sync metadata contract major version",
            "TL-LANG-071 a different major version must use the exact error");
        metadata.contractMajor = kConcurrencyContractMajor;
        metadata.typeName.clear();
        context.expect(!importThreadSafetyMetadata(metadata, error).has_value() &&
                error == "Send/Sync metadata has no type identity",
            "TL-LANG-071 missing type identity must reject without a decision");
    });
}

void registerTaskBorrowCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-072", [](TestContext& context) {
        for (const bool joined : std::array{false, true})
        {
            for (const bool outlives : std::array{false, true})
            {
                context.expect(taskBorrowMayEscapeScope(joined, outlives) == (joined && outlives),
                    "TL-LANG-072 task borrow escape must require both independent proofs");
            }
        }
    });
}

void registerTaskScopePlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-073", [](TestContext& context) {
        const TaskExecutorProtocol complete{"Executor", true, true, true, true, true};
        const std::array children{
            ScopedTask{"plain", false, false, false},
            ScopedTask{"borrowed", true, true, true}};
        std::string error = "stale";
        const auto plan = buildTaskScopePlan(complete, children, 3, true, error);
        context.expect(plan.has_value() && error.empty() && plan->executorType == "Executor" &&
                plan->children.size() == 2 && plan->nestingDepth == 3 && plan->noHeap &&
                !plan->fixesThreadPool,
            "TL-LANG-073 a complete executor and proven children must preserve the exact plan");

        std::array incomplete{
            TaskExecutorProtocol{"", true, true, true, true, true},
            TaskExecutorProtocol{"Executor", false, true, true, true, true},
            TaskExecutorProtocol{"Executor", true, false, true, true, true},
            TaskExecutorProtocol{"Executor", true, true, false, true, true},
            TaskExecutorProtocol{"Executor", true, true, true, false, true}};
        for (const TaskExecutorProtocol& executor : incomplete)
        {
            context.expect(!buildTaskScopePlan(executor, {}, 0, false, error).has_value() &&
                    error == "task scope requires a named static executor with spawn, poll/join, and cancel",
                "TL-LANG-073 every incomplete executor must use the exact protocol error");
        }
        TaskExecutorProtocol heapOnly = complete;
        heapOnly.supportsNoHeap = false;
        context.expect(!buildTaskScopePlan(heapOnly, {}, 0, true, error).has_value() &&
                error == "noheap task scope requires an executor with explicit noheap support",
            "TL-LANG-073 noheap must reject an executor without explicit support");
        const std::array unnamed{ScopedTask{"", false, false, true}};
        context.expect(!buildTaskScopePlan(complete, unnamed, 0, false, error).has_value() &&
                error == "every scoped task requires a stable identity",
            "TL-LANG-073 an unnamed child must reject without a partial plan");
        for (const auto& [joined, outlives] : std::array{
                 std::pair{false, false}, std::pair{false, true}, std::pair{true, false}})
        {
            const std::array borrowed{ScopedTask{"child", true, outlives, joined}};
            context.expect(!buildTaskScopePlan(complete, borrowed, 0, false, error).has_value() &&
                    error == "borrowed child task must be joined before scope exit and fit the scope lifetime",
                "TL-LANG-073 each missing child-borrow proof must use the exact error");
        }
        const std::array unborrowed{ScopedTask{"child", false, false, false}};
        context.expect(buildTaskScopePlan(complete, unborrowed, 0, false, error).has_value() && error.empty(),
            "TL-LANG-073 a child without a scope borrow must not require borrow proofs");
    });
}

bool taskActionMatches(
    const TaskScopeAction& action,
    TaskScopeActionKind kind,
    std::string_view child)
{
    return action.kind == kind && action.child == child;
}

void registerTaskScopeExitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-074", [](TestContext& context) {
        TaskScopePlan scope;
        scope.children = {
            ScopedTask{"first", false, false, true},
            ScopedTask{"second", false, false, true},
            ScopedTask{"third", false, false, true}};
        const std::array completed{std::string("second")};
        auto actions = planTaskScopeExit(scope, completed, TaskScopeExitReason::Normal);
        context.expect(actions.size() == 2 &&
                taskActionMatches(actions[0], TaskScopeActionKind::Join, "third") &&
                taskActionMatches(actions[1], TaskScopeActionKind::Join, "first"),
            "TL-LANG-074 normal exit must join incomplete children in reverse order");
        const std::array abnormal{
            std::pair{TaskScopeExitReason::EarlyError, TaskScopeActionKind::PropagateError},
            std::pair{TaskScopeExitReason::Cancellation, TaskScopeActionKind::PropagateCancellation},
            std::pair{TaskScopeExitReason::Panic, TaskScopeActionKind::PropagatePanic}};
        for (const auto& [reason, propagation] : abnormal)
        {
            actions = planTaskScopeExit(scope, completed, reason);
            context.expect(actions.size() == 5 &&
                    taskActionMatches(actions[0], TaskScopeActionKind::Cancel, "third") &&
                    taskActionMatches(actions[1], TaskScopeActionKind::Cancel, "first") &&
                    taskActionMatches(actions[2], TaskScopeActionKind::Join, "third") &&
                    taskActionMatches(actions[3], TaskScopeActionKind::Join, "first") &&
                    taskActionMatches(actions[4], propagation, ""),
                "TL-LANG-074 abnormal exit must cancel, join, then propagate in exact order");
        }
        const std::array allCompleted{
            std::string("first"), std::string("second"), std::string("third")};
        actions = planTaskScopeExit(scope, allCompleted, TaskScopeExitReason::EarlyError);
        context.expect(actions.size() == 1 &&
                taskActionMatches(actions[0], TaskScopeActionKind::PropagateError, ""),
            "TL-LANG-074 completed children must not be cancelled or joined again");
    });
}

std::string expectedConcurrencyManifest()
{
    return
        "NSCONCURRENCY 1.0\n"
        "target=x86_64-pc-windows-msvc\n"
        "scheduler=explicit-job-system-only\n"
        "parallel-policy=caller-supplied-job-system+threshold+cancel\n"
        "future-start=lazy\n"
        "frame-storage=caller-stack|caller-arena|explicit-heap\n"
        "implicit-heap=no\n"
        "poll=single-consumer-no-resume-after-terminal\n"
        "cancellation=cooperative-no-thread-abort\n"
        "cancellation-cleanup=defers-reverse-then-locals-reverse\n"
        "task-scope=borrow-requires-join-before-scope-exit\n"
        "send-sync=structural-fail-closed+raw-pointer-no-derive\n"
        "atomic-orders=relaxed|acquire|release|acq-rel|seq-cst\n"
        "atomic-load=relaxed|acquire|seq-cst\n"
        "atomic-store=relaxed|release|seq-cst\n"
        "atomic-rmw=all-orders\n"
        "volatile-synchronization=no\n"
        "async-generator=explicit-backpressure\n";
}

void registerConcurrencyIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-075", [](TestContext& context) {
        const std::string firstManifest = concurrencyContractManifest();
        const std::string secondManifest = concurrencyContractManifest();
        const std::string firstFingerprint = concurrencyContractFingerprintHex();
        const std::string secondFingerprint = concurrencyContractFingerprintHex();
        context.expect(firstManifest == expectedConcurrencyManifest() && secondManifest == firstManifest &&
                firstManifest.size() == 695,
            "TL-LANG-075 manifest bytes must equal the frozen concurrency contract");
        context.expect(firstFingerprint == "7cb427840ab173ff" && secondFingerprint == firstFingerprint,
            "TL-LANG-075 repeated fingerprints must equal the independently frozen FNV value");
    });
}

class ConcurrencyFixtureTree final
{
public:
    ConcurrencyFixtureTree()
        : root_(std::filesystem::current_path() /
                (".nsc-suite-TL-LANG-076-" + std::to_string(GetCurrentProcessId())))
    {
        if (!std::filesystem::create_directory(root_))
            throw std::runtime_error("cannot create isolated concurrency fixture: " + root_.string());
        tracked_.push_back(root_);
    }

    ConcurrencyFixtureTree(const ConcurrencyFixtureTree&) = delete;
    ConcurrencyFixtureTree& operator=(const ConcurrencyFixtureTree&) = delete;

    ~ConcurrencyFixtureTree()
    {
        std::error_code ignored;
        for (auto path = tracked_.rbegin(); path != tracked_.rend(); ++path)
        {
            std::filesystem::remove(*path, ignored);
            ignored.clear();
        }
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    void track(const std::filesystem::path& path) { tracked_.push_back(path); }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

void writeFixture(const std::filesystem::path& path, std::string_view bytes)
{
    // NOLINTNEXTLINE(bugprone-signed-bitwise) -- standard openmode flags are a bitmask enum.
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) throw std::runtime_error("cannot write concurrency fixture: " + path.string());
}

std::string withCrlf(std::string_view text)
{
    std::string result;
    result.reserve(text.size() * 2);
    for (const char byte : text)
    {
        if (byte == '\n') result.push_back('\r');
        result.push_back(byte);
    }
    return result;
}

void registerConcurrencyFileCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-076", [](TestContext& context) {
        ConcurrencyFixtureTree tree;
        std::string error = "stale";
        const auto parent = tree.root() / "created";
        const auto nested = parent / "nested";
        const auto contract = nested / "concurrency.contract";
        context.expect(writeConcurrencyContract(contract, error) && error.empty() &&
                std::filesystem::is_regular_file(contract),
            "TL-LANG-076 writing must create absent parents and exact contract bytes");
        tree.track(parent);
        tree.track(nested);
        tree.track(contract);
        context.expect(verifyConcurrencyContractFile(contract, error) && error.empty(),
            "TL-LANG-076 a freshly written contract must verify");

        const auto crlf = tree.root() / "crlf.contract";
        tree.track(crlf);
        writeFixture(crlf, withCrlf(expectedConcurrencyManifest()));
        context.expect(verifyConcurrencyContractFile(crlf, error) && error.empty(),
            "TL-LANG-076 verification must normalize CRLF to canonical LF");

        const auto altered = tree.root() / "altered.contract";
        tree.track(altered);
        writeFixture(altered, expectedConcurrencyManifest() + "altered=yes\n");
        context.expect(!verifyConcurrencyContractFile(altered, error) &&
                error == "incompatible concurrency contract '" + altered.string() +
                    "': expected fingerprint 7cb427840ab173ff",
            "TL-LANG-076 altered bytes must use the exact fingerprint diagnostic");
        context.expect(!writeConcurrencyContract({}, error) &&
                error == "concurrency contract destination is empty",
            "TL-LANG-076 an empty destination must fail without filesystem effects");
        const auto missing = tree.root() / "missing.contract";
        context.expect(!verifyConcurrencyContractFile(missing, error) &&
                error == "cannot open concurrency contract '" + missing.string() + "'",
            "TL-LANG-076 a missing file must use the exact open diagnostic");
    }, "NIGHTLY");
}

void registerIndependentConcurrencyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-077", [](TestContext& context) {
        constexpr std::size_t workerCount = 8;
        std::array<int, workerCount> results{};
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (std::size_t index = 0; index < workerCount; ++index)
        {
            workers.emplace_back([index, &results] {
                std::string error;
                FutureState future;
                const PollOutcome outcome = index % 2 == 0
                    ? PollOutcome::ReadyValue : PollOutcome::ReadyError;
                const FutureCompletion completion = index % 2 == 0
                    ? FutureCompletion::Value : FutureCompletion::Error;
                if (!future.poll(outcome, error) || !error.empty() ||
                    future.completion() != completion || !future.consume(error)) return;

                ThreadSafetyDecision decision;
                decision.send = index % 3 != 0;
                decision.sync = index % 3 == 0;
                const std::string typeName = "ConcurrentType" + std::to_string(index);
                const ThreadSafetyMetadata metadata = exportThreadSafetyMetadata(typeName, decision);
                const auto imported = importThreadSafetyMetadata(metadata, error);
                if (!imported.has_value() || !error.empty() || metadata.typeName != typeName ||
                    imported->send != decision.send || imported->sync != decision.sync) return;

                AsyncGeneratorProtocolState generator;
                if (!generator.requestNext(error) ||
                    !generator.resume(AsyncGeneratorPollOutcome::ItemReady, error) ||
                    !generator.takeItem(error) || generator.state() != AsyncGeneratorState::Pending) return;
                results[index] = 1;
            });
        }
        for (std::thread& worker : workers) worker.join();
        for (const int result : results)
            context.expect(result == 1,
                "TL-LANG-077 independent concurrent state and metadata operations must not interfere");
    }, "NIGHTLY");
}
}

void registerLanguageConcurrencyModule(TestRegistry& registry)
{
    registerStructuralThreadSafetyCase(registry);
    registerThreadSafetyDecisionCase(registry);
    registerThreadSafetyMetadataCase(registry);
    registerTaskBorrowCase(registry);
    registerTaskScopePlanCase(registry);
    registerTaskScopeExitCase(registry);
    registerConcurrencyIdentityCase(registry);
    registerConcurrencyFileCase(registry);
    registerIndependentConcurrencyCase(registry);
}
}
