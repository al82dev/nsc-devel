#include "Api.h"
ATOMIC_API _Atomic(int) atomic_global = 3;
ATOMIC_API _Atomic(enum DllMode) enum_global = dll_low;
int c_read_atomic_global(void) { return atomic_global; }
int c_read_enum_global(void) { return (int)enum_global; }
