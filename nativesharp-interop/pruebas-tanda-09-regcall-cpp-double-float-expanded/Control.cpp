#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5,3.5}; T09Wide::PairFloat extra={2,3};
    int direct=T09Wide::sum(value,7,extra), indirect=T09Wide::fetch()(value,7,extra);
    return !(direct==91 && indirect==91 && value.x==10.25 && extra.x==2);
}
