#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
int __attribute__((sysv_abi)) most(Small p, int bias, Other q) { if (p.y!=20 || p.z!=30) return -99; p.x+=q.x; q.x=999; return (int)(p.x+bias+q.z); }
Callback fetchCallback() { return most; }
}
