#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR7 { unsigned char x, y, z, w, a, b, c; } T09SysvR7;
API_STORAGE __attribute__((sysv_abi)) T09SysvR7 sysv_r7(T09SysvR7 value);
