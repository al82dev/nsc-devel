#include "Api.h"
int pair_size(void) { return (int)sizeof(Pair); }
int pair_alignment(void) { return (int)_Alignof(Pair); }
int pair_b_offset(void) { return (int)__builtin_offsetof(Pair, b); }
