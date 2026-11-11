#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR6 { unsigned char x, y, z, w, a, b; } T09SysvR6;
API_STORAGE __attribute__((sysv_abi)) T09SysvR6 sysv_r6(T09SysvR6 value);
