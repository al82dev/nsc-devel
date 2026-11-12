#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Wide::PairDouble value{10.25,20.5};
    int direct=0, indirect=0;
    T09Wide::sum(value,&direct); T09Wide::fetch()(value,&indirect);
    return !(direct==51 && indirect==51 && value.x==10.25);
}
