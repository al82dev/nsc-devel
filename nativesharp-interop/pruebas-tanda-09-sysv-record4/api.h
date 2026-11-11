#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR4 { unsigned char x, y, z, w; } T09SysvR4;
API_STORAGE __attribute__((sysv_abi)) T09SysvR4 sysv_r4(T09SysvR4 value);
