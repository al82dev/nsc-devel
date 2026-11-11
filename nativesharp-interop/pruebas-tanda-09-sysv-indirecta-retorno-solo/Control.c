#include "api.h"
int main(void) {
    T09OnlyResultCallback callback = t09_get_only_result_callback();
    T09OnlyResult result = callback(7);
    return result.x != 7 || result.y != 17 || result.z != 27;
}
