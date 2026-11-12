#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct T09Three { float x, y, z; } T09Three;
typedef struct T09Four { float x, y, z, w; } T09Four;
typedef T09Three (__attribute__((regcall)) *T09ThreeFn)(T09Three);
typedef T09Four (__attribute__((regcall)) *T09FourFn)(T09Four);
API T09Three __attribute__((regcall)) t09_three(T09Three value);
API T09Four __attribute__((regcall)) t09_four(T09Four value);
API T09ThreeFn t09_get_three(void);
API T09FourFn t09_get_four(void);
