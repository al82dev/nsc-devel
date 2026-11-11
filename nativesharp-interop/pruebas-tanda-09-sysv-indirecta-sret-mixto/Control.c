#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SretSplit small = {1.5, 7}; T09SretLarge large = {1, 2, 3, 4, 5};
    T09SretMixedCallback callback = t09_get_sret_mixed_callback();
    T09SretLarge result = callback(3, small, large, 10);
    return small.x != 1.5 || small.y != 7 || large.a != 1 || large.b != 2 || large.e != 5 ||
        result.a != 4 || result.b != 9 || result.c != 3 || result.d != 4 || result.e != 15;
}
