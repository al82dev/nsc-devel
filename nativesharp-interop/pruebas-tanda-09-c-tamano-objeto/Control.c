#include "api.h"
int main(void) {
    int value = 1;
    if (t09_s0(&value) != 4 || t09_s1(&value) != 4 || t09_s2(&value) != 4 || t09_s3(&value) != 4) return 1;
    if (t09_d0(&value) != 4 || t09_d1(&value) != 4 || t09_d2(&value) != 4 || t09_d3(&value) != 4) return 2;
    struct T09CArray a;
    const int i = 1;
    if (t09_s0(&a.values[0]) != 20 || t09_s2(&a.values[1]) != 16) return 3;
    if (t09_s1(&a.values[0]) != 8 || t09_s3(&a.values[i]) != 4) return 4;
    if (t09_d1(&a.values[i]) != 4 || t09_d3(&a.values[0]) != 8) return 5;
    return 0;
}
