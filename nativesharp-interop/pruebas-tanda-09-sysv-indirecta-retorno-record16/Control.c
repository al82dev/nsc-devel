#include "api.h"
int main(void) {
    T09ReturnRecord input = {1, 2, 3, 4};
    T09ReturnCallback callback = t09_get_return_callback();
    T09ReturnRecord result = callback(input);
    return input.x != 1 || input.y != 2 || input.z != 3 || input.w != 4 || result.x != 11 || result.y != 22 || result.z != 33 || result.w != 44;
}
