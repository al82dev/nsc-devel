#include "api.h"
int main(void) {
    T09SysvCallback callback = t09_get_sysv_callback();
    return callback(1, 2, 3, 4, 5, 6) == 91 ? 0 : 1;
}
