#include "api.h"
int main(void) { return t09_regcall(4000000000u, -5000000000LL, ~0ULL) == 0x8000000000000001ULL ? 0 : 1; }
