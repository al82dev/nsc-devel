#include "api.h"
int main(void) { T09RegCallback callback = t09_get_callback(); return t09_regcall(1) || !t09_regcall(0) || callback(1) || !callback(0); }
