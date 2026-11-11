#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct R3 { unsigned char x, y, z; } R3;
typedef struct R4 { int x; } R4;
typedef struct R8 { int x, y; } R8;
typedef struct R24 { int a, b, c, d, e, f; } R24;
API_STORAGE R3 pass3(R3 value);
API_STORAGE R4 pass4(R4 value);
API_STORAGE R8 pass8(R8 value);
API_STORAGE R24 pass24(R24 value);
