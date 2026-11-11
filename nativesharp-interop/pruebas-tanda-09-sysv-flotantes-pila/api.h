#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Small { unsigned char x, y, z, w; } T09Small;
API_STORAGE __attribute__((sysv_abi)) T09Small sysv_float_stack(T09Small a, float f1, double f2, float f3, double f4, float f5, double f6, float f7, double f8, float f9);
