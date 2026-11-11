#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef _Bool (__attribute__((regcall)) *T09RegCallback)(_Bool value);
API_STORAGE _Bool __attribute__((regcall)) t09_regcall(_Bool value);
API_STORAGE T09RegCallback t09_get_callback(void);
