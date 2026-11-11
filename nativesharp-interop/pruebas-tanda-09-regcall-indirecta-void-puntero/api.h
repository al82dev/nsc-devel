#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef int* (__attribute__((regcall)) *T09PtrCallback)(int*, int);
typedef void (__attribute__((regcall)) *T09VoidCallback)(int*, unsigned int);
API_STORAGE T09PtrCallback t09_get_callback(void);
API_STORAGE T09VoidCallback t09_get_void(void);
