#if __has_include("optional.h")
#define BASE 13
#else
#define BASE 11
#endif
#include "nested.h"
static inline int getValue(void) { return BASE + NEST; }
