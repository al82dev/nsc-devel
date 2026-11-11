#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvDifferentPairs { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvDifferentPairs;
typedef struct T09Wide { unsigned char x,y,z,w,a,b,c,d,e,f,g,h,i,j,k,l; } T09Wide;
API_STORAGE __attribute__((sysv_abi)) T09SysvDifferentPairs sysv_different_pairs(T09SysvDifferentPairs first, T09Wide second, int n);
