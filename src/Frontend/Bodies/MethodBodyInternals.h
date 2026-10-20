#pragma once

#include "nativesharp/Language/AbiSemantics.h"

#include <cstddef>
#include <mutex>
#include <string_view>

namespace nativesharp::frontend::method_body_detail
{
// [NSC METHOD BODIES INSTRUMENTATION CODE]
// Crash-surviving breadcrumbs retained for directed diagnosis. Production builds keep the switch
// false so callable analysis performs no synchronous filesystem work.
inline constexpr bool kTraceEnabled = false;
inline constexpr char kTracePath[] = R"(A:\temp\nsc-method-bodies-trace.log)";

std::mutex& traceMutex();
void resetMethodBodyTrace();
void writeMethodBodyTrace(std::string_view event, std::string_view detail = {});
language::PointerContract pointerContract(
    bool nullable,
    bool pointeeReadonly,
    bool hasProvenance,
    std::size_t knownAlignment,
    unsigned addressSpace) noexcept;
}
