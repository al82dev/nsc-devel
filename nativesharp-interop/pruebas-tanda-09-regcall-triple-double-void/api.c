#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
void __attribute__((regcall)) write_pair(PairMixed value, int* out) {
    *out=(int)(value.x + value.y*2 + value.z*3);
    value.x=999.0f;
}
WriteFn get_write(void) { return write_pair; }
