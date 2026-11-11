#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int* (__attribute__((preserve_most)) *MostPtr)(int*, int);
typedef void (__attribute__((preserve_most)) *MostVoid)(int*, int);
typedef int* (__attribute__((preserve_all)) *AllPtr)(int*, int);
typedef void (__attribute__((preserve_all)) *AllVoid)(int*, int);
API_STORAGE int* __attribute__((preserve_most)) t09_most(int*, int);
API_STORAGE void __attribute__((preserve_most)) t09_most_void(int*, int);
API_STORAGE int* __attribute__((preserve_all)) t09_all_ptr(int*, int);
API_STORAGE void __attribute__((preserve_all)) t09_all(int*, int);
API_STORAGE MostPtr t09_get_most(void);
API_STORAGE MostVoid t09_get_most_void(void);
API_STORAGE AllPtr t09_get_all_ptr(void);
API_STORAGE AllVoid t09_get_all(void);
