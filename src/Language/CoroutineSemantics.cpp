#include "nativesharp/Language/CoroutineSemantics.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace nativesharp::language
{
FutureStart nativeFutureStart() noexcept { return FutureStart::Lazy; }
bool asyncFrameHasImplicitHeap(FrameStorage) noexcept { return false; }
bool borrowMayCrossAwait(bool sourceOutlivesFrame, bool addressStable) noexcept
{
    return sourceOutlivesFrame && addressStable;
}

namespace
{
GeneratorStorage generatorStorage(FrameStorage storage) noexcept
{
    switch (storage)
    {
        case FrameStorage::CallerStack: return GeneratorStorage::Stack;
        case FrameStorage::CallerArena: return GeneratorStorage::Arena;
        case FrameStorage::ExplicitHeap: return GeneratorStorage::ExplicitHeap;
    }
    return GeneratorStorage::Stack;
}

std::string asyncFutureIdentity(
    std::string_view returnType,
    FrameStorage storage,
    const SynchronousGeneratorPlan& stateMachine)
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t value = offsetBasis;
    const auto append = [&](std::string_view text)
    {
        for (const char byte : text)
        {
            value ^= static_cast<unsigned char>(byte);
            value *= prime;
        }
    };
    append(returnType);
    append("|");
    append(std::to_string(static_cast<unsigned>(storage)));
    append("|");
    append(std::to_string(stateMachine.frame.size));
    append("|");
    append(std::to_string(stateMachine.frame.alignment));
    append("|");
    append(std::to_string(stateMachine.states.size()));
    std::ostringstream stream;
    stream << "future<" << returnType << ">#" << std::hex << std::setfill('0')
           << std::setw(16) << value;
    return stream.str();
}
}

std::optional<AsyncFunctionPlan> buildAsyncFunctionPlan(
    std::string_view returnType,
    std::span<const GeneratorLocal> locals,
    std::span<const AwaitSuspension> awaits,
    FrameStorage storage,
    bool hasSelfReference,
    std::string& error) noexcept
{
    error.clear();
    if (returnType.empty())
    {
        error = "an async function requires a resolved return type";
        return std::nullopt;
    }

    AsyncFunctionPlan result;
    result.returnType = std::string(returnType);
    result.storage = storage;
    result.start = nativeFutureStart();
    result.hasImplicitHeapAllocation = asyncFrameHasImplicitHeap(storage);

    if (awaits.empty())
    {
        for (const GeneratorLocal& local : locals)
        {
            if (local.livesAcrossYield)
            {
                error = "an async function without await cannot have locals live across suspension";
                return std::nullopt;
            }
        }
        result.completesSynchronously = true;
        result.stateMachine.storage = generatorStorage(storage);
        result.stateMachine.itemType = result.returnType;
        result.stateMachine.frame = GeneratorFrame{1, 1, hasSelfReference};
        result.stateMachine.completedState = 1;
        result.opaqueFutureIdentity = asyncFutureIdentity(
            returnType, storage, result.stateMachine);
        return result;
    }

    std::vector<GeneratorSuspension> suspensions;
    suspensions.reserve(awaits.size());
    for (std::size_t index = 0; index < awaits.size(); ++index)
    {
        const AwaitSuspension& await = awaits[index];
        if (await.state != index + 1U)
        {
            error = "async await states must be contiguous and start at one";
            return std::nullopt;
        }
        if (await.carriesBorrow &&
            !borrowMayCrossAwait(await.borrowSourceOutlivesFrame, await.borrowAddressStable))
        {
            error = "borrow across await requires source lifetime and stable address proofs";
            return std::nullopt;
        }
        suspensions.push_back(GeneratorSuspension{
            await.state,
            result.returnType,
            GeneratorYieldKind::Value,
            await.cleanupDepth,
            false});
    }

    const auto stateMachine = buildSynchronousGeneratorPlan(
        locals, suspensions, generatorStorage(storage), hasSelfReference, error);
    if (!stateMachine.has_value()) return std::nullopt;
    result.stateMachine = *stateMachine;
    result.opaqueFutureIdentity = asyncFutureIdentity(
        returnType, storage, result.stateMachine);
    return result;
}

