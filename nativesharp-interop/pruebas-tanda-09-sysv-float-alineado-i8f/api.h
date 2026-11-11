#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { double x; int y; } T09Byte;
typedef struct T09Wide { long long x,y,z,w; } T09Wide;
typedef struct T09Out { unsigned char a; float b __attribute__((aligned(8))); } T09Out;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Out sysv_float_aligned_i8f(float factor, T09Byte first, T09Out probe, T09Wide wide, int n);
