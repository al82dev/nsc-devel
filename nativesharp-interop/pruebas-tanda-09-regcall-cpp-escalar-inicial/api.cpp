#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(int bias, Nested v, OtherNested w) { v.pair.x+=w.pair.x+(float)bias; v.pair.y+=w.pair.y; v.z+=w.pair.x; return v; }
Array __attribute__((regcall)) array(int bias, Array v, OtherArray w) { v.values[0]+=w.values[0]+(float)bias; v.values[1]+=w.values[1]; v.values[2]+=w.values[0]; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
