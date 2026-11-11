#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR12 { unsigned char x, y, z, w, a, b, c, d, e, f, g, h; } T09SysvR12;
API_STORAGE __attribute__((sysv_abi)) T09SysvR12 sysv_r12(T09SysvR12 value);
