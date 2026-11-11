#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { int value = 7; float result = t09_regcall(2.25f, &value, 3.5f); return result == 5.5f && value == 19 ? 0 : 1; }
