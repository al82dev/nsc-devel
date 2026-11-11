#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR9 { unsigned char x, y, z, w, a, b, c, d, e; } T09SysvR9;
API_STORAGE __attribute__((sysv_abi)) T09SysvR9 sysv_r9(T09SysvR9 value);
