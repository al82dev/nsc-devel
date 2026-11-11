#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int (__attribute__((regcall)) *T09RegCallback)(int a, int b, int c);
API_STORAGE int __attribute__((regcall)) t09_regcall(int a, int b, int c);
API_STORAGE T09RegCallback t09_get_callback(void);
