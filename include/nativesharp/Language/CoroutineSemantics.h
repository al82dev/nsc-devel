#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
inline constexpr std::uint16_t kConcurrencyContractMajor = 1;
inline constexpr std::uint16_t kConcurrencyContractMinor = 0;

enum class FutureStart { Lazy, Eager };
enum class FrameStorage { CallerStack, CallerArena, ExplicitHeap };
FutureStart nativeFutureStart() noexcept;
bool asyncFrameHasImplicitHeap(FrameStorage storage) noexcept;
bool borrowMayCrossAwait(bool sourceOutlivesFrame, bool addressStable) noexcept;

enum class PollState { Created, Pending, Ready, Cancelled, Consumed };
class FutureState final
{
public:
    PollState state() const noexcept { return state_; }
    bool poll(bool ready, std::string& error);
    bool consume(std::string& error);
    bool cancel(std::string& error);
private:
    PollState state_ = PollState::Created;
};

std::vector<std::string> cancellationCleanup(
    std::span<const std::string> activeLocals,
    std::span<const std::string> activeDefers);

enum class AsyncGeneratorState { Pending, ItemReady, Completed, Cancelled };
bool asyncGeneratorMayProduce(
    AsyncGeneratorState state,
    bool consumerRequestedNext) noexcept;

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

bool taskBorrowMayEscapeScope(bool childJoined, bool borrowOutlivesScope) noexcept;

enum class AtomicOperation { Load, Store, ReadModifyWrite, Fence };
enum class MemoryOrder { Relaxed, Acquire, Release, AcquireRelease, SequentiallyConsistent };
bool validMemoryOrder(AtomicOperation operation, MemoryOrder order) noexcept;
bool volatileProvidesSynchronization() noexcept;

std::string concurrencyContractManifest();
std::string concurrencyContractFingerprintHex();
bool writeConcurrencyContract(const std::filesystem::path& destination, std::string& error);
bool verifyConcurrencyContractFile(const std::filesystem::path& source, std::string& error);
}
