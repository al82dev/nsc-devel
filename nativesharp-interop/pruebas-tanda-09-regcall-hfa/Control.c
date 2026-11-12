#include "api.h"
int _fltused = 0;
int main(void)
{
    T09Hfa original = { (float)11, (float)23 };
    T09Hfa direct = t09_hfa_transform(original);
    T09Hfa indirect = t09_hfa_get()(original);
    return !(original.x == (float)11 && original.y == (float)23 &&
             direct.x == (float)14 && direct.y == (float)18 &&
             indirect.x == (float)14 && indirect.y == (float)18);
}
