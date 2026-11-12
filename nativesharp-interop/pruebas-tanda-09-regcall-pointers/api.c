#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Nested __attribute__((regcall)) t09_nested(T09Nested value, unsigned char* p, short* q, double* r, void* identity) {
    value.pair.x+=((float)(*p)+(float)(*q)+(float)(*r)+(identity==p ? 1.0f : -1000.0f)); *p+=1; *q+=1; *r+=1.0; value.pair.y+=2.0f; value.z+=3.0f; return value;
}
T09Array __attribute__((regcall)) t09_array(T09Array value, unsigned char* p, short* q, double* r, void* identity) {
    value.values[0]+=((float)(*p)+(float)(*q)+(float)(*r)+(identity==p ? 1.0f : -1000.0f)); *p+=1; *q+=1; *r+=1.0; value.values[1]+=2.0f; value.values[2]+=3.0f; return value;
}
T09NestedFn t09_get_nested(void) { return t09_nested; }
T09ArrayFn t09_get_array(void) { return t09_array; }
