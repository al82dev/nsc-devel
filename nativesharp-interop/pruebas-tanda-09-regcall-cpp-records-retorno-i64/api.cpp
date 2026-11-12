#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused=0; }
#endif
namespace T09HfaCpp {
long long __attribute__((regcall)) nested(Nested a, int bias, OtherNested b) { return 4294967296LL + (int)a.pair.x+2*(int)a.pair.y+3*(int)a.z+bias+5*(int)b.pair.x+7*(int)b.pair.y; }
long long __attribute__((regcall)) array(Array a, int bias, OtherArray b) { return 4294967296LL + (int)a.values[0]+2*(int)a.values[1]+3*(int)a.values[2]+bias+5*(int)b.values[0]+7*(int)b.values[1]; }
NestedFn fetchNested(){return nested;} ArrayFn fetchArray(){return array;}
}
