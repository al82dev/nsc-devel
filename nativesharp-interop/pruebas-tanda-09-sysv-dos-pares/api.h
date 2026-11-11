#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvTwoPairs { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvTwoPairs;
API_STORAGE __attribute__((sysv_abi)) T09SysvTwoPairs sysv_two_pairs(T09SysvTwoPairs first, T09SysvTwoPairs second, int n);
