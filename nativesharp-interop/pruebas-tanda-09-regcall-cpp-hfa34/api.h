#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09HfaCpp {
struct Three { float x, y, z; };
struct Four { float x, y, z, w; };
using ThreeFn = Three (__attribute__((regcall)) *)(Three);
using FourFn = Four (__attribute__((regcall)) *)(Four);
API Three __attribute__((regcall)) three(Three value);
API Four __attribute__((regcall)) four(Four value);
API ThreeFn fetchThree();
API FourFn fetchFour();
}
