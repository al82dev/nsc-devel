#include "Api.h"
_Bool c_flag = 0;
_Bool c_values[2] = {0, 1};
int c_check(void) { return c_flag && c_values[0] && !c_values[1]; }
void c_set(void) { c_flag = 0; c_values[0] = 0; c_values[1] = 1; }
