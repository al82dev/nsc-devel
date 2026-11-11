#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvPairSmall { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvPairSmall;
typedef struct T09Tiny { unsigned char x,y,z,w; } T09Tiny;
API_STORAGE __attribute__((sysv_abi)) T09SysvPairSmall sysv_pair_small(T09SysvPairSmall first, T09Tiny second, int n);
