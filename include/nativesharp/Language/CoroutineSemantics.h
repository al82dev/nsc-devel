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

std::string concurrencyContractManifest();
std::string concurrencyContractFingerprintHex();
bool writeConcurrencyContract(const std::filesystem::path& destination, std::string& error);
bool verifyConcurrencyContractFile(const std::filesystem::path& source, std::string& error);
}
