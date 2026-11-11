#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedV { float x, y; } T09SysvMixedV;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedV sysv_mixed_v(T09SysvMixedV value);
