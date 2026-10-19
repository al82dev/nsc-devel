#pragma once

#include "nativesharp/Language/AtomicOperation.h"
#include "nativesharp/Language/DataRaceBehavior.h"
#include "nativesharp/Language/MemoryOrder.h"

#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::language
{
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
