#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Three __attribute__((regcall)) three(Three v) { v.x+=1.0f; v.y+=2.0f; v.z+=3.0f; return v; }
Four __attribute__((regcall)) four(Four v) { v.x+=1.0f; v.y+=2.0f; v.z+=3.0f; v.w+=4.0f; return v; }
ThreeFn fetchThree() { return three; }
FourFn fetchFour() { return four; }
}
