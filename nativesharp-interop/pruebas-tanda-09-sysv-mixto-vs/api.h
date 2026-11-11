#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedVS { float z, w; short x, y; } T09SysvMixedVS;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedVS sysv_mixed_vs(T09SysvMixedVS value);
