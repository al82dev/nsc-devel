#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR15 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k; } T09SysvR15;
API_STORAGE __attribute__((sysv_abi)) T09SysvR15 sysv_r15(T09SysvR15 value);
