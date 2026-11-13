#include "api.h"
namespace T09Cpp {
Small* __attribute__((sysv_abi)) most(Small p, Small* out, Other q) { out->x+=p.x+q.z; p.x=999; q.x=999; return out; }
Callback fetchCallback() { return most; }
}
