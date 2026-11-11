#include "api.h"
int main(void) {
    T09IndirectLarge value = {1, 2, 3, 4, 5};
    T09LargeCallback callback = t09_get_large_callback();
    return callback(value) != 55 || value.a != 1 || value.b != 2 || value.c != 3 || value.d != 4 || value.e != 5;
}
