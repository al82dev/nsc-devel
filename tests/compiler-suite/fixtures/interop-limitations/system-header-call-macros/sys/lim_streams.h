/* System-header stand-in for corecrt_wstdio.h: `#define stderr (__acrt_iob_func(2))`. */
#ifndef LIM_STREAMS_H
#define LIM_STREAMS_H
typedef struct LimFile { int handle; } LimFile;
LimFile* lim_iob_func(unsigned index);
int* lim_errno_func(void);
int lim_handle(LimFile* stream);
#define lim_stderr (lim_iob_func(2))
#define lim_errno (*lim_errno_func())
#endif
