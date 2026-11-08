#pragma once

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
};
}
