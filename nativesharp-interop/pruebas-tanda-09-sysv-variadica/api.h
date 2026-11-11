#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE __attribute__((sysv_abi)) int sysv_varargs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_many(int count, ...);
typedef struct T09VarRecord { int x; int y; } T09VarRecord;
typedef struct T09VarMixed { double x; int y; } T09VarMixed;
typedef struct T09VarLarge { long long x; long long y; long long z; } T09VarLarge;
typedef struct __attribute__((packed)) T09VarPacked { double x; int y; } T09VarPacked;
API_STORAGE __attribute__((sysv_abi)) int sysv_ptrs(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_empty(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_floatspill(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_fixed_ptrs(T09VarRecord *record, short *value, ...);
API_STORAGE __attribute__((sysv_abi)) T09VarRecord *sysv_return_ptr(T09VarRecord *record, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_small(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_mixed(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_two(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_pressure(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_sse_pressure(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_large(int count, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_ptrs(int *fixed, ...);
API_STORAGE __attribute__((sysv_abi)) int *sysv_record_return_ptr(int *fixed, ...);
API_STORAGE __attribute__((sysv_abi)) int sysv_record_rptr(int count, ...);
