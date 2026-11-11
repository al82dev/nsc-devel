#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Record { int x; int y; } T09Record;
typedef T09Record* (__attribute__((preserve_most)) *MostPtr)(T09Record*, int);
typedef void (__attribute__((preserve_most)) *MostVoid)(int**, int);
typedef int** (__attribute__((preserve_all)) *AllPtr)(int**, int);
typedef void (__attribute__((preserve_all)) *AllVoid)(T09Record*, int);
API_STORAGE T09Record* __attribute__((preserve_most)) t09_most(T09Record*, int);
API_STORAGE void __attribute__((preserve_most)) t09_most_void(int**, int);
API_STORAGE int** __attribute__((preserve_all)) t09_all_ptr(int**, int);
API_STORAGE void __attribute__((preserve_all)) t09_all(T09Record*, int);
API_STORAGE MostPtr t09_get_most(void);
API_STORAGE MostVoid t09_get_most_void(void);
API_STORAGE AllPtr t09_get_all_ptr(void);
API_STORAGE AllVoid t09_get_all(void);