bool validateFutureProtocol(const FutureProtocol& protocol, std::string& error) noexcept
{
    error.clear();
    if (protocol.valueType.empty())
    {
        error = "Future value type is unresolved";
        return false;
    }
    if (protocol.errorType.empty() || protocol.cancellationType.empty())
    {
        error = "Future error and cancellation types must be explicit";
        return false;
    }
    if (!protocol.hasPoll || !protocol.hasResume || !protocol.singleConsumer ||
        !protocol.staticDispatch)
    {
        error = "Future requires static Poll/Resume and a single-consumer contract";
        return false;
    }
    return true;
}

bool awaitAllowed(const AwaitContext& context, std::string& error) noexcept
{
    error.clear();
    if (!context.insideAsyncFunction)
    {
        error = "await is only valid inside an async function";
        return false;
    }
    if (context.insideComptime)
    {
        error = "await is not valid during deterministic comptime execution";
        return false;
    }
    if (context.insideNoSuspendCleanup)
    {
        error = "await cannot suspend inside a no-suspend cleanup region";
        return false;
    }
    return true;
}

bool FutureState::poll(PollOutcome outcome, std::string& error)
{
    error.clear();
    if (state_ == PollState::Ready || state_ == PollState::Cancelled || state_ == PollState::Consumed)
    { error = "future cannot be resumed after completion, cancellation, or consumption"; return false; }
    switch (outcome)
    {
        case PollOutcome::Pending:
            state_ = PollState::Pending;
            completion_ = FutureCompletion::None;
            break;
        case PollOutcome::ReadyValue:
            state_ = PollState::Ready;
            completion_ = FutureCompletion::Value;
            break;
        case PollOutcome::ReadyError:
            state_ = PollState::Ready;
            completion_ = FutureCompletion::Error;
            break;
    }
    return true;
}

bool FutureState::poll(bool ready, std::string& error)
{
    return poll(ready ? PollOutcome::ReadyValue : PollOutcome::Pending, error);
}

bool FutureState::consume(std::string& error)
{
    error.clear();
    if (state_ != PollState::Ready) { error = "future result is not ready"; return false; }
    state_ = PollState::Consumed;
    return true;
}

bool FutureState::cancel(std::string& error)
{
    error.clear();
    if (state_ == PollState::Ready || state_ == PollState::Consumed || state_ == PollState::Cancelled)
    { error = "completed future cannot be cancelled"; return false; }
    state_ = PollState::Cancelled;
    completion_ = FutureCompletion::None;
    return true;
}

std::vector<std::string> cancellationCleanup(
    std::span<const std::string> activeLocals,
    std::span<const std::string> activeDefers)
{
    std::vector<std::string> result;
    result.reserve(activeLocals.size() + activeDefers.size());
    result.insert(result.end(), activeDefers.rbegin(), activeDefers.rend());
    result.insert(result.end(), activeLocals.rbegin(), activeLocals.rend());
    return result;
}

