#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef short* (__attribute__((regcall)) *T09PtrCallback)(short*, int);
typedef void (__attribute__((regcall)) *T09VoidCallback)(short*, unsigned int);
API_STORAGE short* __attribute__((regcall)) t09_regcall(short*, int);
API_STORAGE T09PtrCallback t09_get_callback(void);
API_STORAGE T09VoidCallback t09_get_void(void);
