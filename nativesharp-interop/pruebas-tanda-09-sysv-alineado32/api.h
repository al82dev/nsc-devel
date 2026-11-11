#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(32)) T09SysvAlign32 { float x, y, z, w; } T09SysvAlign32;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign32 sysv_align32(T09SysvAlign32 value);
