#include "api.h"
int main(void) { T09RegCallback callback = t09_get_callback(); T09RegCallback direct = t09_regcall; return callback == direct && callback(3, 4, 7) == 347 && direct(3, 4, 7) == 347 ? 0 : 1; }
