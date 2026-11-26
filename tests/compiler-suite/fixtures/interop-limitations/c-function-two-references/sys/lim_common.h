/* A system C header reached through two references (ucrt stdio.h reached through stb_image.h and
   directly, which both declare the inline snprintf). */
#ifndef LIM_COMMON_H
#define LIM_COMMON_H
__inline int lim_twice(int value, ...) { return value * 2; }
#endif
