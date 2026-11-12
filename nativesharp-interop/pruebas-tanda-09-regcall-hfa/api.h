#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct T09Hfa { float x, y; } T09Hfa;
typedef T09Hfa (__attribute__((regcall)) *T09HfaFn)(T09Hfa);
API T09Hfa __attribute__((regcall)) t09_hfa_transform(T09Hfa value);
API T09HfaFn t09_hfa_get(void);
