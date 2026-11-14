#include "bad.h"

__declspec(selectany) int _fltused = 0;

/* Returns a 20-byte record whose fields expose every fixed argument. The variadic part is not read:
   Clang's Windows target provides no SysV va_list, so only the fixed arguments are observable. */
__attribute__((sysv_abi)) T09Unsupported sysv_record(float first, T09Other second, float third, ...)
{
    T09Unsupported result;
    result.x = (int)first + (int)(second.x & 0xffffffffLL);
    result.y = (int)(second.x >> 32);
    result.z = (int)(second.y - 3999999000u);
    result.w = (int)third;
    result.extra = (int)(first * 10.0f) + (int)(third * 100.0f);
    return result;
}
