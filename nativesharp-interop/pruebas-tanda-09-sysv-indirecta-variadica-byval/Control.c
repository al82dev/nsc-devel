#include "api.h"
int main(void) {
    T09IndirectVarRecord value = {1, 2, 3, 4, 5};
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(1, value) != 1 || value.a != 1 || value.e != 5;
}
