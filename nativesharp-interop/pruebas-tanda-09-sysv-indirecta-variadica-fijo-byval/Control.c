#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectVarRecord value = {1, 2, 3, 4, 5};
    T09OtherVarRecord second = {3.5, 11};
    T09IndirectVarCallback callback = t09_get_var_record_callback();
    return callback(value, second) != 1 || value.a != 1 || value.e != 5 ||
        second.x != 3.5 || second.y != 11;
}
