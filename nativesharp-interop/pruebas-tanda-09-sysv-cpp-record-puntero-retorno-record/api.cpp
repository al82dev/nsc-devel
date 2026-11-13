#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, Small* out) { out->x+=p.x; p.y+=out->x; return p; }
Callback fetchCallback() { return most; }
}
