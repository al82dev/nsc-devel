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
typedef signed char (__attribute__((sysv_abi)) *T09S8Callback)(signed char);
typedef unsigned char (__attribute__((sysv_abi)) *T09U8Callback)(unsigned char);
typedef short (__attribute__((sysv_abi)) *T09S16Callback)(short);
typedef unsigned short (__attribute__((sysv_abi)) *T09U16Callback)(unsigned short);
typedef _Bool (__attribute__((sysv_abi)) *T09BoolCallback)(_Bool);
typedef int (__attribute__((sysv_abi)) *T09VarCallback)(int, ...);
API_STORAGE T09SysvCallback t09_get_sysv_callback(void);
API_STORAGE __attribute__((sysv_abi)) int t09_sysv_sum(int, int, int, int, int, int);
API_STORAGE T09FloatCallback t09_get_float_callback(void);
API_STORAGE T09FloatOneCallback t09_get_float_one_callback(void);
API_STORAGE T09WideCallback t09_get_wide_callback(void);
API_STORAGE T09UIntCallback t09_get_uint_callback(void);
API_STORAGE T09ULongCallback t09_get_ulong_callback(void);
API_STORAGE T09PointerCallback t09_get_pointer_callback(void);
API_STORAGE T09VoidCallback t09_get_void_callback(void);
API_STORAGE T09S8Callback t09_get_s8_callback(void);
API_STORAGE T09U8Callback t09_get_u8_callback(void);
API_STORAGE T09S16Callback t09_get_s16_callback(void);
API_STORAGE T09U16Callback t09_get_u16_callback(void);
API_STORAGE T09BoolCallback t09_get_bool_callback(void);
API_STORAGE T09VarCallback t09_get_variadic_callback(void);
API_STORAGE int t09_normal(int, int, int, int, int, int);
