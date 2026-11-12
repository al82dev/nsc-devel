#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5,3.5};
    short direct=T09Wide::sum(value,7), indirect=T09Wide::fetch()(value,7);
    return !(direct==-1224 && indirect==-1224 && value.x==10.25);
}
