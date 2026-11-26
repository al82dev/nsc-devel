#ifndef LIM_FN_H
#define LIM_FN_H
#include <stddef.h>
#define LIM_TWENTY 20
typedef unsigned (*lim_len_fn)(const char* text, unsigned extra, int flag, size_t size);
extern lim_len_fn lim_len;
#ifdef LIM_FN_IMPLEMENTATION
static unsigned lim_len_impl(const char* text, unsigned extra, int flag, size_t size)
{
    unsigned n = 0;
    while (text[n]) ++n;
    return n + extra + (unsigned)flag + (unsigned)size;
}
lim_len_fn lim_len = lim_len_impl;
#endif
#endif
