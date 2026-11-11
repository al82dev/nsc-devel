#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09MixedPair first = {1.5, 3}, second = {2.5, 4};
    T09MixedPairCallback callback = t09_get_mixed_pair_callback();
    return callback(first, second, 5) != 26 || first.x != 1.5 || first.y != 3 || second.x != 2.5 || second.y != 4;
}
