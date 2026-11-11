#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { double x; int y; } T09Byte;
typedef struct __attribute__((packed)) T09Wide { unsigned char x; double y; } T09Wide;
typedef struct T09Out { int x; } T09Out;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Out sysv_packed_small_return_single(float factor, T09Byte first, T09Wide wide, int n);
