#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
static double value; double* __attribute__((sysv_abi)) most(Small p, short* out, Other q) { *out=(short)(p.x+q.z); value=317.5; p.x=999; q.x=999; return &value; }
Callback fetchCallback() { return most; }
}
