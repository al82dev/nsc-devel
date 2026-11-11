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
API_STORAGE __attribute__((sysv_abi)) double* sysv_double_pointer(double* value);
API_STORAGE __attribute__((sysv_abi)) void* sysv_void_pointer(void* value);
API_STORAGE __attribute__((sysv_abi)) int** sysv_pointer_pointer(int** value);
API_STORAGE __attribute__((sysv_abi)) long long sysv_wide(long long value, unsigned int delta);
API_STORAGE __attribute__((sysv_abi)) unsigned int sysv_unsigned(unsigned int value);
API_STORAGE __attribute__((sysv_abi)) unsigned long long sysv_unsigned64(unsigned long long value);
API_STORAGE __attribute__((sysv_abi)) signed char sysv_s8(signed char value);
API_STORAGE __attribute__((sysv_abi)) unsigned char sysv_u8(unsigned char value);
API_STORAGE __attribute__((sysv_abi)) short sysv_s16(short value);
API_STORAGE __attribute__((sysv_abi)) unsigned short sysv_u16(unsigned short value);
API_STORAGE __attribute__((sysv_abi)) _Bool sysv_bool(_Bool value);
typedef int (*T09Callback)(int, int, int, int, int, int);
