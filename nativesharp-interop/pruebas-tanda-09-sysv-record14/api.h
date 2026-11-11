#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR14 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j; } T09SysvR14;
API_STORAGE __attribute__((sysv_abi)) T09SysvR14 sysv_r14(T09SysvR14 value);
