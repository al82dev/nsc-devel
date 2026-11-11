#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedDI { double x; int y; } T09SysvMixedDI;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedDI sysv_mixed_di(T09SysvMixedDI value);
