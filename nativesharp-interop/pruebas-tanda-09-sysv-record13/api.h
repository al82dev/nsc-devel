#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR13 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h, i; } T09SysvR13;
API_STORAGE __attribute__((sysv_abi)) T09SysvR13 sysv_r13(T09SysvR13 value);
