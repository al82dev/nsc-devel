#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectVarRecord value = {2.25, 7};
    int target = 13;
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(1, 2, 3, 4, 5, 6, value, &target) != 1 || value.x != 2.25 || value.y != 7 || target != 23;
}