std::optional<AsyncCancellationPlan> buildAsyncCancellationPlan(
    const AsyncFunctionPlan& function,
    std::size_t suspendedState,
    std::span<const std::string> frameSlotNames,
    std::span<const ActiveAsyncDefer> activeDefers,
    CancellationReason reason,
    std::string& error) noexcept
{
    error.clear();
    const auto state = std::ranges::find_if(
        function.stateMachine.states, [&](const GeneratorState& candidate)
    {
        return candidate.state == suspendedState;
    });
    if (state == function.stateMachine.states.end())
    {
        error = "cancellation requires a currently suspended async state";
        return std::nullopt;
    }
    if (frameSlotNames.size() != function.stateMachine.slots.size())
    {
        error = "async cancellation requires one diagnostic name per frame slot";
        return std::nullopt;
    }

    AsyncCancellationPlan result;
    result.reason = reason;
    result.suspendedState = suspendedState;

    std::vector<ActiveAsyncDefer> defers;
    defers.reserve(activeDefers.size());
    for (const ActiveAsyncDefer& defer : activeDefers)
    {
        if (defer.name.empty() || defer.depth == 0 || defer.depth > state->cleanupDepth)
        {
            error = "active async defer must have a name and fit the suspended cleanup depth";
            return std::nullopt;
        }
        defers.push_back(defer);
    }
    std::ranges::stable_sort(defers, std::greater{}, &ActiveAsyncDefer::depth);
    for (const ActiveAsyncDefer& defer : defers)
        result.actions.push_back(AsyncCleanupAction{AsyncCleanupKind::Defer, defer.name, std::nullopt});

    for (const std::size_t slot :
         generatorCancellationDropOrder(function.stateMachine, suspendedState))
    {
        if (slot >= frameSlotNames.size())
        {
            error = "async cancellation references an invalid frame slot";
            return std::nullopt;
        }
        result.actions.push_back(AsyncCleanupAction{
            AsyncCleanupKind::LocalDrop, frameSlotNames[slot], slot});
    }
    return result;
}

bool asyncGeneratorMayProduce(
    AsyncGeneratorState state,
    bool consumerRequestedNext) noexcept
{
    return consumerRequestedNext && (state == AsyncGeneratorState::Pending ||
        state == AsyncGeneratorState::ItemReady);
}

std::optional<AsyncGeneratorPlan> buildAsyncGeneratorPlan(
    std::string_view itemType,
    std::string_view errorType,
    std::string_view cancellationType,
    std::span<const GeneratorLocal> locals,
    std::span<const AsyncGeneratorSuspension> suspensions,
    FrameStorage storage,
    bool hasSelfReference,
    bool moveOnlyItems,
    std::string& error) noexcept
{
    error.clear();
    if (itemType.empty() || errorType.empty() || cancellationType.empty())
    {
        error = "async generator item, error, and cancellation types must be explicit";
        return std::nullopt;
    }
    if (suspensions.empty())
    {
        error = "async generator requires at least one await or yield suspension";
        return std::nullopt;
    }

    bool hasYield = false;
    std::vector<GeneratorSuspension> unified;
    unified.reserve(suspensions.size());
    for (std::size_t index = 0; index < suspensions.size(); ++index)
    {
        const AsyncGeneratorSuspension& suspension = suspensions[index];
        if (suspension.state != index + 1U)
        {
            error = "async generator states must be contiguous and start at one";
            return std::nullopt;
        }
        if (suspension.carriesBorrow &&
            !borrowMayCrossAwait(
                suspension.borrowSourceOutlivesFrame,
                suspension.borrowAddressStable))
        {
            error = "borrowed async generator state requires source lifetime and stable address proofs";
            return std::nullopt;
        }
        const bool yields = suspension.kind == AsyncGeneratorSuspensionKind::Yield;
        hasYield = hasYield || yields;
        unified.push_back(GeneratorSuspension{
            suspension.state,
            std::string(itemType),
            yields && suspension.carriesBorrow
                ? GeneratorYieldKind::SharedReference
                : GeneratorYieldKind::Value,
            suspension.cleanupDepth,
            suspension.carriesBorrow});
    }
    if (!hasYield)
    {
        error = "async generator requires at least one yield state";
        return std::nullopt;
    }

    const auto machine = buildSynchronousGeneratorPlan(
        locals, unified, generatorStorage(storage), hasSelfReference, error);
    if (!machine.has_value()) return std::nullopt;

    AsyncGeneratorPlan result;
    result.itemType = std::string(itemType);
    result.errorType = std::string(errorType);
    result.cancellationType = std::string(cancellationType);
    result.storage = storage;
    result.stateMachine = *machine;
    result.moveOnlyItems = moveOnlyItems;
    return result;
}

