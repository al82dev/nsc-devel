#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct H2 { float x, y; } H2;
API_STORAGE H2 __vectorcall transform_h2(H2 value, int scale);
