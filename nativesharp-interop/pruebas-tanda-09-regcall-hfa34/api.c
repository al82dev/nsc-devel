#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Three __attribute__((regcall)) t09_three(T09Three value) {
    value.x+=1.0f; value.y+=2.0f; value.z+=3.0f; return value;
}
T09Four __attribute__((regcall)) t09_four(T09Four value) {
    value.x+=1.0f; value.y+=2.0f; value.z+=3.0f; value.w+=4.0f; return value;
}
T09ThreeFn t09_get_three(void) { return t09_three; }
T09FourFn t09_get_four(void) { return t09_four; }
