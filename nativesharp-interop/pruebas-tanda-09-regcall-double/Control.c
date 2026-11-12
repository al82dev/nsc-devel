#include "api.h"
int _fltused = 0;
int main(void)
{
    int a = 7, b = 7;
    double direct = t09_double_transform(2.25, &a, 3.5);
    double indirect = t09_double_get()(2.25, &b, 3.5);
    return !(direct == 5.75 && indirect == 5.75 && a == 19 && b == 19);
}
