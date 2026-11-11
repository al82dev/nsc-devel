#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { double x; int y; } T09Byte;
typedef struct T09Wide { long long x,y,z,w; } T09Wide;
typedef struct T09Out { unsigned int a; double b; } T09Out;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Out sysv_padded_pair_id(float factor, T09Byte first, T09Out probe, T09Wide wide, int n);
