#include "api.h"
__declspec(selectany) int _fltused = 0;
H2 __vectorcall transform_h2(H2 value, int scale) {
    value.x = value.x * scale;
    value.y = value.y + scale;
    return value;
}
