#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, long long delta, unsigned int factor, unsigned long long extra, int* p, double more) { v.pair.x+=((delta==4294967308LL ? 12.0f : -1000.0f)+2.0f*(float)factor+3.0f*(extra==4294967301ULL ? 5.0f : -1000.0f)+4.0f*(float)(*p)+5.0f*(float)more); *p+=1; v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, long long delta, unsigned int factor, unsigned long long extra, int* p, double more) { v.values[0]+=((delta==4294967308LL ? 12.0f : -1000.0f)+2.0f*(float)factor+3.0f*(extra==4294967301ULL ? 5.0f : -1000.0f)+4.0f*(float)(*p)+5.0f*(float)more); *p+=1; v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
