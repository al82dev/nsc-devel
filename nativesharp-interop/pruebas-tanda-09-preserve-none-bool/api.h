#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef _Bool (__attribute__((preserve_none)) *T09MostCallback)(_Bool);
typedef _Bool (__attribute__((preserve_none)) *T09AllCallback)(_Bool);
API_STORAGE _Bool __attribute__((preserve_none)) t09_most(_Bool);
API_STORAGE _Bool __attribute__((preserve_none)) t09_all(_Bool);
API_STORAGE T09MostCallback t09_get_most(void);
API_STORAGE T09AllCallback t09_get_all(void);
