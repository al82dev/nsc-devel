#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef signed char (__attribute__((preserve_none)) *T09SC)(signed char);
typedef unsigned char (__attribute__((preserve_none)) *T09UC)(unsigned char);
typedef short (__attribute__((preserve_none)) *T09SH)(short);
typedef unsigned short (__attribute__((preserve_none)) *T09US)(unsigned short);
API_STORAGE signed char __attribute__((preserve_none)) t09_regcall(signed char);
API_STORAGE unsigned char __attribute__((preserve_none)) t09_uc(unsigned char);
API_STORAGE short __attribute__((preserve_none)) t09_sh(short);
API_STORAGE unsigned short __attribute__((preserve_none)) t09_us(unsigned short);
API_STORAGE T09SC t09_get_sc(void);
API_STORAGE T09UC t09_get_uc(void);
API_STORAGE T09SH t09_get_sh(void);
API_STORAGE T09US t09_get_us(void);
