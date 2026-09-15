#include <stdarg.h>

extern "C" int _fltused = 0;

extern "C" __declspec(dllexport) int NSVarargProbe(int tag, ...)
{
    va_list arguments;
    va_start(arguments, tag);
    const double promoted_float = va_arg(arguments, double);
    const int promoted_bool = va_arg(arguments, int);
    const double direct_double = va_arg(arguments, double);
    const int promoted_byte = va_arg(arguments, int);
    const int promoted_short = va_arg(arguments, int);
    const unsigned int direct_uint = va_arg(arguments, unsigned int);
    const long long direct_long = va_arg(arguments, long long);
    int* const pointer = va_arg(arguments, int*);
    int (*const callback)(int) = va_arg(arguments, int (*)(int));
    va_end(arguments);

    if (tag != 10) return 1;
    if (promoted_float != 1.5) return 2;
    if (promoted_bool != 1) return 3;
    if (direct_double != 2.25) return 4;
    if (promoted_byte != 7 || promoted_short != -3) return 5;
    if (direct_uint != 4000000000U || direct_long != 5000000000LL) return 6;
    if (pointer == nullptr || *pointer != 99) return 7;
    if (callback == nullptr || callback(21) != 42) return 8;
    return 42;
}
