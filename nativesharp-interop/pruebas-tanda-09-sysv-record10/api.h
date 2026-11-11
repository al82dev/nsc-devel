#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR10 { unsigned char x, y, z, w, a, b, c, d, e, f; } T09SysvR10;
API_STORAGE __attribute__((sysv_abi)) T09SysvR10 sysv_r10(T09SysvR10 value);
