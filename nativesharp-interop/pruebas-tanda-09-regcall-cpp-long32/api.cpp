#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, long delta, unsigned long factor, double extra) { v.pair.x+=((delta==-7 && factor==4294967295UL) ? (float)extra : -1000.0f); v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, long delta, unsigned long factor, double extra) { v.values[0]+=((delta==-7 && factor==4294967295UL) ? (float)extra : -1000.0f); v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
