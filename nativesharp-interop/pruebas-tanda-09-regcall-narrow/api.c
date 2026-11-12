#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Nested __attribute__((regcall)) t09_nested(T09Nested value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last) {
    value.pair.x+=((float)delta+2.0f*(float)factor+3.0f*(float)extra+4.0f*(float)fourth+5.0f*(float)last); value.pair.y+=2.0f; value.z+=3.0f; return value;
}
T09Array __attribute__((regcall)) t09_array(T09Array value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last) {
    value.values[0]+=((float)delta+2.0f*(float)factor+3.0f*(float)extra+4.0f*(float)fourth+5.0f*(float)last); value.values[1]+=2.0f; value.values[2]+=3.0f; return value;
}
T09NestedFn t09_get_nested(void) { return t09_nested; }
T09ArrayFn t09_get_array(void) { return t09_array; }
