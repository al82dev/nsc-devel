#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR5 { unsigned char x, y, z, w, a; } T09SysvR5;
API_STORAGE __attribute__((sysv_abi)) T09SysvR5 sysv_r5(T09SysvR5 value);
