#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value = {10.25f,20.5};
    _Bool direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct && indirect && !sum_pair(value,8) && value.x==10.25f);
}
