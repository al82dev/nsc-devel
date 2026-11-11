#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE __attribute__((sysv_abi)) int sysv_varargs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_many(int count, ...);
typedef struct T09VarRecord { int x; int y; } T09VarRecord;
API_STORAGE __attribute__((sysv_abi)) int sysv_ptrs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_empty(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_floatspill(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_fixed_ptrs(T09VarRecord *record, short *value, ...);
API_STORAGE __attribute__((sysv_abi)) T09VarRecord *sysv_return_ptr(T09VarRecord *record, ...);
