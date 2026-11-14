#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) { int count = 3; return t09_csum(&count, -4, 1, -3, 2.5f, 7) == 32 ? 0 : 1; }
