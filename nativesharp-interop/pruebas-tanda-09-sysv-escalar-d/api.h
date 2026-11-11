#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvScalarD { double x; } T09SysvScalarD;
API_STORAGE __attribute__((sysv_abi)) T09SysvScalarD sysv_scalar_d(T09SysvScalarD value);
