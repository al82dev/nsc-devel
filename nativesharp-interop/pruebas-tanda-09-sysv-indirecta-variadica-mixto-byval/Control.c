#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectVarRecord value = {1, 2, 3, 4, 5};
    T09IndirectVarMixed mixed = {2.25, 7};
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(1, mixed, value) != 1 || value.a != 1 || value.e != 5 || mixed.x != 2.25 || mixed.y != 7;
}