bool AsyncGeneratorProtocolState::requestNext(std::string& error)
{
    error.clear();
    if (state_ == AsyncGeneratorState::Completed ||
        state_ == AsyncGeneratorState::Error ||
        state_ == AsyncGeneratorState::Cancelled)
    {
        error = "async generator cannot accept demand after a terminal state";
        return false;
    }
    if (demandOutstanding_ || itemOutstanding_)
    {
        error = "async generator allows one outstanding demand and one borrowed item";
        return false;
    }
    demandOutstanding_ = true;
    state_ = AsyncGeneratorState::Pending;
    return true;
}

bool AsyncGeneratorProtocolState::resume(
    AsyncGeneratorPollOutcome outcome,
    std::string& error)
{
    error.clear();
    if (state_ == AsyncGeneratorState::Completed ||
        state_ == AsyncGeneratorState::Error ||
        state_ == AsyncGeneratorState::Cancelled)
    {
        error = "async generator cannot resume after a terminal state";
        return false;
    }
    if (!demandOutstanding_)
    {
        error = "async generator resume requires explicit consumer demand";
        return false;
    }

    switch (outcome)
    {
        case AsyncGeneratorPollOutcome::Pending:
            state_ = AsyncGeneratorState::Pending;
            return true;
        case AsyncGeneratorPollOutcome::ItemReady:
            state_ = AsyncGeneratorState::ItemReady;
            demandOutstanding_ = false;
            itemOutstanding_ = true;
            return true;
        case AsyncGeneratorPollOutcome::ReadyError:
            state_ = AsyncGeneratorState::Error;
            break;
        case AsyncGeneratorPollOutcome::Completed:
            state_ = AsyncGeneratorState::Completed;
            break;
    }
    demandOutstanding_ = false;
    itemOutstanding_ = false;
    return true;
}

bool AsyncGeneratorProtocolState::takeItem(std::string& error)
{
    error.clear();
    if (!itemOutstanding_ || state_ != AsyncGeneratorState::ItemReady)
    {
        error = "async generator has no yielded item to consume";
        return false;
    }
    itemOutstanding_ = false;
    state_ = AsyncGeneratorState::Pending;
    return true;
}

bool AsyncGeneratorProtocolState::stopEarly(std::string& error)
{
    error.clear();
    if (state_ == AsyncGeneratorState::Completed ||
        state_ == AsyncGeneratorState::Error ||
        state_ == AsyncGeneratorState::Cancelled)
    {
        error = "terminal async generator cannot be cancelled again";
        return false;
    }
    state_ = AsyncGeneratorState::Cancelled;
    demandOutstanding_ = false;
    itemOutstanding_ = false;
    return true;
}

bool structurallySend(const ThreadSafetyFacts& facts) noexcept
{
    return facts.fieldsSend && !facts.containsRawPointer && !facts.negativeSendImpl;
}

bool structurallySync(const ThreadSafetyFacts& facts) noexcept
{
    return facts.fieldsSync && !facts.containsRawPointer && !facts.negativeSyncImpl;
}

