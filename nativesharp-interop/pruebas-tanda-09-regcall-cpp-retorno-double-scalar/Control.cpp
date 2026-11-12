#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Compact::Triple value{{10.0f,20.0f},30.0f};
    auto direct=T09Compact::combine(7);
    auto indirect=T09Compact::fetch()(7);
    return !(direct.x==17.25 && direct.y==50.5 && indirect.x==17.25 && indirect.y==50.5 && value.pair.x==10.0f);
}


