#include "api.h"
int _fltused = 0;
int main(void) {
    Triple a = {{10.0f, 20.0f}, 30.0f};
    Other b = {{1.0f, 2.0f}};
    Triple c = combine(a, b, 7);
    Triple d = get_combine()(a, b, 7);
    return !(c.pair.x == 18.0f && c.pair.y == 22.0f && c.z == 31.0f &&
             d.pair.x == 18.0f && d.pair.y == 22.0f && d.z == 31.0f &&
             a.pair.x == 10.0f && b.pair.x == 1.0f);
}
