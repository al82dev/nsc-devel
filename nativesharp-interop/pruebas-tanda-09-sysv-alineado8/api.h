#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvA8 { long long x, y, z; } T09SysvA8;
API_STORAGE __attribute__((sysv_abi)) T09SysvA8 sysv_a8(T09SysvA8 value);
