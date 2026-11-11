#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09IndirectMixed value = {1.5, 7};
    T09MixedCallback callback = t09_get_mixed_callback();
    return callback(value) != 42 || value.x != 1.5 || value.y != 7;
}
