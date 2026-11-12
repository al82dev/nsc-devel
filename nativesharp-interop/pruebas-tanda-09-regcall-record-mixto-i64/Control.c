#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value = {10.25f,20.5};
    long long direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==4294967354LL && indirect==4294967354LL && value.x==10.25f);
}
