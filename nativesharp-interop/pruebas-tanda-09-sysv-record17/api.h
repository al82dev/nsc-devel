#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR17 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i, j, k, l, m; } T09SysvR17;
API_STORAGE __attribute__((sysv_abi)) T09SysvR17 sysv_r17(T09SysvR17 value);
