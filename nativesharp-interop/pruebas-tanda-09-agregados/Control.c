#include "api.h"
int main(void) {
    R3 a; a.x = 1; a.y = 2; a.z = 3;
    R3 b = pass3(a);
    if (a.x != 1 || b.x != 11 || b.y != 2 || b.z != 3) return 1;
    R4 c; c.x = 4;
    R4 d = pass4(c);
    if (c.x != 4 || d.x != 14) return 2;
    R8 e; e.x = 5; e.y = 6;
    R8 f = pass8(e);
    if (e.x != 5 || f.x != 15 || f.y != 6) return 3;
    R24 g; g.a = 1; g.b = 2; g.c = 3; g.d = 4; g.e = 5; g.f = 6;
    R24 h = pass24(g);
    if (g.a != 1 || h.a != 11 || h.b != 2 || h.c != 3 || h.d != 4 || h.e != 5 || h.f != 6) return 4;
    return 0;
}
