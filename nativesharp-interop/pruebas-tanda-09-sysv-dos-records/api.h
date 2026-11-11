#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvTwo { unsigned char x, y, z, w; } T09SysvTwo;
API_STORAGE __attribute__((sysv_abi)) T09SysvTwo sysv_two(T09SysvTwo first, T09SysvTwo second);
