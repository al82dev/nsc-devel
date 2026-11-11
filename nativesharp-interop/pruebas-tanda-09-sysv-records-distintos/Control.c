#include "api.h"
int main(void) {
    T09Small a={1,2,3,4}; T09Wide b={5,6,7,8,9,10,11,12};
    T09Small r=sysv_hetero(a,b);
    if(a.x!=1 || b.x!=5 || r.x!=15 || r.y!=6 || r.z!=3 || r.w!=12) return 1;
    return 0;
}
