/* Minimal GLAD-like C header: entry points are function-pointer variables (L-003 through a function pointer). */
#ifndef LIM_FP_H
#define LIM_FP_H
typedef int (*PFNLIMLENGTHPROC)(const char *text);
extern PFNLIMLENGTHPROC lim_length;
#define limLength lim_length
#ifdef LIM_FP_IMPLEMENTATION
static int lim_length_impl(const char *text) { int n = 0; while (text[n] != 0) n = n + 1; return n; }
PFNLIMLENGTHPROC lim_length = lim_length_impl;
#endif
#endif
