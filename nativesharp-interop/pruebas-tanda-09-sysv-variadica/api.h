#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE __attribute__((sysv_abi)) int sysv_varargs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_many(int count, ...);
typedef struct T09VarRecord { int x; int y; } T09VarRecord;
