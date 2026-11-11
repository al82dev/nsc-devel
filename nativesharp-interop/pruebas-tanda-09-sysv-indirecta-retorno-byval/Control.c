#include "api.h"
int main(void) {
    T09LargeReturn input = {1, 2, 3, 4, 5};
    T09LargeReturnCallback callback = t09_get_large_return_callback();
    T09LargeReturn result = callback(input);
    return input.a != 1 || input.b != 2 || input.c != 3 || input.d != 4 || input.e != 5 ||
        result.a != 11 || result.b != 22 || result.c != 33 || result.d != 44 || result.e != 55;
}
