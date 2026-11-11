#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR11 { unsigned char x, y, z, w, a, b, c, d, e, f, g; } T09SysvR11;
API_STORAGE __attribute__((sysv_abi)) T09SysvR11 sysv_r11(T09SysvR11 value);
