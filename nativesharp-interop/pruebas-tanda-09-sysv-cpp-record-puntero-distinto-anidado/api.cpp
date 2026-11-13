#include "api.h"
namespace T09Cpp {
int __attribute__((sysv_abi)) most(Small p, Extra** out, Other q) { (*out)->x+=p.x; p.x=999; q.x=999; return (*out)->x+q.z; }
Callback fetchCallback() { return most; }
}
