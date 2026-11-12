#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09HfaCpp {
Pair __attribute__((regcall)) transform(Pair value) {
    value.x += 3.0f; value.y -= 5.0f; return value;
}
}
