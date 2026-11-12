#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, float scale, Small q, double drift, int bias) { p.x+=q.x; p.y+=bias+(int)(scale*2+drift); p.z+=q.z; q.x=999; p.w+=4; p.v+=5; p.u+=6; return p; }
Callback fetchCallback() { return most; }
}
