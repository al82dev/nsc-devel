#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedVI { float z, w; int x, y; } T09SysvMixedVI;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedVI sysv_mixed_vi(T09SysvMixedVI value);
