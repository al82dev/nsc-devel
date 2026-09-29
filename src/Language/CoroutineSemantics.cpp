#include "nativesharp/Language/CoroutineSemantics.h"

namespace nativesharp::language
{
FutureStart nativeFutureStart() noexcept { return FutureStart::Lazy; }
bool asyncFrameHasImplicitHeap(FrameStorage) noexcept { return false; }
bool borrowMayCrossAwait(bool sourceOutlivesFrame, bool addressStable) noexcept
{
    return sourceOutlivesFrame && addressStable;
}

bool FutureState::poll(bool ready, std::string& error)
{
    error.clear();
    if (state_ == PollState::Ready || state_ == PollState::Cancelled || state_ == PollState::Consumed)
    { error = "future cannot be resumed after completion, cancellation, or consumption"; return false; }
    state_ = ready ? PollState::Ready : PollState::Pending;
    return true;
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

bool asyncGeneratorMayProduce(
    AsyncGeneratorState state,
    bool consumerRequestedNext) noexcept
{
    return consumerRequestedNext && (state == AsyncGeneratorState::Pending ||
        state == AsyncGeneratorState::ItemReady);
}

bool structurallySend(const ThreadSafetyFacts& facts) noexcept
{
    return facts.fieldsSend && !facts.containsRawPointer && !facts.negativeSendImpl;
}

bool structurallySync(const ThreadSafetyFacts& facts) noexcept
{
    return facts.fieldsSync && !facts.containsRawPointer && !facts.negativeSyncImpl;
}

bool taskBorrowMayEscapeScope(bool childJoined, bool borrowOutlivesScope) noexcept
{
    return childJoined && borrowOutlivesScope;
}

bool validMemoryOrder(AtomicOperation operation, MemoryOrder order) noexcept
{
    switch (operation)
    {
        case AtomicOperation::Load:
            return order != MemoryOrder::Release && order != MemoryOrder::AcquireRelease;
        case AtomicOperation::Store:
            return order != MemoryOrder::Acquire && order != MemoryOrder::AcquireRelease;
        case AtomicOperation::ReadModifyWrite:
        case AtomicOperation::Fence:
            return true;
    }
    return false;
}

bool volatileProvidesSynchronization() noexcept { return false; }
}
