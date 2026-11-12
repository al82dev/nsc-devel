#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, int bias, OtherNested w) { v.pair.x+=w.x+(float)bias; v.pair.y+=(float)w.y; v.z+=w.x; w.y=999; return v; }
Array __attribute__((regcall)) array(Array v, int bias, OtherArray w) { v.values[0]+=w.x+(float)bias; v.values[1]+=(float)w.y; v.values[2]+=w.x; w.y=999; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
