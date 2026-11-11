#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { int value = 7; float result = T09Cpp::calculate(2.25f, &value, 3.5f); return result == 5.5f && value == 19 ? 0 : 1; }
