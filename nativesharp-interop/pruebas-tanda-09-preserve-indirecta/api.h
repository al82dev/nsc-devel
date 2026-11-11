#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int (__attribute__((preserve_most)) *T09MostCallback)(int,int,int);
typedef int (__attribute__((preserve_all)) *T09AllCallback)(int,int,int);
API_STORAGE int __attribute__((preserve_most)) t09_most(int a,int b,int c);
API_STORAGE int __attribute__((preserve_all)) t09_all(int a,int b,int c);
API_STORAGE T09MostCallback t09_get_most(void);
API_STORAGE T09AllCallback t09_get_all(void);
