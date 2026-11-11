#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectVarRecord value = {2.25, 7};
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(1, value, 13) != 250 || value.x != 2.25 || value.y != 7;
}
