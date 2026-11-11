#include "api.h"
int main(void) {
    if (sysv_mix(1, 2, 3, 4, 5, 6) != 91) return 1;
    sysv_set(41);
    if (sysv_value != 41) return 2;
    return 0;
}
