#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE extern int sysv_value;
API_STORAGE __attribute__((sysv_abi)) int sysv_mix(int a, int b, int c, int d, int e, int f);
API_STORAGE __attribute__((sysv_abi)) void sysv_set(int value);
API_STORAGE __attribute__((sysv_abi)) double sysv_fp(double a, float b, int c, double d);
API_STORAGE __attribute__((sysv_abi)) float sysv_float(float value);
API_STORAGE __attribute__((sysv_abi)) int* sysv_pointer(int* value, int delta);
typedef int (*T09Callback)(int, int, int, int, int, int);
