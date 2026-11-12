#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Nested __attribute__((regcall)) t09_nested(T09Nested value, const double* input, int* out) {
    value.pair.x+=(float)*input; *out+=1; value.pair.y+=2.0f; value.z+=3.0f; return value;
}
T09Array __attribute__((regcall)) t09_array(T09Array value, const double* input, int* out) {
    value.values[0]+=(float)*input; *out+=1; value.values[1]+=2.0f; value.values[2]+=3.0f; return value;
}
T09NestedFn t09_get_nested(void) { return t09_nested; }
T09ArrayFn t09_get_array(void) { return t09_array; }
