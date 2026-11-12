#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
typedef struct Result { double x, y; } Result;
API Result __attribute__((regcall)) combine(int bias);
typedef Result (__attribute__((regcall)) *CombineFn)(int);
API CombineFn get_combine(void);

