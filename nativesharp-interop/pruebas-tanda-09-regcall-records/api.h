#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Small { int x,y; } T09Small;
typedef struct T09Large { int a,b,c,d,e,f; } T09Large;
typedef T09Small (__attribute__((regcall)) *MostPtr)(T09Small);
typedef T09Large (__attribute__((regcall)) *MostVoid)(T09Large);
typedef T09Small (__attribute__((regcall)) *AllPtr)(T09Small);
typedef T09Large (__attribute__((regcall)) *AllVoid)(T09Large);
API_STORAGE T09Small __attribute__((regcall)) t09_most(T09Small);
API_STORAGE T09Large __attribute__((regcall)) t09_most_void(T09Large);
API_STORAGE T09Small __attribute__((regcall)) t09_all_ptr(T09Small);
API_STORAGE T09Large __attribute__((regcall)) t09_all(T09Large);
API_STORAGE MostPtr t09_get_most(void);
API_STORAGE MostVoid t09_get_most_void(void);
API_STORAGE AllPtr t09_get_all_ptr(void);
API_STORAGE AllVoid t09_get_all(void);
