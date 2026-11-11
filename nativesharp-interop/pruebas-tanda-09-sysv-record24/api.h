#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR24 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t; } T09SysvR24;
API_STORAGE __attribute__((sysv_abi)) T09SysvR24 sysv_r24(T09SysvR24 value);
