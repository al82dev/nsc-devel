#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Compact::Triple value{{10.0f,20.0f},30.0f};
    auto direct=T09Compact::combine(value,7);
    auto indirect=T09Compact::fetch()(value,7);
    return !(direct.x==17 && direct.y==50 && direct.z==3 && direct.v==6 && indirect.x==17 && indirect.y==50 && indirect.z==3 && indirect.v==6 && value.pair.x==10.0f);
}

