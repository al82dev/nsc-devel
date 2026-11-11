#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef unsigned char* (__attribute__((preserve_most)) *MostPtr)(unsigned char*, int);
typedef void (__attribute__((preserve_most)) *MostVoid)(void*, int);
typedef short* (__attribute__((preserve_all)) *AllPtr)(short*, int);
typedef void (__attribute__((preserve_all)) *AllVoid)(void*, int);
API_STORAGE unsigned char* __attribute__((preserve_most)) t09_most(unsigned char*, int);
API_STORAGE void __attribute__((preserve_most)) t09_most_void(void*, int);
API_STORAGE short* __attribute__((preserve_all)) t09_all_ptr(short*, int);
API_STORAGE void __attribute__((preserve_all)) t09_all(void*, int);
API_STORAGE MostPtr t09_get_most(void);
API_STORAGE MostVoid t09_get_most_void(void);
API_STORAGE AllPtr t09_get_all_ptr(void);
API_STORAGE AllVoid t09_get_all(void);
