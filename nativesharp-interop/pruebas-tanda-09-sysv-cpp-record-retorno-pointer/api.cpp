#include "api.h"
namespace T09Cpp {
static int value; int* __attribute__((sysv_abi)) most(Small p, int bias, Other q) { value=p.x+bias+q.z; p.x=999; q.x=999; return &value; }
Callback fetchCallback() { return most; }
}
