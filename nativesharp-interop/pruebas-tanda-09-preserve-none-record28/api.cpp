#include "api.h"
namespace T09Cpp {
Small __attribute__((preserve_none)) most(Small p) { p.x+=1; p.q+=2; return p; }
Callback fetchCallback() { return most; }
}
