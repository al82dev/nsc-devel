#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09Small a={1,2,3,4};
    T09Small r=sysv_floats_record(7.0f,a,3.0f);
    if(a.x!=1 || r.x!=11 || r.y!=2 || r.z!=3 || r.w!=14) return 1;
    return 0;
}
