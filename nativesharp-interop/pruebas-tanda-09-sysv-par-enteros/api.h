#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvPairInts { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvPairInts;
API_STORAGE __attribute__((sysv_abi)) T09SysvPairInts sysv_pair_ints(T09SysvPairInts value, unsigned int a, long long b, unsigned long long c);
