#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { unsigned char x,y,z,w,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t; } T09Byte;
typedef struct __attribute__((aligned(16))) T09Wide { long long x,y,z,w; } T09Wide;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Result sysv_indirect_diff16(float factor, T09Byte first, int n, T09Wide second);
