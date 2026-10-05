#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/IterationSemantics.h"

namespace nativesharp::language
{
inline constexpr std::uint16_t kConcurrencyContractMajor = 1;
inline constexpr std::uint16_t kConcurrencyContractMinor = 0;

enum class FutureStart { Lazy, Eager };
enum class FrameStorage { CallerStack, CallerArena, ExplicitHeap };
FutureStart nativeFutureStart() noexcept;
bool asyncFrameHasImplicitHeap(FrameStorage storage) noexcept;
bool borrowMayCrossAwait(bool sourceOutlivesFrame, bool addressStable) noexcept;

struct AwaitSuspension final
{
    std::size_t state = 0;
    std::size_t cleanupDepth = 0;
    bool carriesBorrow = false;
    bool borrowSourceOutlivesFrame = false;
    bool borrowAddressStable = false;
};

struct AsyncFunctionPlan final
{
    std::string returnType;
    std::string opaqueFutureIdentity;
    FutureStart start = FutureStart::Lazy;
    FrameStorage storage = FrameStorage::CallerStack;
    SynchronousGeneratorPlan stateMachine;
    bool completesSynchronously = false;
    bool hasImplicitHeapAllocation = false;
};

// Builds the opaque future and native frame contract from liveness computed by the frontend.
// GeneratorLocal is intentionally shared with synchronous generators so await/yield cannot drift
// into two incompatible frame ABIs.
std::optional<AsyncFunctionPlan> buildAsyncFunctionPlan(
    std::string_view returnType,
    std::span<const GeneratorLocal> locals,
    std::span<const AwaitSuspension> awaits,
    FrameStorage storage,
    bool hasSelfReference,
    std::string& error) noexcept;

enum class PollState { Created, Pending, Ready, Cancelled, Consumed };
enum class FutureCompletion { None, Value, Error };
enum class PollOutcome { Pending, ReadyValue, ReadyError };

struct FutureProtocol final
{
    std::string valueType;
    std::string errorType;
    std::string cancellationType;
    bool hasPoll = true;
    bool hasResume = true;
    bool singleConsumer = true;
    bool staticDispatch = true;
};

bool validateFutureProtocol(const FutureProtocol& protocol, std::string& error) noexcept;

struct AwaitContext final
{
    bool insideAsyncFunction = false;
    bool insideComptime = false;
    bool insideNoSuspendCleanup = false;
};

bool awaitAllowed(const AwaitContext& context, std::string& error) noexcept;

class FutureState final
{
public:
    PollState state() const noexcept { return state_; }
    FutureCompletion completion() const noexcept { return completion_; }
    bool poll(PollOutcome outcome, std::string& error);
    bool poll(bool ready, std::string& error);
    bool consume(std::string& error);
    bool cancel(std::string& error);
private:
    PollState state_ = PollState::Created;
    FutureCompletion completion_ = FutureCompletion::None;
};

std::vector<std::string> cancellationCleanup(
    std::span<const std::string> activeLocals,
    std::span<const std::string> activeDefers);

enum class CancellationReason { Requested, FutureDropped, ParentScopeExited };
enum class AsyncCleanupKind { Defer, LocalDrop };

struct ActiveAsyncDefer final
{
    std::string name;
    std::size_t depth = 0;
};

struct AsyncCleanupAction final
{
    AsyncCleanupKind kind = AsyncCleanupKind::LocalDrop;
    std::string name;
    std::optional<std::size_t> frameSlot;
};

struct AsyncCancellationPlan final
{
    CancellationReason reason = CancellationReason::Requested;
    std::size_t suspendedState = 0;
    std::vector<AsyncCleanupAction> actions;
    bool cooperative = true;
    bool usesThreadAbort = false;
};

std::optional<AsyncCancellationPlan> buildAsyncCancellationPlan(
    const AsyncFunctionPlan& function,
    std::size_t suspendedState,
    std::span<const std::string> frameSlotNames,
    std::span<const ActiveAsyncDefer> activeDefers,
    CancellationReason reason,
    std::string& error) noexcept;

enum class AsyncGeneratorState { Pending, ItemReady, Completed, Error, Cancelled };
bool asyncGeneratorMayProduce(
    AsyncGeneratorState state,
    bool consumerRequestedNext) noexcept;

enum class AsyncGeneratorSuspensionKind { Await, Yield };

struct AsyncGeneratorSuspension final
{
    std::size_t state = 0;
    AsyncGeneratorSuspensionKind kind = AsyncGeneratorSuspensionKind::Await;
    std::size_t cleanupDepth = 0;
    bool carriesBorrow = false;
    bool borrowSourceOutlivesFrame = false;
    bool borrowAddressStable = false;
};

struct AsyncGeneratorPlan final
{
    std::string itemType;
    std::string errorType;
    std::string cancellationType;
    FrameStorage storage = FrameStorage::CallerStack;
    SynchronousGeneratorPlan stateMachine;
    bool explicitBackpressure = true;
    bool moveOnlyItems = false;
};

std::optional<AsyncGeneratorPlan> buildAsyncGeneratorPlan(
    std::string_view itemType,
    std::string_view errorType,
    std::string_view cancellationType,
    std::span<const GeneratorLocal> locals,
    std::span<const AsyncGeneratorSuspension> suspensions,
    FrameStorage storage,
    bool hasSelfReference,
    bool moveOnlyItems,
    std::string& error) noexcept;

enum class AsyncGeneratorPollOutcome { Pending, ItemReady, ReadyError, Completed };

class AsyncGeneratorProtocolState final
{
public:
    AsyncGeneratorState state() const noexcept { return state_; }
    bool demandOutstanding() const noexcept { return demandOutstanding_; }
    bool itemOutstanding() const noexcept { return itemOutstanding_; }
    bool requestNext(std::string& error);
    bool resume(AsyncGeneratorPollOutcome outcome, std::string& error);
    bool takeItem(std::string& error);
    bool stopEarly(std::string& error);
private:
    AsyncGeneratorState state_ = AsyncGeneratorState::Pending;
    bool demandOutstanding_ = false;
    bool itemOutstanding_ = false;
};

struct ThreadSafetyFacts final
{
    bool fieldsSend = true;
    bool fieldsSync = true;
    bool containsRawPointer = false;
    bool negativeSendImpl = false;
    bool negativeSyncImpl = false;
};
bool structurallySend(const ThreadSafetyFacts& facts) noexcept;
bool structurallySync(const ThreadSafetyFacts& facts) noexcept;

enum class ThreadSafetyMemberKind
{
    Value,
    Atomic,
    SharedReference,
    MutableReference,
    RawPointer,
    DynamicTrait
};

struct ThreadSafetyMemberFacts final
{
    std::string name;
    ThreadSafetyMemberKind kind = ThreadSafetyMemberKind::Value;
    bool send = true;
    bool sync = true;
    bool dynamicTraitSend = false;
    bool dynamicTraitSync = false;
};

struct ThreadSafetyTypeFacts final
{
    std::string typeName;
    std::vector<ThreadSafetyMemberFacts> members;
    bool negativeSendImpl = false;
    bool negativeSyncImpl = false;
    bool unsafeSendImpl = false;
    bool unsafeSyncImpl = false;
    bool unsafeProofReviewed = false;
    bool asyncClosure = false;
};

struct ThreadSafetyDecision final
{
    bool send = false;
    bool sync = false;
    bool usedUnsafeSend = false;
    bool usedUnsafeSync = false;
    std::string reason;
};

ThreadSafetyDecision evaluateThreadSafety(
    const ThreadSafetyTypeFacts& facts) noexcept;

struct ThreadSafetyMetadata final
{
    std::uint16_t contractMajor = kConcurrencyContractMajor;
    std::uint16_t contractMinor = kConcurrencyContractMinor;
    std::string typeName;
    bool send = false;
    bool sync = false;
    bool unsafeSend = false;
    bool unsafeSync = false;
};

ThreadSafetyMetadata exportThreadSafetyMetadata(
    std::string_view typeName,
    const ThreadSafetyDecision& decision);
std::optional<ThreadSafetyDecision> importThreadSafetyMetadata(
    const ThreadSafetyMetadata& metadata,
    std::string& error) noexcept;

bool taskBorrowMayEscapeScope(bool childJoined, bool borrowOutlivesScope) noexcept;

struct TaskExecutorProtocol final
{
    std::string typeName;
    bool hasSpawn = true;
    bool hasPollJoin = true;
    bool hasCancel = true;
    bool staticDispatch = true;
    bool supportsNoHeap = false;
};

struct ScopedTask final
{
    std::string name;
    bool borrowsFromScope = false;
    bool borrowOutlivesScope = false;
    bool joinedBeforeScopeExit = true;
};

struct TaskScopePlan final
{
    std::string executorType;
    std::vector<ScopedTask> children;
    std::size_t nestingDepth = 0;
    bool noHeap = false;
    bool fixesThreadPool = false;
};

std::optional<TaskScopePlan> buildTaskScopePlan(
    const TaskExecutorProtocol& executor,
    std::span<const ScopedTask> children,
    std::size_t nestingDepth,
    bool noHeap,
    std::string& error) noexcept;

enum class TaskScopeExitReason { Normal, EarlyError, Cancellation, Panic };
enum class TaskScopeActionKind { Cancel, Join, PropagateError, PropagateCancellation, PropagatePanic };

struct TaskScopeAction final
{
    TaskScopeActionKind kind = TaskScopeActionKind::Join;
    std::string child;
};

std::vector<TaskScopeAction> planTaskScopeExit(
    const TaskScopePlan& scope,
    std::span<const std::string> completedChildren,
    TaskScopeExitReason reason) noexcept;

std::string concurrencyContractManifest();
std::string concurrencyContractFingerprintHex();
bool writeConcurrencyContract(const std::filesystem::path& destination, std::string& error);
bool verifyConcurrencyContractFile(const std::filesystem::path& source, std::string& error);
}
