#include "api.h"
int main(void) {
    T09NoArgsCallback callback = t09_get_noargs_callback();
    T09NoArgsResult result = callback();
    return result.x != 7 || result.y != 17 || result.z != 27;
}
