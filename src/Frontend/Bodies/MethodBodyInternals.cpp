#include "MethodBodyInternals.h"

#include <fstream>
#include <mutex>

namespace nativesharp::frontend::method_body_detail
{
std::mutex& traceMutex()
{
    static std::mutex mutex;
    return mutex;
}

void resetMethodBodyTrace()
{
    if constexpr (!kTraceEnabled) return;
    const std::lock_guard<std::mutex> lock(traceMutex());
    std::ofstream trace(kTracePath, std::ios::binary | std::ios::trunc);
    if (!trace) return;
    trace << "nsc-method-bodies-trace version=1\r\n";
    trace.flush();
}

void writeMethodBodyTrace(std::string_view event, std::string_view detail)
{
    if constexpr (!kTraceEnabled) return;
    const std::lock_guard<std::mutex> lock(traceMutex());
    std::ofstream trace(kTracePath, std::ios::binary | std::ios::app);
    if (!trace) return;
    trace << "[NSC METHOD BODIES INSTRUMENTATION CODE] event=" << event;
    if (!detail.empty()) trace << '|' << detail;
    trace << "\r\n";
    trace.flush();
}

language::PointerContract pointerContract(
    bool nullable,
    bool pointeeReadonly,
    bool hasProvenance,
    std::size_t knownAlignment,
    unsigned addressSpace) noexcept
{
    return language::PointerContract{
        nullable,
        !pointeeReadonly,
        hasProvenance,
        knownAlignment,
        addressSpace};
}
}
