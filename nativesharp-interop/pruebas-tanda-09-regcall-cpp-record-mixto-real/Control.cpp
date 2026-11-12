#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5};
    double direct=T09Wide::sum(value,7), indirect=T09Wide::fetch()(value,7);
    return !(direct==58.25 && indirect==58.25 && value.x==10.25);
}

