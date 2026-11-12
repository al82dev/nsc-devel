#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value = {10.25,20.5,3.5};
    long long direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==4294967364LL && indirect==4294967364LL && value.x==10.25f);
}
