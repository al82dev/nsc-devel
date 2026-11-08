#include "Api.h"
static _Bool values[2] = {0, 1};
_Bool* c_bool_pointer = values;
_Bool* c_bool_get(void) { return values; }
int c_bool_check(void) { return values[0] && !values[1]; }
int c_bool_probe(int index) { return c_bool_pointer[index]; }
void c_bool_write(int index, _Bool value) { c_bool_pointer[index] = value; }
void c_set_through_pointer(_Bool* value, _Bool next) { *value = next; }
int c_read_through_pointer(const _Bool* value) { return *value; }
