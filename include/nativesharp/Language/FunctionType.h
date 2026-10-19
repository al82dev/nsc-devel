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
};
}
