#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
API_STORAGE int __attribute__((preserve_most)) t09_most(int a, int b, int c);
API_STORAGE int __attribute__((preserve_all)) t09_all(int a, int b, int c);
