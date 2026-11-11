#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE unsigned long long __attribute__((regcall)) t09_regcall(unsigned int a, long long b, unsigned long long c);
