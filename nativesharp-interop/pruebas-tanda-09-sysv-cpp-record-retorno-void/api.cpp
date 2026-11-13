#include "api.h"
namespace T09Cpp {
void __attribute__((sysv_abi)) most(Small p, int* out, Other q) { *out=p.x+q.z; p.x=999; q.x=999; }
Callback fetchCallback() { return most; }
}
