#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvEight { unsigned char x, y, z, w; } T09SysvEight;
API_STORAGE __attribute__((sysv_abi)) T09SysvEight sysv_eight(T09SysvEight a, T09SysvEight b, T09SysvEight c, T09SysvEight d, T09SysvEight e, T09SysvEight f, T09SysvEight g, T09SysvEight h);
