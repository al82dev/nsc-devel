#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
typedef struct Other { Pair pair; float z; } Other;
API Triple __attribute__((regcall)) combine(Triple a, Other b, int bias);
typedef Triple (__attribute__((regcall)) *CombineFn)(Triple, Other, int);
API CombineFn get_combine(void);
