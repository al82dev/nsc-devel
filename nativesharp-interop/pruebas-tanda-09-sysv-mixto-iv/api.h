#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedIV { int x, y; float z, w; } T09SysvMixedIV;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedIV sysv_mixed_iv(T09SysvMixedIV value);
