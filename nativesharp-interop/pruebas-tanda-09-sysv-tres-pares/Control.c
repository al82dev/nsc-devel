#include "api.h"
int main(void) {
    T09SysvThreePairs first={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    T09SysvThreePairs second={21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    T09SysvThreePairs third={41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52}; T09SysvThreePairs result=sysv_three_pairs(first,second,third,7);
    if (first.x!=1 || second.x!=21 || result.x!=70 || result.h!=52 || result.g!=11) return 1;
    return 0;
}
