#include "api.h"
int main(void) { int value = 7; int* result = t09_regcall(&value,3); t09_regvoid(result,4u); return result == &value && value == 14 ? 0 : 1; }
