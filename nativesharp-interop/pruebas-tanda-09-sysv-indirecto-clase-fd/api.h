#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { float x; double y; } T09Byte;
typedef struct T09Wide { long long x,y,z,w; } T09Wide;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Result sysv_class_fd(float factor, T09Byte first, int n, T09Wide second);
