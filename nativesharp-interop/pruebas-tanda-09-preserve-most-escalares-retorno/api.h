#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef float (__attribute__((preserve_most)) *T09SC)(float);
API_STORAGE float __attribute__((preserve_most)) t09_regcall(float);
API_STORAGE T09SC t09_get_sc(void);
typedef unsigned int (__attribute__((preserve_most)) *T09UC)(unsigned int);
API_STORAGE unsigned int __attribute__((preserve_most)) t09_uc(unsigned int);
API_STORAGE T09UC t09_get_uc(void);
typedef unsigned long long (__attribute__((preserve_most)) *T09SH)(unsigned long long);
API_STORAGE unsigned long long __attribute__((preserve_most)) t09_sh(unsigned long long);
API_STORAGE T09SH t09_get_sh(void);
typedef double (__attribute__((preserve_most)) *T09US)(double);
API_STORAGE double __attribute__((preserve_most)) t09_us(double);
API_STORAGE T09US t09_get_us(void);
