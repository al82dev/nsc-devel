#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE double __attribute__((preserve_most)) t09_most(double x, int* p, short s, _Bool b);
API_STORAGE long long __attribute__((preserve_all)) t09_all(long long x, unsigned int y, float z);
