#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5,3.5};
    int direct=0, indirect=0;
    T09Wide::sum(value,&direct); T09Wide::fetch()(value,&indirect);
    return !(direct==61 && indirect==61 && value.x==10.25);
}