ThreadSafetyDecision evaluateThreadSafety(
    const ThreadSafetyTypeFacts& facts) noexcept
{
    ThreadSafetyDecision result;
    if (facts.typeName.empty())
    {
        result.reason = "thread-safety derivation requires a resolved type identity";
        return result;
    }

    bool derivedSend = true;
    bool derivedSync = true;
    for (const ThreadSafetyMemberFacts& member : facts.members)
    {
        switch (member.kind)
        {
            case ThreadSafetyMemberKind::Atomic:
                break;
            case ThreadSafetyMemberKind::SharedReference:
                derivedSend = derivedSend && member.sync;
                derivedSync = derivedSync && member.sync;
                break;
            case ThreadSafetyMemberKind::MutableReference:
                derivedSend = derivedSend && member.send;
                derivedSync = derivedSync && member.sync;
                break;
            case ThreadSafetyMemberKind::RawPointer:
                derivedSend = false;
                derivedSync = false;
                break;
            case ThreadSafetyMemberKind::DynamicTrait:
                derivedSend = derivedSend && member.dynamicTraitSend;
                derivedSync = derivedSync && member.dynamicTraitSync;
                break;
            case ThreadSafetyMemberKind::Value:
                derivedSend = derivedSend && member.send;
                derivedSync = derivedSync && member.sync;
                break;
        }
    }

    if (facts.negativeSendImpl) derivedSend = false;
    if (facts.negativeSyncImpl) derivedSync = false;
    if (facts.unsafeProofReviewed && !facts.negativeSendImpl && facts.unsafeSendImpl)
    {
        derivedSend = true;
        result.usedUnsafeSend = true;
    }
    if (facts.unsafeProofReviewed && !facts.negativeSyncImpl && facts.unsafeSyncImpl)
    {
        derivedSync = true;
        result.usedUnsafeSync = true;
    }

    result.send = derivedSend;
    result.sync = derivedSync;
    if ((facts.unsafeSendImpl || facts.unsafeSyncImpl) && !facts.unsafeProofReviewed)
        result.reason = "unsafe Send/Sync implementation requires an audited safety proof";
    else if (facts.negativeSendImpl || facts.negativeSyncImpl)
        result.reason = "negative Send/Sync implementation opts the type out structurally";
    else if (!result.send || !result.sync)
        result.reason = facts.asyncClosure
            ? "async closure captures do not satisfy structural Send/Sync"
            : "one or more members do not satisfy structural Send/Sync";
    return result;
}

ThreadSafetyMetadata exportThreadSafetyMetadata(
    std::string_view typeName,
    const ThreadSafetyDecision& decision)
{
    ThreadSafetyMetadata metadata;
    metadata.typeName = std::string(typeName);
    metadata.send = decision.send;
    metadata.sync = decision.sync;
    metadata.unsafeSend = decision.usedUnsafeSend;
    metadata.unsafeSync = decision.usedUnsafeSync;
    return metadata;
}

std::optional<ThreadSafetyDecision> importThreadSafetyMetadata(
    const ThreadSafetyMetadata& metadata,
    std::string& error) noexcept
{
    error.clear();
    if (metadata.contractMajor != kConcurrencyContractMajor)
    {
        error = "incompatible Send/Sync metadata contract major version";
        return std::nullopt;
    }
    if (metadata.typeName.empty())
    {
        error = "Send/Sync metadata has no type identity";
        return std::nullopt;
    }
    ThreadSafetyDecision result;
    result.send = metadata.send;
    result.sync = metadata.sync;
    result.usedUnsafeSend = metadata.unsafeSend;
    result.usedUnsafeSync = metadata.unsafeSync;
    return result;
}

bool taskBorrowMayEscapeScope(bool childJoined, bool borrowOutlivesScope) noexcept
{
    return childJoined && borrowOutlivesScope;
}

std::optional<TaskScopePlan> buildTaskScopePlan(
    const TaskExecutorProtocol& executor,
    std::span<const ScopedTask> children,
    std::size_t nestingDepth,
    bool noHeap,
    std::string& error) noexcept
{
    error.clear();
    if (executor.typeName.empty() || !executor.hasSpawn || !executor.hasPollJoin ||
        !executor.hasCancel || !executor.staticDispatch)
    {
        error = "task scope requires a named static executor with spawn, poll/join, and cancel";
        return std::nullopt;
    }
    if (noHeap && !executor.supportsNoHeap)
    {
        error = "noheap task scope requires an executor with explicit noheap support";
        return std::nullopt;
    }

    TaskScopePlan result;
    result.executorType = executor.typeName;
    result.nestingDepth = nestingDepth;
    result.noHeap = noHeap;
    result.children.assign(children.begin(), children.end());
    for (const ScopedTask& child : result.children)
    {
        if (child.name.empty())
        {
            error = "every scoped task requires a stable identity";
            return std::nullopt;
        }
        if (child.borrowsFromScope &&
            !taskBorrowMayEscapeScope(child.joinedBeforeScopeExit, child.borrowOutlivesScope))
        {
            error = "borrowed child task must be joined before scope exit and fit the scope lifetime";
            return std::nullopt;
        }
    }
    return result;
}

