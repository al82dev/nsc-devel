#include "api.h"
short* __attribute__((regcall)) t09_regcall(short *a, int increment) { *a += increment; return a; }
void __attribute__((regcall)) t09_regvoid(short *a, unsigned int increment) { *a += (int)increment; }
T09PtrCallback t09_get_callback(void) { return t09_regcall; }
T09VoidCallback t09_get_void(void) { return t09_regvoid; }
