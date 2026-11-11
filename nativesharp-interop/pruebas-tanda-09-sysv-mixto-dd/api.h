#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedDD { double x, y; } T09SysvMixedDD;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedDD sysv_mixed_dd(T09SysvMixedDD value);
