#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, unsigned int u, long long n, unsigned long long w, float* v) {
 p.x+=(u==4000000000U ? 1 : 0); p.y+=(n==-5000000000LL ? 2 : 0); p.z+=(w==5000000000ULL ? 3 : 0);
 *v+=4; return p;
}
#pragma pack(push,1)
struct Guard { long long before; float value; int after; };
#pragma pack(pop)
static Guard slot{1234567,5.0f,7654321};
Callback fetchCallback() { return most; }
float* guarded() { return &slot.value; }
int checkGuard() { return slot.before==1234567 && slot.after==7654321; }
}