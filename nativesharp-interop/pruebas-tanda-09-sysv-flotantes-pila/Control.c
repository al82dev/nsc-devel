#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09Small a={1,2,3,4};
    T09Small r=sysv_float_stack(a, 1.0f, 2.0, 3.0f, 4.0, 5.0f, 6.0, 7.0f, 8.0, 9.0f);
    if(a.x!=1 || r.x!=46 || r.y!=2 || r.z!=3 || r.w!=49) return 1;
    return 0;
}
