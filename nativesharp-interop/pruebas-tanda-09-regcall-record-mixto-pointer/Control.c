#include "api.h"
int _fltused = 0;
int main(void) {
    PairDouble value={10.25f,20.5}; int a=0,b=0;
    int* x=sum_pair(value,&a); int* y=get_sum()(value,&b);
    return !(x==&a && y==&b && a==51 && b==51 && value.x==10.25f);
}
