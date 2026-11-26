/* Stand-in for stdio.h: it includes the stream macros from a system include directory. */
#ifndef LIM_STDIO_H
#define LIM_STDIO_H
#include <lim_streams.h>
#ifdef LIM_STDIO_IMPLEMENTATION
static LimFile lim_streams[3] = {{10}, {11}, {12}};
static int lim_error = 30;
LimFile* lim_iob_func(unsigned index) { return &lim_streams[index]; }
int* lim_errno_func(void) { return &lim_error; }
int lim_handle(LimFile* stream) { return stream->handle; }
#endif
#endif
