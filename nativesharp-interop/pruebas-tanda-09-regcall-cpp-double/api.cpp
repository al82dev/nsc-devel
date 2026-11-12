#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09DoubleCpp {
double __attribute__((regcall)) transform(double a, int* b, double c) {
    if (a != 2.25 || *b != 7 || c != 3.5) return -1.0;
    *b=19; return a+c;
}
Callback fetch() { return transform; }
}
