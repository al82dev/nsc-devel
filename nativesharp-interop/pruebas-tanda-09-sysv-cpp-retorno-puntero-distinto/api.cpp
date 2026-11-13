#include "api.h"
namespace T09Cpp {
Extra* __attribute__((sysv_abi)) most(Small p, Extra* out, Other q) { out->x+=p.x+q.z; p.x=999; q.x=999; return out; }
Callback fetchCallback() { return most; }
}
