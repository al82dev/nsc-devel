#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR32 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, A, B, C, D, E, F; } T09SysvR32;
API_STORAGE __attribute__((sysv_abi)) T09SysvR32 sysv_r32(T09SysvR32 value);
