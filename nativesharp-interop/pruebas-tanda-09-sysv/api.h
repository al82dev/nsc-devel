#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE extern int sysv_value;
API_STORAGE __attribute__((sysv_abi)) int sysv_mix(int a, int b, int c, int d, int e, int f);
API_STORAGE __attribute__((sysv_abi)) void sysv_set(int value);
typedef int (*T09Callback)(int, int, int, int, int, int);
