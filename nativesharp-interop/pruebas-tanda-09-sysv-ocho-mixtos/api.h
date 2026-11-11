#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixed { unsigned char x, y, z, w; } T09SysvMixed;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixed sysv_mixed(T09SysvMixed a, T09SysvMixed b, int c, T09SysvMixed d, int e, T09SysvMixed f, int g, T09SysvMixed h);
