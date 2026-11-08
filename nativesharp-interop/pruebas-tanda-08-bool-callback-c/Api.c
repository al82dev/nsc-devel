#include "Api.h"
typedef _Bool (*BoolCallback)(int, _Bool);
static _Bool c_bool_callback(int tag, _Bool value) { return tag == 7 && !value; }
void* c_get_bool_callback(void) { return (void*)c_bool_callback; }
int c_invoke_bool_callback(void* callback) {
    BoolCallback f = (BoolCallback)callback;
    return f(7, 0) && !f(7, 1) && !f(2, 0);
}
