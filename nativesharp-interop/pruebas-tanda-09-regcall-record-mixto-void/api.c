#include "api.h"
void __attribute__((regcall)) write_pair(PairMixed value, int* out) {
    *out=(int)(value.x + value.y*2);
    value.x=999.0f;
}
WriteFn get_write(void) { return write_pair; }
