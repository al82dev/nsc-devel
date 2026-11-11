#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvPairMix { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvPairMix;
API_STORAGE __attribute__((sysv_abi)) T09SysvPairMix sysv_pair_mix(T09SysvPairMix value, int a, float b);
