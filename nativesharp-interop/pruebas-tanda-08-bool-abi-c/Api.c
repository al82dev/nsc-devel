#include "Api.h"
static _Bool saved;
_Bool c_bool_identity(_Bool value) { return value; }
_Bool c_bool_mixed(int prefix, _Bool value, int suffix) { return prefix == 3 && value && suffix == 7; }
void c_bool_save(_Bool value) { saved = value; }
int c_bool_saved(void) { return saved; }
