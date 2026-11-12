#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, int delta, float factor, double extra) { v.pair.x+=((float)delta+2.0f*factor+3.0f*(float)extra); v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, int delta, float factor, double extra) { v.values[0]+=((float)delta+2.0f*factor+3.0f*(float)extra); v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
