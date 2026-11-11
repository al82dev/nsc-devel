#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectVarRecord value = {2.25, 7};
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(1, value, 0xffffffffffffffffULL) != 1 || value.x != 2.25 || value.y != 7;
}
