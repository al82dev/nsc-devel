#include "nativesharp/Runtime/RuntimeCapabilityReport.h"

#include <initializer_list>
#include <utility>

#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include <algorithm>

namespace nativesharp::runtime
{
namespace
{
bool hasSymbol(std::string_view name)
{
    return std::ranges::any_of(kRuntimeAbiSymbols, [name](const RuntimeAbiSymbol& symbol)
    {
        return symbol.name == name;
    });
}

bool hasAll(std::initializer_list<std::string_view> symbols)
{
    return std::ranges::all_of(symbols, [](std::string_view symbol) { return hasSymbol(symbol); });
}

RuntimeCapabilityStatus statusFor(const RuntimeCapability& capability)
{
    if (capability.missing.empty()) return RuntimeCapabilityStatus::Ready;
    if (capability.evidence.empty()) return RuntimeCapabilityStatus::Missing;
    return RuntimeCapabilityStatus::Partial;
}
}

const char* runtimeCapabilityStatusName(RuntimeCapabilityStatus status) noexcept
{
    switch (status)
    {
        case RuntimeCapabilityStatus::Ready: return "ready";
        case RuntimeCapabilityStatus::Partial: return "partial";
        case RuntimeCapabilityStatus::Missing: return "missing";
    }
    return "unknown";
}

std::vector<RuntimeCapability> runtimeCapabilities()
{
    RuntimeCapability sharedMemory{
        "E032", "shared-memory-regions", RuntimeCapabilityStatus::Missing, {}, {}};
    if (hasAll({"__nativesharp_shared_memory_create", "__nativesharp_shared_memory_open",
                "__nativesharp_shared_memory_map", "__nativesharp_shared_memory_unmap"}))
        sharedMemory.evidence.push_back("shared-memory-runtime-hooks");
    else
        sharedMemory.missing = {"region create/open/map/unmap hooks", "offset pointer contract",
            "regional allocator", "cross-process validation"};
    sharedMemory.status = statusFor(sharedMemory);

    RuntimeCapability synchronization{
        "E033", "shared-synchronization", RuntimeCapabilityStatus::Missing, {}, {}};
    if (hasAll({"__nativesharp_mutex_create_named", "__nativesharp_semaphore_create_named",
                "__nativesharp_event_create_named", "__nativesharp_wait_for_handle_timeout"}))
        synchronization.evidence.push_back("named mutex/semaphore/event and timed wait");
    if (hasAll({"__nativesharp_monitor_enter", "__nativesharp_monitor_wait",
                "__nativesharp_monitor_pulse", "__nativesharp_monitor_pulse_all"}))
        synchronization.evidence.push_back("in-process monitor/condition-variable hooks");
    synchronization.missing = {"SharedMutex contract and hooks", "atomic wait/notify hooks",
        "contention/fairness measurements"};
    synchronization.status = statusFor(synchronization);

    RuntimeCapability futures{
        "E034", "future-continuations", RuntimeCapabilityStatus::Missing, {}, {}};
    if (hasAll({"__nativesharp_future_continue", "__nativesharp_future_when_all",
                "__nativesharp_future_when_any"}))
        futures.evidence.push_back("continuation and aggregate-future hooks");
    else
        futures.missing = {"continuation scheduling contract", "WhenAll/WhenAny hooks",
            "result slots", "cooperative cancellation propagation"};
    futures.status = statusFor(futures);

    RuntimeCapability timeZones{
        "E035", "windows-time-zones", RuntimeCapabilityStatus::Missing, {}, {}};
    if (hasSymbol("__nativesharp_clock_query_system_filetime"))
        timeZones.evidence.push_back("raw Windows system FILETIME hook");
    timeZones.missing = {"Windows time-zone database/version", "UTC/local conversion",
        "DST ambiguity and invalid-time policy", "versioned rule lookup"};
    timeZones.status = statusFor(timeZones);

    return {std::move(sharedMemory), std::move(synchronization),
        std::move(futures), std::move(timeZones)};
}

std::string runtimeCapabilityReport()
{
    std::string result = "NativeSharp runtime capability report v1\n";
    result += "target: x86_64-pc-windows-msvc\n";
    result += "runtime-abi-fingerprint: " + runtimeAbiContractFingerprintHex() + "\n";
    for (const RuntimeCapability& capability : runtimeCapabilities())
    {
        result += capability.id + " " + capability.name + " status=" +
            runtimeCapabilityStatusName(capability.status) + "\n";
        for (const std::string& evidence : capability.evidence)
            result += "  evidence: " + evidence + "\n";
        for (const std::string& missing : capability.missing)
            result += "  missing: " + missing + "\n";
    }
    return result;
}

bool runtimeCapabilityReady(std::string_view id, std::string& detail)
{
    detail.clear();
    const std::vector<RuntimeCapability> capabilities = runtimeCapabilities();
    const auto found = std::ranges::find_if(capabilities, [id](const RuntimeCapability& capability)
    {
        return capability.id == id;
    });
    if (found == capabilities.end())
    {
        detail = "unknown runtime capability '" + std::string(id) + "'";
        return false;
    }
    if (found->status != RuntimeCapabilityStatus::Ready)
    {
        detail = found->id + " " + found->name + " is " +
            runtimeCapabilityStatusName(found->status);
        if (!found->missing.empty()) detail += "; missing " + found->missing.front();
        return false;
    }
    detail = found->id + " " + found->name + " is ready";
    return true;
}
}
