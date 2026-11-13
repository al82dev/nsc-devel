#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, unsigned int u, long long n, unsigned long long w, int* v) {
 p.x+=(u==4000000000U ? 1 : 0); p.y+=(n==-5000000000LL ? 2 : 0); p.z+=(w==5000000000ULL ? 3 : 0);
 *v+=4; return p;
}
Callback fetchCallback() { return most; }
}