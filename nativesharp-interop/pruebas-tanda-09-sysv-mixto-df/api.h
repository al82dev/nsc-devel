#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedDF { double x; float y; } T09SysvMixedDF;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedDF sysv_mixed_df(T09SysvMixedDF value);
