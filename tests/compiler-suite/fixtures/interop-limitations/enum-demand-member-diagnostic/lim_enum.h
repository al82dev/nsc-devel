/* C header with an enum in a function signature: the record observation builds a preliminary member model. */
#ifndef LIM_ENUM_H
#define LIM_ENUM_H
typedef enum LimMode { LIM_MODE_A = 1, LIM_MODE_B = 2 } LimMode;
int lim_mode_value(LimMode mode);
#ifdef LIM_ENUM_IMPLEMENTATION
int lim_mode_value(LimMode mode) { return (int)mode; }
#endif
#endif
