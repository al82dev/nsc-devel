#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
typedef struct Other { int x; int y; int z; int w; int u; int v; } Other;
API int __attribute__((regcall)) combine(Triple a, int bias, Other b);
typedef int (__attribute__((regcall)) *CombineFn)(Triple, int, Other);
API CombineFn get_combine(void);
