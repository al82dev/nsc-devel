#pragma once

#include <cstdint>
#include "nativesharp/Language/CallingConvention.h"
#include "nativesharp/Language/FunctionCarrier.h"

#include <string>
#include <vector>

namespace nativesharp::language
{
struct FunctionType final
{
    std::vector<std::string> parameters;
    std::string result;
    FunctionCarrier carrier = FunctionCarrier::Pointer;
    CallingConvention callingConvention = CallingConvention::NativeSharp;
    std::string errorType;
    std::string lifetime;
    bool variadic = false;
    bool isUnsafe = false;
    bool isNoHeap = false;
    bool isAsync = false;
    bool nullable = false;
    // EN: C signature qualification kept in the identity: one flag per parameter whose pointer
    //     pointee is volatile, and one for the result. An empty vector means that no parameter
    //     carries the qualifier; otherwise it has exactly one entry per parameter.
    // ES: Calificación de la firma C conservada en la identidad: una marca por parámetro cuyo dato
    //     apuntado es volatile, y otra para el resultado. Un vector vacío indica que ningún
    //     parámetro la lleva; en otro caso tiene exactamente una entrada por parámetro.
    std::vector<bool> parameterPointeeVolatile;
    bool resultPointeeVolatile = false;
    // EN: The same for a `const` (readonly) pointee, with the same layout rules (T08-002).
    // ES: Lo mismo para un dato apuntado `const` (readonly), con las mismas reglas (T08-002).
    std::vector<bool> parameterPointeeReadonly;
    bool resultPointeeReadonly = false;
    // EN: T19-038. The same for a C `_Atomic` pointee (`_Atomic T*`), with the same layout rules. It has no
    //     NativeSharp spelling: only C function types carry it.
    // ES: T19-038. Lo mismo para un dato apuntado C `_Atomic` (`_Atomic T*`), con las mismas reglas. No tiene
    //     grafia NativeSharp: solo lo llevan los tipos de funcion C.
    std::vector<bool> parameterPointeeAtomic;
    bool resultPointeeAtomic = false;
    std::uint8_t resultPointerLevelQualifiers = 0; // C1-02: intermediate pointer level qualifiers (Language/PointerLevelQualifiers.h).
    std::vector<std::uint8_t> parameterPointerLevelQualifiers; // C1-02: intermediate pointer level qualifiers (Language/PointerLevelQualifiers.h).
    // EN: C3-02. The N of each C `_BitInt(N)` parameter narrower than its carrier, then of the result (0 for any
    //     other type). Empty when none: only C function types carry it, and C passes such a value as `iN`.
    // ES: C3-02. El N de cada parametro C `_BitInt(N)` mas estrecho que su portador y del resultado (0 si no).
    //     Vacio si no hay: solo lo llevan los tipos de funcion C, y C pasa ese valor como `iN`.
    std::vector<std::uint8_t> parameterBitIntWidths;
    std::uint8_t resultBitIntWidth = 0;
};
}
