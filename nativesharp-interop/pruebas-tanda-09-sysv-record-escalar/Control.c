#include "api.h"
int main(void) {
    T09Small a={1,2,3,4};
    T09Small r=sysv_record_scalar(a,7);
    if(a.x!=1 || r.x!=8 || r.y!=2 || r.z!=3 || r.w!=11) return 1;
    return 0;
}
