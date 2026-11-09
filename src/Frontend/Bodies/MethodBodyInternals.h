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
// EN: T08-D17: in a pointer to pointer (`T const**`), the pointee qualifier belongs to the base type, as in C. A
//     `T**` cannot become a `T const**` implicitly: a `T const*` stored through the result would then be writable
//     through the original `T**` (C++ rejects it; NativeSharp cannot spell `T const* const*`).
// ES: T08-D17: en un puntero a puntero (`T const**`), el calificador del apuntado es del tipo base, como en C. Un
//     `T**` no pasa implicitamente a `T const**`: un `T const*` guardado mediante el resultado quedaria escribible
//     mediante el `T**` original (C++ lo rechaza; NativeSharp no puede escribir `T const* const*`).
bool nestedPointerTypeKey(std::string_view typeKey) noexcept;
bool addsNestedPointeeConst(
    std::string_view sourceTypeKey,
    bool sourcePointeeReadonly,
    std::string_view targetTypeKey,
    bool targetPointeeReadonly) noexcept;
inline constexpr char kNestedPointeeConstMessage[] =
    "a pointer to a mutable-pointee pointer cannot become a 'T const**' implicitly: a 'T const*' stored through "
    "it would be writable through the original 'T**'; use an explicit cast";
}
