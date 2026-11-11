#include "api.h"
_Bool __attribute__((regcall)) t09_regcall(_Bool value) { return !value; }
T09RegCallback t09_get_callback(void) { return t09_regcall; }
