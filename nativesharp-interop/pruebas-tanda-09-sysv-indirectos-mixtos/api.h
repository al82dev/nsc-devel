#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvIndirectMix { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t; } T09SysvIndirectMix;
API_STORAGE __attribute__((sysv_abi)) T09SysvIndirectMix sysv_indirect_mix(float factor, T09SysvIndirectMix first, int n, T09SysvIndirectMix second);
