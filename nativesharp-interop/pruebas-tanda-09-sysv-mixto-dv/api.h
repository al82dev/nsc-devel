#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedDV { double x; float y, z; } T09SysvMixedDV;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedDV sysv_mixed_dv(T09SysvMixedDV value);
