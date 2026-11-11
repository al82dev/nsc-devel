#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedVD { float x, y; double z; } T09SysvMixedVD;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedVD sysv_mixed_vd(T09SysvMixedVD value);
