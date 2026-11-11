#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedVV { float x, y, z, w; } T09SysvMixedVV;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedVV sysv_mixed_vv(T09SysvMixedVV value);
