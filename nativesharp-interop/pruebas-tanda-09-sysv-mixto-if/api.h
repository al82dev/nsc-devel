#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedIF { int x, y; float z; } T09SysvMixedIF;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedIF sysv_mixed_if(T09SysvMixedIF value);
