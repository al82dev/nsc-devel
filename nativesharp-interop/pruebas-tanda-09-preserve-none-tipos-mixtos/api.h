#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int (__attribute__((preserve_none)) *T09SC)(short);
API_STORAGE int __attribute__((preserve_none)) t09_regcall(short);
API_STORAGE T09SC t09_get_sc(void);
typedef unsigned int (__attribute__((preserve_none)) *T09UC)(unsigned char);
API_STORAGE unsigned int __attribute__((preserve_none)) t09_uc(unsigned char);
API_STORAGE T09UC t09_get_uc(void);
typedef long long (__attribute__((preserve_none)) *T09SH)(int);
API_STORAGE long long __attribute__((preserve_none)) t09_sh(int);
API_STORAGE T09SH t09_get_sh(void);
typedef double (__attribute__((preserve_none)) *T09US)(float);
API_STORAGE double __attribute__((preserve_none)) t09_us(float);
API_STORAGE T09US t09_get_us(void);
