#ifndef LIM_STR_H
#define LIM_STR_H
unsigned lim_strlen(const char* text);
#ifdef LIM_STR_IMPLEMENTATION
unsigned lim_strlen(const char* text) { unsigned n = 0; while (text[n]) ++n; return n; }
#endif
#endif
