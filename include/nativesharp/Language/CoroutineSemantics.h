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
#include "nativesharp/Language/AsyncFunctionPlan.h"
#include "nativesharp/Language/ActiveAsyncDefer.h"
#include "nativesharp/Language/AsyncCancellationPlan.h"
#include "nativesharp/Language/AsyncCleanupAction.h"
#include "nativesharp/Language/AsyncCleanupKind.h"
#include "nativesharp/Language/AsyncGeneratorPlan.h"
#include "nativesharp/Language/AsyncGeneratorPollOutcome.h"
#include "nativesharp/Language/AsyncGeneratorProtocolState.h"
#include "nativesharp/Language/AsyncGeneratorState.h"
#include "nativesharp/Language/AsyncGeneratorSuspension.h"
#include "nativesharp/Language/AsyncGeneratorSuspensionKind.h"
#include "nativesharp/Language/AwaitContext.h"
#include "nativesharp/Language/AwaitSuspension.h"
#include "nativesharp/Language/FrameStorage.h"
#include "nativesharp/Language/FutureCompletion.h"
#include "nativesharp/Language/FutureProtocol.h"
#include "nativesharp/Language/FutureStart.h"
#include "nativesharp/Language/FutureState.h"
#include "nativesharp/Language/PollOutcome.h"
#include "nativesharp/Language/PollState.h"
#include "nativesharp/Language/CancellationReason.h"
#include "nativesharp/Language/ConcurrencyContractVersion.h"
#include "nativesharp/Language/ScopedTask.h"
#include "nativesharp/Language/TaskExecutorProtocol.h"
#include "nativesharp/Language/TaskScopeAction.h"
#include "nativesharp/Language/TaskScopeActionKind.h"
#include "nativesharp/Language/TaskScopeExitReason.h"
#include "nativesharp/Language/TaskScopePlan.h"
#include "nativesharp/Language/ThreadSafetyDecision.h"
#include "nativesharp/Language/ThreadSafetyFacts.h"
#include "nativesharp/Language/ThreadSafetyMemberFacts.h"
#include "nativesharp/Language/ThreadSafetyMemberKind.h"
#include "nativesharp/Language/ThreadSafetyMetadata.h"
#include "nativesharp/Language/ThreadSafetyTypeFacts.h"

namespace nativesharp::language
{
FutureStart nativeFutureStart() noexcept;
bool asyncFrameHasImplicitHeap(FrameStorage storage) noexcept;
bool borrowMayCrossAwait(bool sourceOutlivesFrame, bool addressStable) noexcept;

// NS304-T05-F03: surface type key of an async function's opaque lazy future.
// futureTypeKey("builtin:int") == "future:builtin:int"; the inverse strips the prefix.
std::string futureTypeKey(std::string_view innerTypeKey);

// NS304-T05-F07: an async function that throws carries its error type in the future key as
// `future:<T>|<E>` (the pipe separates value from error; absent for non-throwing async).
std::string futureTypeKey(std::string_view innerTypeKey, std::string_view errorTypeKey);

std::optional<std::string_view> futureInnerTypeKey(std::string_view typeKey) noexcept;

// NS304-T05-F07: the error type carried by a throwing async future (`future:<T>|<E>`), if any.
std::optional<std::string_view> futureErrorTypeKey(std::string_view typeKey) noexcept;

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

bool validateFutureProtocol(const FutureProtocol& protocol, std::string& error) noexcept;

bool awaitAllowed(const AwaitContext& context, std::string& error) noexcept;

std::vector<std::string> cancellationCleanup(
    std::span<const std::string> activeLocals,
    std::span<const std::string> activeDefers);

std::optional<AsyncCancellationPlan> buildAsyncCancellationPlan(
    const AsyncFunctionPlan& function,
    std::size_t suspendedState,
    std::span<const std::string> frameSlotNames,
    std::span<const ActiveAsyncDefer> activeDefers,
    CancellationReason reason,
    std::string& error) noexcept;

bool asyncGeneratorMayProduce(
    AsyncGeneratorState state,
    bool consumerRequestedNext) noexcept;

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

bool structurallySend(const ThreadSafetyFacts& facts) noexcept;
bool structurallySync(const ThreadSafetyFacts& facts) noexcept;

ThreadSafetyDecision evaluateThreadSafety(
    const ThreadSafetyTypeFacts& facts) noexcept;

ThreadSafetyMetadata exportThreadSafetyMetadata(
    std::string_view typeName,
    const ThreadSafetyDecision& decision);
std::optional<ThreadSafetyDecision> importThreadSafetyMetadata(
    const ThreadSafetyMetadata& metadata,
    std::string& error) noexcept;

bool taskBorrowMayEscapeScope(bool childJoined, bool borrowOutlivesScope) noexcept;

std::optional<TaskScopePlan> buildTaskScopePlan(
    const TaskExecutorProtocol& executor,
    std::span<const ScopedTask> children,
    std::size_t nestingDepth,
    bool noHeap,
    std::string& error) noexcept;

std::vector<TaskScopeAction> planTaskScopeExit(
    const TaskScopePlan& scope,
    std::span<const std::string> completedChildren,
    TaskScopeExitReason reason) noexcept;

std::string concurrencyContractManifest();
std::string concurrencyContractFingerprintHex();
bool writeConcurrencyContract(const std::filesystem::path& destination, std::string& error);
bool verifyConcurrencyContractFile(const std::filesystem::path& source, std::string& error);
}
