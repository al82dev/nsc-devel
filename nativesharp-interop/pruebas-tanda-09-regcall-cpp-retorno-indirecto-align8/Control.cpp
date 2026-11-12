#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09Compact::Triple value{{10.0f,20.0f},30.0f};
    auto direct=T09Compact::combine(value,7);
    auto indirect=T09Compact::fetch()(value,7);
    return !(direct.x==1099511627793LL && direct.y==-1099511627726LL && direct.z==3 && direct.v==6 &&
             indirect.x==1099511627793LL && indirect.y==-1099511627726LL && indirect.z==3 && indirect.v==6 && value.pair.x==10.0f);
}


