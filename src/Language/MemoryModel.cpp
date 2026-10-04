#include "nativesharp/Language/MemoryModel.h"

namespace nativesharp::language
{
std::optional<MemoryOrder> decodeMemoryOrder(std::uint64_t value) noexcept
{
    if (value > static_cast<std::uint64_t>(MemoryOrder::SequentiallyConsistent))
        return std::nullopt;
    return static_cast<MemoryOrder>(value);
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
            return true;
        case AtomicOperation::Fence:
            return order != MemoryOrder::Relaxed;
    }
    return false;
}

bool validCompareExchangeOrders(MemoryOrder success, MemoryOrder failure) noexcept
{
    if (!validMemoryOrder(AtomicOperation::ReadModifyWrite, success) ||
        !validMemoryOrder(AtomicOperation::Load, failure))
        return false;
    switch (success)
    {
        case MemoryOrder::Relaxed:
            return failure == MemoryOrder::Relaxed;
        case MemoryOrder::Acquire:
            return failure == MemoryOrder::Relaxed || failure == MemoryOrder::Acquire;
        case MemoryOrder::Release:
            return failure == MemoryOrder::Relaxed;
        case MemoryOrder::AcquireRelease:
            return failure == MemoryOrder::Relaxed || failure == MemoryOrder::Acquire;
        case MemoryOrder::SequentiallyConsistent:
            return true;
    }
    return false;
}

DataRaceBehavior dataRaceBehavior() noexcept
{
    return DataRaceBehavior::UndefinedBehavior;
}

bool atomicAccessIsLockFreeWindowsX64(
    std::uint32_t bitWidth,
    bool naturallyAligned) noexcept
{
    return naturallyAligned &&
        (bitWidth == 8U || bitWidth == 16U || bitWidth == 32U || bitWidth == 64U);
}

bool volatileProvidesSynchronization() noexcept
{
    return false;
}

bool volatileAccessMayTear(
    std::uint32_t bitWidth,
    bool naturallyAligned) noexcept
{
    return !naturallyAligned || bitWidth > 64U;
}

std::string memoryModelManifest()
{
    return
        "NSMEMORY 3.0\n"
        "target=x86_64-pc-windows-msvc\n"
        "data-race=undefined-behavior\n"
        "atomic-load=relaxed|acquire|seq-cst\n"
        "atomic-store=relaxed|release|seq-cst\n"
        "atomic-rmw=relaxed|acquire|release|acq-rel|seq-cst\n"
        "atomic-fence=acquire|release|acq-rel|seq-cst\n"
        "compare-exchange=failure-not-stronger-than-success\n"
        "volatile=observable-access-only\n"
        "volatile-synchronization=no\n"
        "volatile-tearing=possible-if-misaligned-or-wider-than-64\n";
}
}
