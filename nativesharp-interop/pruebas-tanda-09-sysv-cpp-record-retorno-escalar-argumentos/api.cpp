#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
int __attribute__((sysv_abi)) most(Small p, signed char a, unsigned char b, short c, unsigned short d, bool flag, unsigned int u, long long l, unsigned long long ul, float f, double g, Other q) {
 int result=(p.x==10)+(q.z==300)+(a==-7)+(b==250)+(c==-1000)+(d==65000)+(flag?1:0)+(u==4000000000U)+(l==-5000000000LL)+(ul==9000000000ULL)+(f==1.5f)+(g==2.25);
 p.x=999; q.z=999; return result;
}
Callback fetchCallback() { return most; }
}
