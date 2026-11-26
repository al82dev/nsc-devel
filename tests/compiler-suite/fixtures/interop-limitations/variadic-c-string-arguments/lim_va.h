/* A C variadic function: the total length of its strings up to a NULL, after a `_Nonnull` first string. */
#ifndef LIM_VA_H
#define LIM_VA_H
#include <stdarg.h>
int lim_total_length(const char* _Nonnull first, ...);
#ifdef LIM_VA_IMPLEMENTATION
int lim_total_length(const char* _Nonnull first, ...)
{
    int total = 0;
    for (const char* s = first; s != 0; )
    {
        while (s[0] != 0) { total = total + 1; s = s + 1; }
        break;
    }
    va_list args;
    va_start(args, first);
    for (const char* s = va_arg(args, const char*); s != 0; s = va_arg(args, const char*))
        while (*s != 0) { total = total + 1; s = s + 1; }
    va_end(args);
    return total;
}
#endif
#endif
