#pragma once

#include "nativesharp/Language/PointerLevelQualifiers.h"
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
// EN: C1-04. The same rule for the volatile base mark (`T volatile**`): a `T**` does not become a `T volatile**`
//     implicitly, as in C, because a `T volatile*` stored through it would then be read without volatile through
//     the original `T**`.
// ES: C1-04. La misma regla para la marca volatile de la base: un `T**` no pasa implicitamente a `T volatile**`,
//     como en C, porque un `T volatile*` guardado mediante el resultado se leeria sin volatile mediante el `T**`.
inline bool addsNestedPointeeVolatile(
    std::string_view sourceTypeKey,
    bool sourcePointeeVolatile,
    std::string_view targetTypeKey,
    bool targetPointeeVolatile) noexcept
{
    return addsNestedPointeeConst(sourceTypeKey, sourcePointeeVolatile, targetTypeKey, targetPointeeVolatile);
}
// EN: C1-02. The whole rule for chains with level masks (Language/PointerLevelQualifiers.h); null when the implicit
//     conversion keeps every qualifier as C allows. The messages of T08-D17 and C1-04 stay for the base cases.
// ES: C1-02. La regla completa para cadenas con mascara de niveles; nulo si la conversion implicita conserva cada
//     calificador como C permite. Los mensajes de T08-D17 y C1-04 se conservan para los casos de la base.
const char* nestedPointerQualificationError(
    std::string_view sourceTypeKey, std::uint8_t sourceLevels, bool sourceBaseConst, bool sourceBaseVolatile,
    std::string_view targetTypeKey, std::uint8_t targetLevels, bool targetBaseConst, bool targetBaseVolatile) noexcept;
// EN: C1-02 (item 4.7). A fixed array that decays at a C boundary gives its element qualifiers to the object its
//     first pointer points to: `int* const a[2]` decays to `int* const*`, `const int a[3]` to `int const*`. True when
//     the parameter would drop one of them.
// ES: C1-02 (item 4.7). Un arreglo fijo que decae en una frontera C da sus calificadores de elemento al objeto al
//     que apunta su primer puntero. Verdadero si el parametro perderia alguno.
bool decayedFixedArrayDropsQualifier(
    std::string_view decayedTypeKey, bool elementConst, bool elementVolatile, std::uint8_t arrayLevels,
    bool arrayBaseConst, bool arrayBaseVolatile, std::string_view parameterTypeKey, std::uint8_t parameterLevels,
    bool parameterBaseConst, bool parameterBaseVolatile) noexcept;
inline constexpr char kNestedPointeeVolatileMessage[] =
    "a pointer to a non-volatile-pointee pointer cannot become a 'T volatile**' implicitly: a 'T volatile*' stored "
    "through it would be read without volatile through the original 'T**'; use an explicit cast";
}
