#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused=0; }
#endif
namespace T09HfaCpp {
int __attribute__((regcall)) nested(Nested a, int bias, OtherNested b) { int result = (int)a.pair.x+2*(int)a.pair.y+3*(int)a.z+bias+5*(int)b.x+7*(int)b.y; b.y = 999; return result; }
int __attribute__((regcall)) array(Array a, int bias, OtherArray b) { int result = (int)a.values[0]+2*(int)a.values[1]+3*(int)a.values[2]+bias+5*(int)b.x+7*(int)b.y; b.y = 999; return result; }
NestedFn fetchNested(){return nested;} ArrayFn fetchArray(){return array;}
}
