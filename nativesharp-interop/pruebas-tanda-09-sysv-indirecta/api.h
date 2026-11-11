#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int (__attribute__((sysv_abi)) *T09SysvCallback)(int, int, int, int, int, int);
API_STORAGE T09SysvCallback t09_get_sysv_callback(void);
API_STORAGE int t09_normal(int, int, int, int, int, int);
