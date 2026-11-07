#include "Api.h"
_Atomic(int) atomic_global = 3;
int c_read_atomic_global(void) { return atomic_global; }
