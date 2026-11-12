#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, int b, int c, int d, int e, int f, int g, int h, int i) { v.pair.x+=(float)(b+2*c+3*d+4*e+5*f+6*g+7*h+8*i); v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, int b, int c, int d, int e, int f, int g, int h, int i) { v.values[0]+=(float)(b+2*c+3*d+4*e+5*f+6*g+7*h+8*i); v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
