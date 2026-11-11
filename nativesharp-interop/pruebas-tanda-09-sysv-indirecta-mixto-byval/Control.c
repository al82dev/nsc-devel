#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SplitValue small = {1.5, 7}; T09ByvalValue large = {1, 2, 3, 4, 5};
    T09SplitByvalCallback callback = t09_get_split_byval_callback();
    return callback(3, small, large, 10) != 42 || small.x != 1.5 || small.y != 7 ||
        large.a != 1 || large.b != 2 || large.c != 3 || large.d != 4 || large.e != 5;
}
