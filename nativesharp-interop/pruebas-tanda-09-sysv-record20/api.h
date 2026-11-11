#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR20 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p; } T09SysvR20;
API_STORAGE __attribute__((sysv_abi)) T09SysvR20 sysv_r20(T09SysvR20 value);
