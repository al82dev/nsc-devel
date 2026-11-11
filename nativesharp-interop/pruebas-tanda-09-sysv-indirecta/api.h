#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int (__attribute__((sysv_abi)) *T09SysvCallback)(int, int, int, int, int, int);
typedef double (__attribute__((sysv_abi)) *T09FloatCallback)(double, float, int, double);
typedef float (__attribute__((sysv_abi)) *T09FloatOneCallback)(float);
typedef long long (__attribute__((sysv_abi)) *T09WideCallback)(long long, unsigned int);
typedef unsigned int (__attribute__((sysv_abi)) *T09UIntCallback)(unsigned int);
typedef unsigned long long (__attribute__((sysv_abi)) *T09ULongCallback)(unsigned long long, unsigned int);
typedef int* (__attribute__((sysv_abi)) *T09PointerCallback)(int*, int);
typedef void (__attribute__((sysv_abi)) *T09VoidCallback)(int*);
API_STORAGE T09SysvCallback t09_get_sysv_callback(void);
API_STORAGE T09FloatCallback t09_get_float_callback(void);
API_STORAGE T09FloatOneCallback t09_get_float_one_callback(void);
API_STORAGE T09WideCallback t09_get_wide_callback(void);
API_STORAGE T09UIntCallback t09_get_uint_callback(void);
API_STORAGE T09ULongCallback t09_get_ulong_callback(void);
API_STORAGE T09PointerCallback t09_get_pointer_callback(void);
API_STORAGE T09VoidCallback t09_get_void_callback(void);
API_STORAGE int t09_normal(int, int, int, int, int, int);
