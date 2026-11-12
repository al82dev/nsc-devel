#include "api.h"
int _fltused = 0;
int main(void) {
    Triple value = {{10.0f, 20.0f}, 30.0f};
    Result direct = combine(value, 7);
    Result indirect = get_combine()(value, 7);
    return !(direct.x == 17 && direct.y == 50 && indirect.x == 17 && indirect.y == 50 && value.pair.x == 10.0f);
}
