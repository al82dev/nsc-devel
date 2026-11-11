#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvScalarPairs { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvScalarPairs;
typedef struct T09Wide { unsigned char x,y,z,w,a,b,c,d,e,f,g,h,i,j,k,l; } T09Wide;
API_STORAGE __attribute__((sysv_abi)) T09SysvScalarPairs sysv_scalar_pairs(int n, T09SysvScalarPairs first, T09Wide second);
