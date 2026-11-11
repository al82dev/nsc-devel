#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef double (__attribute__((preserve_most)) *T09MostCallback)(double,int*,short,_Bool);
typedef long long (__attribute__((preserve_all)) *T09AllCallback)(long long,unsigned int,float);
API_STORAGE double __attribute__((preserve_most)) t09_most(double,int*,short,_Bool);
API_STORAGE long long __attribute__((preserve_all)) t09_all(long long,unsigned int,float);
API_STORAGE T09MostCallback t09_get_most(void);
API_STORAGE T09AllCallback t09_get_all(void);
