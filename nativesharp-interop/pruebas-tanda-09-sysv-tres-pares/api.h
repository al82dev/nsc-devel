#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvThreePairs { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvThreePairs;
API_STORAGE __attribute__((sysv_abi)) T09SysvThreePairs sysv_three_pairs(T09SysvThreePairs first, T09SysvThreePairs second, T09SysvThreePairs third, int n);
