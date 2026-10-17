#pragma once

#include "nativesharp/Language/AbiSemantics.h"

#include <cstddef>
#include <fstream>
#include <mutex>
#include <string_view>

namespace nativesharp::frontend::method_body_detail
{
// [NSC METHOD BODIES INSTRUMENTATION CODE]
// Crash-surviving breadcrumbs retained for directed diagnosis. Production builds keep the switch
// false so callable analysis performs no synchronous filesystem work.
inline constexpr bool kTraceEnabled = false;
inline constexpr char kTracePath[] = R"(A:\temp\nsc-method-bodies-trace.log)";

inline std::mutex& traceMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline void resetMethodBodyTrace()
{
    if constexpr (!kTraceEnabled) return;
    const std::lock_guard<std::mutex> lock(traceMutex());
    std::ofstream trace(kTracePath, std::ios::binary | std::ios::trunc);
    if (!trace) return;
    trace << "nsc-method-bodies-trace version=1\r\n";
    trace.flush();
}

inline void writeMethodBodyTrace(std::string_view event, std::string_view detail = {})
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

inline language::PointerContract pointerContract(
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
