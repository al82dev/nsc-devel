#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR3 { unsigned char x, y, z; } T09SysvR3;
API_STORAGE __attribute__((sysv_abi)) T09SysvR3 sysv_r3(T09SysvR3 value);
