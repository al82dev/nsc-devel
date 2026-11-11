#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvA2 { short x, y, z, a, b, c, d, e, f, g; } T09SysvA2;
API_STORAGE __attribute__((sysv_abi)) T09SysvA2 sysv_a2(T09SysvA2 value);
