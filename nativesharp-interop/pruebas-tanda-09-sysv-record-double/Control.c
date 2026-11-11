#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09Small a={1,2,3,4};
    T09Small r=sysv_record_double(a,7.0);
    if(a.x!=1 || r.x!=8 || r.y!=2 || r.z!=3 || r.w!=11) return 1;
    return 0;
}
