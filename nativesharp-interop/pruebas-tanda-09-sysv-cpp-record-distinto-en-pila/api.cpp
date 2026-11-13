extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p0, int bias, Small p1, Small p2, Small p3, Small p4, Other p5) { if (p1.x!=2 || p1.y!=11 || p1.z!=21 || p2.x!=3 || p2.y!=12 || p2.z!=22 || p3.x!=4 || p3.y!=13 || p3.z!=23 || p4.x!=5 || p4.y!=14 || p4.z!=24 || p5.x!=6 || p5.y!=15 || p5.z!=25) { p0.z=-99; return p0; } p0.x+=p1.x+p2.x+p3.x+p4.x+p5.x; p0.z+=bias+p1.z+p2.z+p3.z+p4.z+p5.z; return p0; }
Callback fetchCallback() { return most; }
}
