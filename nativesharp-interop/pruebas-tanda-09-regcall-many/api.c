#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Nested __attribute__((regcall)) t09_nested(T09Nested value, int b, int c, int d, int e, int f, int g, int h, int i) {
    value.pair.x+=(float)(b+2*c+3*d+4*e+5*f+6*g+7*h+8*i); value.pair.y+=2.0f; value.z+=3.0f; return value;
}
T09Array __attribute__((regcall)) t09_array(T09Array value, int b, int c, int d, int e, int f, int g, int h, int i) {
    value.values[0]+=(float)(b+2*c+3*d+4*e+5*f+6*g+7*h+8*i); value.values[1]+=2.0f; value.values[2]+=3.0f; return value;
}
T09NestedFn t09_get_nested(void) { return t09_nested; }
T09ArrayFn t09_get_array(void) { return t09_array; }
