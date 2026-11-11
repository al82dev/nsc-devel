#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedFD { float x; double y; } T09SysvMixedFD;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedFD sysv_mixed_fd(T09SysvMixedFD value);
