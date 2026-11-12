#include "api.h"
Result __attribute__((regcall)) combine(Triple value, int bias) {
    Result result = {(int)value.pair.x + bias, (int)value.pair.y + (int)value.z};
    value.pair.x = 999.0f;
    return result;
}
CombineFn get_combine(void) { return combine; }
