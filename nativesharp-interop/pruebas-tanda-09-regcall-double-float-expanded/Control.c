#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value={10.25,20.5,3.5}; PairFloat extra={2,3};
    int direct=sum_pair(value,7,extra), indirect=get_sum()(value,7,extra);
    return !(direct==91 && indirect==91 && value.x==10.25 && extra.x==2);
}
