#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Small { int x; } T09Small;
typedef struct T09Large { unsigned char a,b,c; } T09Large;
typedef T09Small (__attribute__((preserve_most)) *MostPtr)(T09Small);
typedef T09Large (__attribute__((preserve_most)) *MostVoid)(T09Large);
typedef T09Small (__attribute__((preserve_all)) *AllPtr)(T09Small);
typedef T09Large (__attribute__((preserve_all)) *AllVoid)(T09Large);
API_STORAGE T09Small __attribute__((preserve_most)) t09_most(T09Small);
API_STORAGE T09Large __attribute__((preserve_most)) t09_most_void(T09Large);
API_STORAGE T09Small __attribute__((preserve_all)) t09_all_ptr(T09Small);
API_STORAGE T09Large __attribute__((preserve_all)) t09_all(T09Large);
API_STORAGE MostPtr t09_get_most(void);
API_STORAGE MostVoid t09_get_most_void(void);
API_STORAGE AllPtr t09_get_all_ptr(void);
API_STORAGE AllVoid t09_get_all(void);
