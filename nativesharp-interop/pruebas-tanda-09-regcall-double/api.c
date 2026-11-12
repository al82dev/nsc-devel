#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
double __attribute__((regcall)) t09_double_transform(double a, int* b, double c) {
    if (a != 2.25 || *b != 7 || c != 3.5) return -1.0;
    *b = 19; return a + c;
}
T09DoubleFn t09_double_get(void) { return t09_double_transform; }
