#include "api.h"
__declspec(selectany) int _fltused = 0;
double __attribute__((preserve_none)) t09_most(double x, int* p, short s, _Bool b) {
  if (x != 2.25 || *p != 7 || s != -7 || !b) return -1.0;
  *p = 19; return 9.5;
}
long long __attribute__((preserve_none)) t09_all(long long x, unsigned int y, float z) {
  if (x != -5000000000LL || y != 4000000000u || z != 3.5f) return 0;
  return -4999999990LL;
}

T09MostCallback t09_get_most(void) { return t09_most; }
T09AllCallback t09_get_all(void) { return t09_all; }
