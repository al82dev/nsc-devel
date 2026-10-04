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
