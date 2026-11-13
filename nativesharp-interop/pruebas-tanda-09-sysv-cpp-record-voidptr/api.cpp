#include "api.h"
namespace T09Cpp {
void* __attribute__((sysv_abi)) most(Small p, void* out, Other q) { *static_cast<int*>(out)+=p.x+q.z; p.x=999; q.x=999; return out; }
Callback fetchCallback() { return most; }
}
