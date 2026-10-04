#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::language
{
enum class AtomicOperation : std::uint8_t
{
    Load,
    Store,
    ReadModifyWrite,
    Fence
};

// These values are the public NativeSharp.Core.Threading.MemoryOrder ABI. They are deliberately
// independent from LLVM's numeric ordering constants.
enum class MemoryOrder : std::uint8_t
{
    Relaxed = 0,
    Acquire = 1,
    Release = 2,
    AcquireRelease = 3,
    SequentiallyConsistent = 4
};

enum class DataRaceBehavior : std::uint8_t
{
    UndefinedBehavior
};

std::optional<MemoryOrder> decodeMemoryOrder(std::uint64_t value) noexcept;
bool validMemoryOrder(AtomicOperation operation, MemoryOrder order) noexcept;
bool validCompareExchangeOrders(MemoryOrder success, MemoryOrder failure) noexcept;
DataRaceBehavior dataRaceBehavior() noexcept;
bool atomicAccessIsLockFreeWindowsX64(
    std::uint32_t bitWidth,
    bool naturallyAligned) noexcept;
bool volatileProvidesSynchronization() noexcept;
bool volatileAccessMayTear(
    std::uint32_t bitWidth,
    bool naturallyAligned) noexcept;
std::string memoryModelManifest();
}
