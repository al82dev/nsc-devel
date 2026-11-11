#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE float __attribute__((regcall)) t09_regcall(float a, int *b, float c);
