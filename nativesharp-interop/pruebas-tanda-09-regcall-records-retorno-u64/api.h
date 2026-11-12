#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
typedef struct Other { Pair pair; } Other;
API unsigned long long __attribute__((regcall)) combine(Triple a, int bias, Other b);
typedef unsigned long long (__attribute__((regcall)) *CombineFn)(Triple, int, Other);
API CombineFn get_combine(void);
