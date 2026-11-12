#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value={10.25,20.5};
    double direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==58.25 && indirect==58.25 && value.x==10.25);
}

