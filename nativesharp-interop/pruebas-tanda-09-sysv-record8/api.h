#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR8 { unsigned char x, y, z, w, a, b, c, d; } T09SysvR8;
API_STORAGE __attribute__((sysv_abi)) T09SysvR8 sysv_r8(T09SysvR8 value);
