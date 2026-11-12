#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Nested __attribute__((regcall)) nested(Nested v, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last) { v.pair.x+=((float)delta+2.0f*(float)factor+3.0f*(float)extra+4.0f*(float)fourth+5.0f*(float)last); v.pair.y+=2.0f; v.z+=3.0f; return v; }
Array __attribute__((regcall)) array(Array v, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last) { v.values[0]+=((float)delta+2.0f*(float)factor+3.0f*(float)extra+4.0f*(float)fourth+5.0f*(float)last); v.values[1]+=2.0f; v.values[2]+=3.0f; return v; }
NestedFn fetchNested() { return nested; }
ArrayFn fetchArray() { return array; }
}
