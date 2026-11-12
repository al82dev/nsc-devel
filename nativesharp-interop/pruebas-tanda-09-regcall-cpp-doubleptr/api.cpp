#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, unsigned char** p, short** q, double** r) { v.pair.x+=((float)(**p)+(float)(**q)+(float)(**r)+1.0f); **p+=1; **q+=1; **r+=1.0; v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, unsigned char** p, short** q, double** r) { v.values[0]+=((float)(**p)+(float)(**q)+(float)(**r)+1.0f); **p+=1; **q+=1; **r+=1.0; v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
