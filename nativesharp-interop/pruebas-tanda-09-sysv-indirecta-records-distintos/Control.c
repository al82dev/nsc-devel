#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09MixedDistinct first = {1.5, 3}; T09TripleDistinct second = {4, 5, 6};
    T09DistinctCallback callback = t09_get_distinct_callback();
    return callback(first, second, 5) != 55 || first.x != 1.5 || first.y != 3 ||
        second.x != 4 || second.y != 5 || second.z != 6;
}
