#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Byte { int x; } T09Byte;
typedef struct T09Wide { float x,y; } T09Wide;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) unsigned char sysv_narrow_byte_record(float factor, T09Byte first, T09Wide wide, unsigned char small, int n);
