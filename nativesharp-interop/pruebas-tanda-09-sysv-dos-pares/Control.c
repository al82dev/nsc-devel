#include "api.h"
int main(void) {
    T09SysvTwoPairs first={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    T09SysvTwoPairs second={21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    T09SysvTwoPairs result=sysv_two_pairs(first,second,7);
    if (first.x!=1 || second.x!=21 || result.x!=29 || result.h!=32 || result.g!=11) return 1;
    return 0;
}
