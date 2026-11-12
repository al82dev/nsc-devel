#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, const double* input, int* out) { v.pair.x+=(float)*input; *out+=1; v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, const double* input, int* out) { v.values[0]+=(float)*input; *out+=1; v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
