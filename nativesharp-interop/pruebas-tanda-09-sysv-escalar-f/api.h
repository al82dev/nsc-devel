#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvScalarF { float x; } T09SysvScalarF;
API_STORAGE __attribute__((sysv_abi)) T09SysvScalarF sysv_scalar_f(T09SysvScalarF value);
