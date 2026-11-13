#include "api.h"
namespace T09Cpp {
int __attribute__((sysv_abi)) most(Small p, int bias, Other q) { p.x+=q.x; q.x=999; return p.x+bias+q.z; }
Callback fetchCallback() { return most; }
}