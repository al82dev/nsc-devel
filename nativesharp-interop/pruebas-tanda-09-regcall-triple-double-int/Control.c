#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value={10.25,20.5,3.5};
    int direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==68 && indirect==68 && value.x==10.25);
}
