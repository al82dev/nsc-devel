#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value = {10.25f,20.5};
    float direct=sum_pair(value,7), indirect=get_sum()(value,7);
    return !(direct==58.25f && indirect==58.25f && value.x==10.25f);
}
