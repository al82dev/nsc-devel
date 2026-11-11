#include "api.h"
int main(void) {
    T09SysvPairSmall first={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    T09Tiny second={21, 22, 23, 24};
    T09SysvPairSmall result=sysv_pair_small(first,second,7);
    if (first.x!=1 || second.x!=21 || result.x!=29 || result.h!=24 || result.g!=11) return 1;
    return 0;
}
