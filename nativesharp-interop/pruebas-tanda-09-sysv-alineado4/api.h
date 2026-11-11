#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvA4 { int x, y, z, a, b, c; } T09SysvA4;
API_STORAGE __attribute__((sysv_abi)) T09SysvA4 sysv_a4(T09SysvA4 value);
