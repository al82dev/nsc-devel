#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR2 { unsigned char x, y; } T09SysvR2;
API_STORAGE __attribute__((sysv_abi)) T09SysvR2 sysv_r2(T09SysvR2 value);
