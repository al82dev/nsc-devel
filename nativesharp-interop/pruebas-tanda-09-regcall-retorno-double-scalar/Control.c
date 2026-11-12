#include "api.h"
int _fltused = 0;
int main(void) {
    Triple value={{10.0f,20.0f},30.0f};
    Result direct=combine(7), indirect=get_combine()(7);
    return !(direct.x==17.25 && direct.y==50.5 && indirect.x==17.25 && indirect.y==50.5 && value.pair.x==10.0f);
}

