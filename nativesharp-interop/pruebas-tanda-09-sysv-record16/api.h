#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR16 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l; } T09SysvR16;
API_STORAGE __attribute__((sysv_abi)) T09SysvR16 sysv_r16(T09SysvR16 value);
