#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvPair16Mix { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l; } T09SysvPair16Mix;
API_STORAGE __attribute__((sysv_abi)) T09SysvPair16Mix sysv_pair16_mix(T09SysvPair16Mix value, int a, float b);
