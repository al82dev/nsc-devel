#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09MixedArgs value = {1.5, 7};
    T09MixedArgsCallback callback = t09_get_mixed_args_callback();
    return callback(4, value, 8) != 42 || value.x != 1.5 || value.y != 7;
}
