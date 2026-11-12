#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value = {10.25f,20.5};
    short direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==-1234 && indirect==-1234 && value.x==10.25f);
}
