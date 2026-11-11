#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvSmallPair { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvSmallPair;
typedef struct T09Tiny { unsigned char x,y,z,w; } T09Tiny;
API_STORAGE __attribute__((sysv_abi)) T09SysvSmallPair sysv_small_pair(T09Tiny second, T09SysvSmallPair first, int n);
