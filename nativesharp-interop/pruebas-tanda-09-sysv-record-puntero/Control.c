#include "api.h"
int main(void) {
    T09Small a={1,2,3,4}; int n=7;
    T09Small r=sysv_record_pointer(a,&n);
    if(a.x!=1 || n!=9 || r.x!=8 || r.y!=2 || r.z!=3 || r.w!=11) return 1;
    return 0;
}
