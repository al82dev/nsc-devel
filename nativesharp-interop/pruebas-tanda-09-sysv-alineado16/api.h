#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(16)) T09SysvAlign16 { float x, y, z, w; } T09SysvAlign16;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign16 sysv_align16(T09SysvAlign16 value);
