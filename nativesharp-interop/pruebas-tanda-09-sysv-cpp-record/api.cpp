#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p) { p.x+=1; return p; }
Callback fetchCallback() { return most; }
}
