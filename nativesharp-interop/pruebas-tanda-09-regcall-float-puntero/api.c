#include "api.h"
__declspec(selectany) int _fltused = 0;
float __attribute__((regcall)) t09_regcall(float a, int *b, float c) {
    if (a != 2.25f || c != 3.5f || *b != 7) return -1.0f;
    *b = 19;
    return 5.5f;
}
