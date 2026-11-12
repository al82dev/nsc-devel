#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5,3.5}; int a=0,b=0;
    int* x=T09Wide::sum(value,&a); int* y=T09Wide::fetch()(value,&b);
    return !(x==&a && y==&b && a==61 && b==61 && value.x==10.25);
}
