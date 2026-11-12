#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Nested __attribute__((regcall)) t09_nested(T09Nested value, long long delta, unsigned int factor, unsigned long long extra, int* p, double more) {
    value.pair.x+=((delta==4294967308LL ? 12.0f : -1000.0f)+2.0f*(float)factor+3.0f*(extra==4294967301ULL ? 5.0f : -1000.0f)+4.0f*(float)(*p)+5.0f*(float)more); *p+=1; value.pair.y+=2.0f; value.z+=3.0f; return value;
}
T09Array __attribute__((regcall)) t09_array(T09Array value, long long delta, unsigned int factor, unsigned long long extra, int* p, double more) {
    value.values[0]+=((delta==4294967308LL ? 12.0f : -1000.0f)+2.0f*(float)factor+3.0f*(extra==4294967301ULL ? 5.0f : -1000.0f)+4.0f*(float)(*p)+5.0f*(float)more); *p+=1; value.values[1]+=2.0f; value.values[2]+=3.0f; return value;
}
T09NestedFn t09_get_nested(void) { return t09_nested; }
T09ArrayFn t09_get_array(void) { return t09_array; }