std::vector<TaskScopeAction> planTaskScopeExit(
    const TaskScopePlan& scope,
    std::span<const std::string> completedChildren,
    TaskScopeExitReason reason) noexcept
{
    const auto completed = [&](std::string_view child)
    {
        return std::ranges::find(completedChildren, child) != completedChildren.end();
    };

    std::vector<TaskScopeAction> result;
    const bool abnormal = reason != TaskScopeExitReason::Normal;
    if (abnormal)
    {
        for (auto child = scope.children.rbegin(); child != scope.children.rend(); ++child)
        {
            if (!completed(child->name))
                result.push_back(TaskScopeAction{TaskScopeActionKind::Cancel, child->name});
        }
    }
    for (auto child = scope.children.rbegin(); child != scope.children.rend(); ++child)
    {
        if (!completed(child->name))
            result.push_back(TaskScopeAction{TaskScopeActionKind::Join, child->name});
    }
    switch (reason)
    {
        case TaskScopeExitReason::Normal: break;
        case TaskScopeExitReason::EarlyError:
            result.push_back(TaskScopeAction{TaskScopeActionKind::PropagateError, {}});
            break;
        case TaskScopeExitReason::Cancellation:
            result.push_back(TaskScopeAction{TaskScopeActionKind::PropagateCancellation, {}});
            break;
        case TaskScopeExitReason::Panic:
            result.push_back(TaskScopeAction{TaskScopeActionKind::PropagatePanic, {}});
            break;
    }
    return result;
}

std::string concurrencyContractManifest()
{
    std::string result = "NSCONCURRENCY " + std::to_string(kConcurrencyContractMajor) + "." +
        std::to_string(kConcurrencyContractMinor) + "\n";
    result += "target=x86_64-pc-windows-msvc\n";
    result += "scheduler=explicit-job-system-only\n";
    result += "parallel-policy=caller-supplied-job-system+threshold+cancel\n";
    result += "future-start=lazy\n";
    result += "frame-storage=caller-stack|caller-arena|explicit-heap\n";
    result += "implicit-heap=no\n";
    result += "poll=single-consumer-no-resume-after-terminal\n";
    result += "cancellation=cooperative-no-thread-abort\n";
    result += "cancellation-cleanup=defers-reverse-then-locals-reverse\n";
    result += "task-scope=borrow-requires-join-before-scope-exit\n";
    result += "send-sync=structural-fail-closed+raw-pointer-no-derive\n";
    result += "atomic-orders=relaxed|acquire|release|acq-rel|seq-cst\n";
    result += "atomic-load=relaxed|acquire|seq-cst\n";
    result += "atomic-store=relaxed|release|seq-cst\n";
    result += "atomic-rmw=all-orders\n";
    result += "volatile-synchronization=no\n";
    result += "async-generator=explicit-backpressure\n";
    return result;
}

std::string concurrencyContractFingerprintHex()
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t value = offsetBasis;
    for (const char byte : concurrencyContractManifest())
    {
        value ^= static_cast<unsigned char>(byte);
        value *= prime;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

bool writeConcurrencyContract(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "concurrency contract destination is empty";
        return false;
    }
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create concurrency contract directory '" + parent.string() +
                "': " + filesystemError.message();
            return false;
        }
    }
    const std::string manifest = concurrencyContractManifest();
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(manifest.data(), static_cast<std::streamsize>(manifest.size())))
    {
        error = "cannot write concurrency contract '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyConcurrencyContractFile(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open concurrency contract '" + source.string() + "'";
        return false;
    }
    std::string actual{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
    if (actual != concurrencyContractManifest())
    {
        error = "incompatible concurrency contract '" + source.string() +
            "': expected fingerprint " + concurrencyContractFingerprintHex();
        return false;
    }
    return true;
}
}
