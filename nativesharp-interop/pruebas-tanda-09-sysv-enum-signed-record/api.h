#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef enum T09Color { T09A=-9, T09B=7 } T09Color;
typedef struct T09Byte { int x; } T09Byte;
typedef struct T09Wide { float x,y; } T09Wide;
typedef struct T09Result { int x,y,z,w,extra; } T09Result;
API_STORAGE __attribute__((sysv_abi)) T09Color sysv_enum_signed_record(float factor, T09Byte first, T09Wide wide, T09Color code, int n);
