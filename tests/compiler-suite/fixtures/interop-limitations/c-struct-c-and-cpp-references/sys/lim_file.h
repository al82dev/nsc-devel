/* A system C header reached by a C reference and by a C++ reference (UCRT's stdio.h, with `struct _iobuf` and the
   inline `fprintf`, reached directly and through a C++ header that includes <cstdio>). */
#ifndef LIM_FILE_H
#define LIM_FILE_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lim_iobuf { void* placeholder; } LIM_FILE;
__inline LIM_FILE* lim_stream(void) { return 0; }
__inline int lim_put(LIM_FILE* file, int value, ...) { return file == 0 ? value + 2 : value; }
#ifdef __cplusplus
}
#endif
#endif
