#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE int* __attribute__((regcall)) t09_regcall(int *a, int increment);
API_STORAGE void __attribute__((regcall)) t09_regvoid(int *a, unsigned int increment);
