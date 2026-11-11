#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedVF { float x, y, z; } T09SysvMixedVF;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedVF sysv_mixed_vf(T09SysvMixedVF value);
