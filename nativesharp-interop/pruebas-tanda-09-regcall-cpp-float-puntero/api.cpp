#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { float __attribute__((regcall)) calculate(float a, int *b, float c) {
    if (a != 2.25f || c != 3.5f || *b != 7) return -1.0f;
    *b = 19; return 5.5f;
} }
